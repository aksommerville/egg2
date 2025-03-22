#include "eggdev/eggdev_internal.h"
#include "opt/synth/eau.h"

/* Report error, helper for acquiring line number.
 */

#define eaut_error(ctx,string,fmt,...) \
  eggdev_convert_error_at(ctx,eggdev_lineno(ctx->src,(string)-(char*)ctx->src),fmt,##__VA_ARGS__)

/* Helpers for reading at block level.
 */
 
struct eaut_statement {
  const char *head; // Line before the opening bracket.
  int headc;
  const char *body; // Content within brackets, can be empty.
  int bodyc; // Parse (body) as more statements.
};

static int eaut_statement_next(struct eaut_statement *dst,const char *src,int srcc,struct eggdev_convert_context *ctx) {
  dst->headc=dst->bodyc=0;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *line=src+srcp;
    int linec=0;
    while ((srcp<srcc)&&(src[srcp++]!=0x0a)) linec++;
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    if (!linec||(line[0]=='#')) continue;
    dst->head=line;
    dst->headc=linec;
    if (line[linec-1]!='{') return srcp;
    dst->headc--;
    while (dst->headc&&((unsigned char)dst->head[dst->headc-1]<=0x20)) dst->headc--;
    dst->body=src+srcp;
    dst->bodyc=0;
    int depth=1;
    for (;;) {
      if (srcp>=srcc) return eaut_error(ctx,src,"Unclosed block.");
      if (src[srcp]=='{') depth++;
      else if (src[srcp]=='}') {
        if (!--depth) {
          srcp++;
          break;
        }
      }
      srcp++;
      dst->bodyc++;
    }
    while (dst->bodyc&&((unsigned char)dst->body[dst->bodyc-1]<=0x20)) dst->bodyc--;
    while (dst->bodyc&&((unsigned char)dst->body[0]<=0x20)) { dst->body++; dst->bodyc--; }
    return srcp;
  }
  return 0;
}

/* Statement helpers that pop a token and return it to you.
 */
 
static int eaut_statement_next_token(void *dstpp,struct eaut_statement *st) {
  while (st->headc&&((unsigned char)st->head[0]<=0x20)) { st->head++; st->headc--; }
  *(const void**)dstpp=st->head;
  int tokenc=0;
  while (st->headc&&((unsigned char)st->head[0]>0x20)) { st->head++; st->headc--; tokenc++; }
  return tokenc;
}

static int eaut_statement_finished(struct eaut_statement *st,struct eggdev_convert_context *ctx,int forbid_body) {
  while (st->headc&&((unsigned char)st->head[0]<=0x20)) { st->head++; st->headc--; }
  if (st->headc) return eaut_error(ctx,st->head,"Unexpected tokens after statement.");
  if (forbid_body&&st->bodyc) return eaut_error(ctx,st->head,"Bracketted body not permitted after this statement.");
  return 0;
}

static int eaut_statement_int(int *dst,struct eaut_statement *st,struct eggdev_convert_context *ctx,const char *desc) {
  const char *src=0;
  int srcc=eaut_statement_next_token(&src,st);
  if (srcc<1) return eaut_error(ctx,st->head,"Expected integer (%s) before end of line.",desc);
  if (sr_int_eval(dst,src,srcc)<2) return eaut_error(ctx,src,"Failed to parse '%.*s' as integer for %s.",srcc,src,desc);
  return 0;
}

static int eaut_statement_u16(int *dst,struct eaut_statement *st,struct eggdev_convert_context *ctx,const char *desc) {
  const char *src=0;
  int srcc=eaut_statement_next_token(&src,st);
  if (srcc<1) return eaut_error(ctx,st->head,"Expected integer (%s) before end of line.",desc);
  if (sr_int_eval(dst,src,srcc)<2) return eaut_error(ctx,src,"Failed to parse '%.*s' as integer for %s.",srcc,src,desc);
  if ((*dst<0)||(*dst>0xffff)) return eaut_error(ctx,src,"%s must be in 0..65535, found %d.",desc,*dst);
  return 0;
}

static int eaut_statement_u8(int *dst,struct eaut_statement *st,struct eggdev_convert_context *ctx,const char *desc) {
  const char *src=0;
  int srcc=eaut_statement_next_token(&src,st);
  if (srcc<1) return eaut_error(ctx,st->head,"Expected integer (%s) before end of line.",desc);
  if (sr_int_eval(dst,src,srcc)<2) return eaut_error(ctx,src,"Failed to parse '%.*s' as integer for %s.",srcc,src,desc);
  if ((*dst<0)||(*dst>0xff)) return eaut_error(ctx,src,"%s must be in 0..255, found %d.",desc,*dst);
  return 0;
}

/* Statement helpers that produce output directly.
 */

static int eaut_statement_hexdump(struct eaut_statement *st,struct eggdev_convert_context *ctx) {
  while (st->headc>0) {
    if ((unsigned char)st->head[0]<=0x20) { st->head++; st->headc--; }
    if (st->headc<2) return eaut_error(ctx,st->head,"Malformed hex dump, uneven length.");
    int hi=sr_digit_eval(st->head[0]);
    int lo=sr_digit_eval(st->head[1]);
    if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) {
      return eaut_error(ctx,st->head,"Malformed hex dump, invalid byte '%.2s'.",st->head);
    }
    st->head+=2;
    st->headc-=2;
    if (sr_encode_u8(ctx->dst,(hi<<4)|lo)<0) return -1;
  }
  return 0;
}

static int eaut_scalar_u0_8(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  const char *src;
  int srcc=eaut_statement_next_token(&src,st);
  if (srcc<1) return eaut_error(ctx,st->head,"Expected u0.8 for %s",name);
  double vf;
  if ((sr_double_eval(&vf,src,srcc)<0)||(vf<0.0)||(vf>1.0)) {
    return eaut_error(ctx,src,"Expected float in 0..1 for %s, found '%.*s'",name,srcc,src);
  }
  if (sr_encode_u8(ctx->dst,(int)(vf*255.0f))<0) return -1;
  return 0;
}

static int eaut_scalar_u8_8(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  const char *src;
  int srcc=eaut_statement_next_token(&src,st);
  if (srcc<1) return eaut_error(ctx,st->head,"Expected u8.8 for %s",name);
  double vf;
  if ((sr_double_eval(&vf,src,srcc)<0)||(vf<0.0)||(vf>256.0)) {
    return eaut_error(ctx,src,"Expected float in 0..256 for %s, found '%.*s'",name,srcc,src);
  }
  int vi=(int)(vf*256.0);
  if (vi>0xffff) vi=0xffff;
  if (sr_encode_intbe(ctx->dst,vi,2)<0) return -1;
  return 0;
}

static int eaut_scalar_u16(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  int v,err;
  if ((err=eaut_statement_u16(&v,st,ctx,name))<0) return err;
  if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  return 0;
}

static int eaut_scalar_u8(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  int v,err;
  if ((err=eaut_statement_u8(&v,st,ctx,name))<0) return err;
  if (sr_encode_u8(ctx->dst,v)<0) return -1;
  return 0;
}

/* Compile envelope.
 */
 
static int eaut_env(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  #define POINT_LIMIT 16 /* This is imposed by synth too. */
  struct env_point {
    int tlo,vlo,thi,vhi,sus;
  } pointv[POINT_LIMIT]={0};
  int pointc=0;
  int initlo=0,inithi=0;
  const char *token;
  int tokenc,err;
  
  #define DECODE_RANGE(lo,hi,sus) { \
    if ((tokenc>0)&&(token[tokenc-1]=='*')) { \
      if (!sus) return eaut_error(ctx,token,"Sustain not allowed here: '%.*s'",tokenc,token); \
      *(int*)(sus)=1; \
      tokenc--; \
    } \
    int i=0,gotrange=0; for (;i<tokenc;i++) { \
      if (token[i]=='.') { \
        if ((i>tokenc-2)||(token[i+1]!='.')) return eaut_error(ctx,token,"Malformed env token '%.*s'",tokenc,token); \
        if ((sr_int_eval(lo,token,i)<2)||(*(lo)<0)||(*(lo)>0xffff)) return eaut_error(ctx,token,"Expected 0..65535, found '%.*s'",i,token); \
        if ((sr_int_eval(hi,token+i+2,tokenc-i-2)<2)||(*(hi)<0)||(*(hi)>0xffff)) return eaut_error(ctx,token,"Expected 0..65535, found '%.*s'",tokenc-i-2,token+i+2); \
        gotrange=1; \
        break; \
      } \
    } \
    if (!gotrange) { \
      if ((sr_int_eval(lo,token,tokenc)<2)||(*(lo)<0)||(*(lo)>0xffff)) return eaut_error(ctx,token,"Expected 0..65535, found '%.*s'",tokenc,token); \
      *(hi)=*(lo); \
    } \
  }
  
  // First token is init levels and it's mandatory.
  if ((tokenc=eaut_statement_next_token(&token,st))<1) return eaut_error(ctx,st->head,"Expected init levels.");
  DECODE_RANGE(&initlo,&inithi,0)
  
  // Followed by zero or more of (time,level).
  for (;;) {
    if ((tokenc=eaut_statement_next_token(&token,st))<1) break;
    if (pointc>=POINT_LIMIT) return eaut_error(ctx,token,"Too many points in envelope. Limit %d without sustain or %d with.",POINT_LIMIT,POINT_LIMIT-1);
    struct env_point *point=pointv+pointc++;
    DECODE_RANGE(&point->tlo,&point->thi,0)
    if ((tokenc=eaut_statement_next_token(&token,st))<1) return eaut_error(ctx,st->head,"Expected levels.");
    DECODE_RANGE(&point->vlo,&point->vhi,&point->sus)
  }
  
  // Determine which features are in play.
  uint8_t flags=0;
  int susp;
  if (initlo!=inithi) flags|=0x01;
  else {
    int i=pointc; while (i-->0) {
      if (pointv[i].tlo!=pointv[i].thi) { flags|=0x01; break; }
      if (pointv[i].vlo!=pointv[i].vhi) { flags|=0x01; break; }
    }
  }
  if (initlo||inithi) flags|=0x02;
  {
    int i=pointc; while (i-->0) {
      if (pointv[i].sus) {
        if (flags&0x04) return eaut_error(ctx,token,"Multiple sustain points.");
        flags|=0x04;
        susp=i;
      }
    }
  }
  if (susp&&(pointc>=POINT_LIMIT)) return eaut_error(ctx,st->head,"Sustaining envelope limited to %d points.",POINT_LIMIT-1);
  
  // Emit.
  if (sr_encode_u8(ctx->dst,flags)<0) return -1;
  if (flags&0x02) {
    if (sr_encode_intbe(ctx->dst,initlo,2)<0) return -1;
    if (flags&0x01) {
      if (sr_encode_intbe(ctx->dst,inithi,2)<0) return -1;
    }
  }
  if (flags&0x04) {
    if (sr_encode_u8(ctx->dst,susp)<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,pointc)<0) return -1;
  const struct env_point *point=pointv;
  int i=pointc;
  for (;i-->0;point++) {
    if (sr_encode_intbe(ctx->dst,point->tlo,2)<0) return -1;
    if (sr_encode_intbe(ctx->dst,point->vlo,2)<0) return -1;
    if (flags&0x01) {
      if (sr_encode_intbe(ctx->dst,point->thi,2)<0) return -1;
      if (sr_encode_intbe(ctx->dst,point->vhi,2)<0) return -1;
    }
  }
  
  #undef DECODE_RANGE
  #undef POINT_LIMIT
  return 0;
}

/* Compile wave.
 */
 
static int eaut_wave(struct eggdev_convert_context *ctx,struct eaut_statement *st,const char *name) {
  int err;
  
  // Start with a placeholder.
  int dstc0=ctx->dst->c;
  if ((sr_encode_raw(ctx->dst,"\0\0\0",3))<0) return -1;
  
  // Shape is from an enum, or 0..255.
  int shape;
  const char *shapestr;
  int shapec=eaut_statement_next_token(&shapestr,st);
  if ((shapec==4)&&!memcmp(shapestr,"sine",4)) shape=EAU_SHAPE_SINE;
  else if ((shapec==6)&&!memcmp(shapestr,"square",6)) shape=EAU_SHAPE_SQUARE;
  else if ((shapec==3)&&!memcmp(shapestr,"saw",3)) shape=EAU_SHAPE_SAW;
  else if ((shapec==8)&&!memcmp(shapestr,"triangle",8)) shape=EAU_SHAPE_TRIANGLE;
  else if ((shapec==7)&&!memcmp(shapestr,"fixedfm",7)) shape=EAU_SHAPE_FIXEDFM;
  else if ((sr_int_eval(&shape,shapestr,shapec)>=2)&&(shape>=0)&&(shape<=0xff)) ;
  else return eaut_error(ctx,st->head,"Expected shape name or 0..255, found '%.*s'",shapec,shapestr);
  ((uint8_t*)ctx->dst->v)[dstc0]=shape;
  
  for (;;) {
    const char *token;
    int tokenc=eaut_statement_next_token(&token,st);
    if (tokenc<1) break;
    
    if (token[0]=='+') {
      if (((uint8_t*)ctx->dst->v)[dstc0+1]) return eaut_error(ctx,token,"Multiple wave qualifiers.");
      if (((uint8_t*)ctx->dst->v)[dstc0+2]) return eaut_error(ctx,token,"Wave qualifier must come before harmonics.");
      int qual;
      if ((sr_int_eval(&qual,token+1,tokenc-1)<2)||(qual<0)||(qual>0xff)) {
        return eaut_error(ctx,token,"Expected +0..255 for wave qualifier, found '%.*s'",tokenc,token);
      }
      ((uint8_t*)ctx->dst->v)[dstc0+1]=qual;
      continue;
    }
    
    int harm;
    if ((sr_int_eval(&harm,token,tokenc)<2)||(harm<0)||(harm>0xffff)) {
      return eaut_error(ctx,token,"Expected wave harmonic coefficient in 0..65535, found '%.*s'",tokenc,token);
    }
    if (((uint8_t*)ctx->dst->v)[dstc0+2]>=0xff) return eaut_error(ctx,token,"Too many harmonics.");
    if (sr_encode_intbe(ctx->dst,harm,2)<0) return -1;
    ((uint8_t*)ctx->dst->v)[dstc0+2]++;
  }
  
  return 0;
}

/* Globals.
 */
 
static int eggdev_eau_from_eaut_default_globals(struct eggdev_convert_context *ctx) {
  return sr_encode_raw(ctx->dst,"\0EAU\x01\xf4\0\0",8);
}

static int eggdev_eau_from_eaut_globals(struct eggdev_convert_context *ctx,const char *body,int bodyc) {
  int tempo=500;
  int bodyp=0,err;
  struct eaut_statement st;
  while (bodyp<bodyc) {
    if ((err=eaut_statement_next(&st,body+bodyp,bodyc-bodyp,ctx))<0) return err;
    if (!err) break;
    bodyp+=err;
    const char *kw;
    int kwc=eaut_statement_next_token(&kw,&st);
    if (!kwc) continue;
    
    if ((kwc==5)&&!memcmp(kw,"tempo",5)) {
      if ((err=eaut_statement_u16(&tempo,&st,ctx,"ms/qnote"))<0) return err;
      if ((err=eaut_statement_finished(&st,ctx,1))<0) return err;
      continue;
    }
    
    return eaut_error(ctx,kw,"Unexpected keyword '%.*s' in 'globals' block.",kwc,kw);
  }
  if (sr_encode_raw(ctx->dst,"\0EAU",4)<0) return -1;
  if (sr_encode_intbe(ctx->dst,tempo,2)<0) return -1;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  return 0;
}

/* Events.
 */
 
static int eggdev_eau_from_eaut_events(struct eggdev_convert_context *ctx,const char *body,int bodyc) {
  int bodyp=0,err;
  while (bodyp<bodyc) {
    struct eaut_statement st;
    if ((err=eaut_statement_next(&st,body+bodyp,bodyc-bodyp,ctx))<0) return err;
    if (!err) break;
    bodyp+=err;
    const char *kw;
    int kwc=eaut_statement_next_token(&kw,&st);
    if (!kwc) continue;
    
    if ((kwc==5)&&!memcmp(kw,"delay",5)) {
      int ms;
      if ((err=eaut_statement_int(&ms,&st,ctx,"ms"))<0) return err;
      if (ms<1) return eaut_error(ctx,st.head,"Delay must be greater than zero.");
      if ((err=eaut_statement_finished(&st,ctx,1))<0) return err;
      while (ms>=2048) {
        if (sr_encode_u8(ctx->dst,0x8f)<0) return -1;
        ms-=2048;
      }
      if (ms>=128) {
        if (sr_encode_u8(ctx->dst,0x80|((ms>>7)-1))<0) return -1;
        ms&=0x7f;
      }
      if (ms>0) {
        if (sr_encode_u8(ctx->dst,ms)<0) return -1;
      }
      continue;
    }
    
    if ((kwc==4)&&!memcmp(kw,"note",4)) {
      int chid,noteid,velocity,duration;
      if ((err=eaut_statement_int(&chid,&st,ctx,"chid"))<0) return err;
      if ((err=eaut_statement_int(&noteid,&st,ctx,"noteid"))<0) return err;
      if ((err=eaut_statement_int(&velocity,&st,ctx,"velocity"))<0) return err;
      if ((err=eaut_statement_int(&duration,&st,ctx,"duration"))<0) return err;
      if ((err=eaut_statement_finished(&st,ctx,1))<0) return err;
      if ((chid<0)||(chid>15)) return eaut_error(ctx,kw,"chid must be in 0..15, found %d",chid);
      if ((noteid<0)||(noteid>0x7f)) return eaut_error(ctx,kw,"noteid must be in 0..127, found %d",noteid);
      if ((velocity<0)||(velocity>15)) return eaut_error(ctx,kw,"velocity must be in 0..15, found %d",velocity);
      if ((duration<0)||(duration>32767)) return eaut_error(ctx,kw,"duration must be in 0..32767, found %d",duration);
      if (duration>=1024) {
        if (sr_encode_u8(ctx->dst,0xb0|chid)<0) return -1;
        duration=(duration>>10)-1;
      } else if (duration>=32) {
        if (sr_encode_u8(ctx->dst,0xa0|chid)<0) return -1;
        duration=(duration>>5)-1;
      } else {
        if (sr_encode_u8(ctx->dst,0x90|chid)<0) return -1;
      }
      if (sr_encode_u8(ctx->dst,(noteid<<1)|(velocity>>3))<0) return -1;
      if (sr_encode_u8(ctx->dst,(velocity<<5)|duration)<0) return -1;
      continue;
    }
    
    if ((kwc==5)&&!memcmp(kw,"wheel",5)) {
      int chid,v;
      if ((err=eaut_statement_int(&chid,&st,ctx,"chid"))<0) return err;
      if ((err=eaut_statement_u8(&v,&st,ctx,"wheel"))<0) return err;
      if ((err=eaut_statement_finished(&st,ctx,1))<0) return err;
      if ((chid<0)||(chid>15)) return eaut_error(ctx,kw,"chid must be in 0..15, found %d",chid);
      if (sr_encode_u8(ctx->dst,0xc0|chid)<0) return -1;
      if (sr_encode_u8(ctx->dst,v)<0) return -1;
      continue;
    }
    
    return eaut_error(ctx,kw,"Unexpected keyword '%.*s' in 'events' block.",kwc,kw);
  }
  return 0;
}

/* Field in DRUM channel header.
 */
 
static int eggdev_eau_from_eaut_chhdr_drum(struct eggdev_convert_context *ctx,const char *kw,int kwc,struct eaut_statement *st,int fieldp) {
  int err;
  if ((kwc==4)&&!memcmp(kw,"note",4)) {
    int noteid,trimlo=0x80,trimhi=0xff,pan=0x80;
    if ((err=eaut_statement_u8(&noteid,st,ctx,"noteid"))<0) return err;
    if (noteid>0x7f) return eaut_error(ctx,kw,"noteid must be in 0..127, found %d",noteid);
    const char *token;
    int tokenc;
    if ((tokenc=eaut_statement_next_token(&token,st))>0) {
      if ((sr_int_eval(&trimlo,token,tokenc)<2)||(trimlo<0)||(trimlo>0xff)) {
        return eaut_error(ctx,token,"Expected trim low in 0..255, found '%.*s'",tokenc,token);
      }
    }
    if ((tokenc=eaut_statement_next_token(&token,st))>0) {
      if ((sr_int_eval(&trimhi,token,tokenc)<2)||(trimhi<0)||(trimhi>0xff)) {
        return eaut_error(ctx,token,"Expected trim high in 0..255, found '%.*s'",tokenc,token);
      }
    }
    if ((tokenc=eaut_statement_next_token(&token,st))>0) {
      if ((sr_int_eval(&pan,token,tokenc)<2)||(pan<0)||(pan>0xff)) {
        return eaut_error(ctx,token,"Expected pan in 0..255, found '%.*s'",tokenc,token);
      }
    }
    if ((err=eaut_statement_finished(st,ctx,0))<0) return err;
    if (sr_encode_u8(ctx->dst,noteid)<0) return -1;
    if (sr_encode_u8(ctx->dst,trimlo)<0) return -1;
    if (sr_encode_u8(ctx->dst,trimhi)<0) return -1;
    if (sr_encode_u8(ctx->dst,pan)<0) return -1;
    int lenp=ctx->dst->c;
    if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
    struct eggdev_convert_context subctx=*ctx;
    subctx.src=st->body;
    subctx.srcc=st->bodyc;
    subctx.lineno0=ctx->lineno0+eggdev_lineno(ctx->src,st->head-(char*)ctx->src);
    if ((err=eggdev_eau_from_eaut(&subctx))<0) return err;
    int len=ctx->dst->c-lenp-2;
    if ((len<0)||(len>0xffff)) return eaut_error(ctx,kw,"Invalid drum body length %d, must be 0..65535",len);
    ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
    ((uint8_t*)ctx->dst->v)[lenp+1]=len;
    return 0;
  }
  return eaut_error(ctx,kw,"Unexpected command '%.*s' in drum channel.",kwc,kw);
}

/* Field in FM channel header.
 */
 
static int eggdev_eau_from_eaut_chhdr_fm(struct eggdev_convert_context *ctx,const char *kw,int kwc,struct eaut_statement *st,int fieldp) {
  int err;
  switch (fieldp) {
    #define SEQCMD(p,name,evalfn) case p: { \
      if ((kwc!=sizeof(name)-1)||memcmp(kw,name,kwc)) return eaut_error(ctx,kw, \
        "Expected '%s', found '%.*s'. Channel config fields can not be skipped or reordered.",name,kwc,kw \
      ); \
      if ((err=evalfn(ctx,st,name))<0) return err; \
      if ((err=eaut_statement_finished(st,ctx,1))<0) return err; \
    } return 0;
    SEQCMD(0,"level",eaut_env)
    SEQCMD(1,"wave",eaut_wave)
    SEQCMD(2,"pitchenv",eaut_env)
    SEQCMD(3,"wheel",eaut_scalar_u16)
    SEQCMD(4,"rate",eaut_scalar_u8_8)
    SEQCMD(5,"range",eaut_scalar_u8_8)
    SEQCMD(6,"rangeenv",eaut_env)
    SEQCMD(7,"rangelforate",eaut_scalar_u8_8)
    SEQCMD(8,"rangelfodepth",eaut_scalar_u0_8)
    #undef SEQCMD
  }
  return eaut_error(ctx,kw,"Unexpected command '%.*s' in fm channel header.",kwc,kw);
}

/* Field in SUB channel header.
 */
 
static int eggdev_eau_from_eaut_chhdr_sub(struct eggdev_convert_context *ctx,const char *kw,int kwc,struct eaut_statement *st,int fieldp) {
  int err;
  switch (fieldp) {
    #define SEQCMD(p,name,evalfn) case p: { \
      if ((kwc!=sizeof(name)-1)||memcmp(kw,name,kwc)) return eaut_error(ctx,kw, \
        "Expected '%s', found '%.*s'. Channel config fields can not be skipped or reordered.",name,kwc,kw \
      ); \
      if ((err=evalfn(ctx,st,name))<0) return err; \
      if ((err=eaut_statement_finished(st,ctx,1))<0) return err; \
    } return 0;
    SEQCMD(0,"level",eaut_env)
    SEQCMD(1,"width",eaut_scalar_u16)
    SEQCMD(2,"stagec",eaut_scalar_u8)
    SEQCMD(3,"gain",eaut_scalar_u8_8)
    #undef SEQCMD
  }
  return eaut_error(ctx,kw,"Unexpected command '%.*s' in sub channel header.",kwc,kw);
}

/* Post: gain
 */
 
static int eggdev_eau_from_eaut_post_gain(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_GAIN)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,2)<0) return -1;
  if ((err=eaut_scalar_u8_8(ctx,st,"gain"))<0) return err;
  const char *gatestr;
  int gatec=eaut_statement_next_token(&gatestr,st);
  if (gatec>0) {
    double gate;
    if ((sr_double_eval(&gate,gatestr,gatec)<0)||(gate<0.0)||(gate>1.0)) {
      return eaut_error(ctx,gatestr,"Expected float in 0..1 for gate, found '%.*s'",gatec,gatestr);
    }
    if (sr_encode_u8(ctx->dst,(int)(gate*255.0))<0) return -1;
    ((uint8_t*)ctx->dst->v)[lenp]=3;
  }
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}

/* Post: delay
 */
 
static int eggdev_eau_from_eaut_post_delay(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_DELAY)<0) return -1;
  if (sr_encode_u8(ctx->dst,6)<0) return -1;
  if ((err=eaut_scalar_u8_8(ctx,st,"period(qnotes)"))<0) return err;
  if ((err=eaut_scalar_u0_8(ctx,st,"dry"))<0) return err;
  if ((err=eaut_scalar_u0_8(ctx,st,"wet"))<0) return err;
  if ((err=eaut_scalar_u0_8(ctx,st,"store"))<0) return err;
  if ((err=eaut_scalar_u0_8(ctx,st,"feedback"))<0) return err;
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}

/* Post: filters
 */
 
static int eggdev_eau_from_eaut_post_lopass(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_LOPASS)<0) return -1;
  if (sr_encode_u8(ctx->dst,2)<0) return -1;
  if ((err=eaut_scalar_u16(ctx,st,"freq"))<0) return err;
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}
 
static int eggdev_eau_from_eaut_post_hipass(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_HIPASS)<0) return -1;
  if (sr_encode_u8(ctx->dst,2)<0) return -1;
  if ((err=eaut_scalar_u16(ctx,st,"freq"))<0) return err;
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}
 
static int eggdev_eau_from_eaut_post_bpass(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_BPASS)<0) return -1;
  if (sr_encode_u8(ctx->dst,4)<0) return -1;
  if ((err=eaut_scalar_u16(ctx,st,"freq"))<0) return err;
  if ((err=eaut_scalar_u16(ctx,st,"width"))<0) return err;
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}
 
static int eggdev_eau_from_eaut_post_notch(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_NOTCH)<0) return -1;
  if (sr_encode_u8(ctx->dst,4)<0) return -1;
  if ((err=eaut_scalar_u16(ctx,st,"freq"))<0) return err;
  if ((err=eaut_scalar_u16(ctx,st,"width"))<0) return err;
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}

/* Post: waveshaper
 */
 
static int eggdev_eau_from_eaut_post_waveshaper(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  if (sr_encode_u8(ctx->dst,EAU_STAGEID_WAVESHAPER)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,0)<0) return -1;
  for (;;) {
    const char *token;
    int tokenc=eaut_statement_next_token(&token,st);
    if (tokenc<1) break;
    int v;
    if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>0xffff)) {
      return eaut_error(ctx,token,"Expected integer in 0..65535, found '%.*s'",tokenc,token);
    }
    if (((uint8_t*)ctx->dst->v)[lenp]>0xfd) return eaut_error(ctx,token,"Too many waveshaper control points.");
    if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
    ((uint8_t*)ctx->dst->v)[lenp]+=2;
  }
  if ((err=eaut_statement_finished(st,ctx,1))<0) return err;
  return 0;
}

/* Generic post field.
 */
 
static int eggdev_eau_from_eaut_post_generic(struct eggdev_convert_context *ctx,const char *kw,int kwc,struct eaut_statement *st) {
  int stageid,err;
  if ((sr_int_eval(&stageid,kw,kwc)<2)||(stageid<0)||(stageid>0xff)) {
    return eaut_error(ctx,kw,"Expected post stage name or 0..255, found '%.*s'",kwc,kw);
  }
  if (sr_encode_u8(ctx->dst,stageid)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,0)<0) return -1;
  if ((err=eaut_statement_hexdump(st,ctx))<0) return err;
  int len=ctx->dst->c-lenp-1;
  if (len>0xff) return eaut_error(ctx,kw,"Post stage body length exceeds limit (%d>255)",len);
  ((uint8_t*)ctx->dst->v)[lenp]=len;
  return 0;
}

/* Post body.
 * We do not emit the length, caller must.
 */
 
static int eggdev_eau_from_eaut_post(struct eggdev_convert_context *ctx,const char *src,int srcc) {
  int srcp=0,err;
  while (srcp<srcc) {
    struct eaut_statement st;
    if ((err=eaut_statement_next(&st,src+srcp,srcc-srcp,ctx))<0) return err;
    if (!err) break;
    srcp+=err;
    const char *kw;
    int kwc=eaut_statement_next_token(&kw,&st);
    if (!kwc) continue;
    
    if ((kwc==4)&&!memcmp(kw,"gain",4)) err=eggdev_eau_from_eaut_post_gain(ctx,&st);
    else if ((kwc==5)&&!memcmp(kw,"delay",5)) err=eggdev_eau_from_eaut_post_delay(ctx,&st);
    else if ((kwc==6)&&!memcmp(kw,"lopass",6)) err=eggdev_eau_from_eaut_post_lopass(ctx,&st);
    else if ((kwc==6)&&!memcmp(kw,"hipass",6)) err=eggdev_eau_from_eaut_post_hipass(ctx,&st);
    else if ((kwc==5)&&!memcmp(kw,"bpass",5)) err=eggdev_eau_from_eaut_post_bpass(ctx,&st);
    else if ((kwc==5)&&!memcmp(kw,"notch",5)) err=eggdev_eau_from_eaut_post_notch(ctx,&st);
    else err=eggdev_eau_from_eaut_post_generic(ctx,kw,kwc,&st);
    if (err<0) return err;
  }
  return 0;
}

/* Channel Header innards: Mode config and post.
 * Caller emits thru (mode), we emit (payloadc,payload,postc,post).
 */
 
static int eggdev_eau_from_eaut_chhdr_body(struct eggdev_convert_context *ctx,const char *src,int srcc,uint8_t mode) {
  int paylenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  int gotpost=0,gotmodecfg=0,fieldp=0;
  int srcp=0,err;
  while (srcp<srcc) {
    struct eaut_statement st;
    if ((err=eaut_statement_next(&st,src+srcp,srcc-srcp,ctx))<0) return err;
    if (!err) break;
    srcp+=err;
    const char *kw;
    int kwc=eaut_statement_next_token(&kw,&st);
    if (!kwc) continue;
    
    if ((kwc==4)&&!memcmp(kw,"post",4)) {
      if ((err=eaut_statement_finished(&st,ctx,0))<0) return err;
      if (gotpost) return eaut_error(ctx,kw,"Multiple post in channel header.");
      int paylen=ctx->dst->c-paylenp-2;
      if ((paylen<0)||(paylen>0xffff)) return eaut_error(ctx,kw,"Invalid channel payload length %d",paylen);
      ((uint8_t*)ctx->dst->v)[paylenp]=paylen>>8;
      ((uint8_t*)ctx->dst->v)[paylenp+1]=paylen;
      int postlenp=ctx->dst->c;
      if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
      if ((err=eggdev_eau_from_eaut_post(ctx,st.body,st.bodyc))<0) return err;
      int postlen=ctx->dst->c-postlenp-2;
      if ((postlen<0)||(postlen>0xffff)) return eaut_error(ctx,kw,"Invalid channel post length %d",postlen);
      ((uint8_t*)ctx->dst->v)[postlenp]=postlen>>8;
      ((uint8_t*)ctx->dst->v)[postlenp+1]=postlen;
      gotpost=1;
      continue;
    }
    if (gotpost) return eaut_error(ctx,kw,"Channel header statement '%.*s' not permitted after 'post'",kwc,kw);
    
    if ((kwc==7)&&!memcmp(kw,"modecfg",7)) {
      if (fieldp) return eaut_error(ctx,kw,"'modecfg' may not be used with other channel payload fields");
      if (gotmodecfg) return eaut_error(ctx,kw,"Multiple 'modecfg'");
      if ((err=eaut_statement_hexdump(&st,ctx))<0) return err;
      gotmodecfg=1;
      continue;
    }
    
    switch (mode) {
      case EAU_CHANNEL_MODE_DRUM: err=eggdev_eau_from_eaut_chhdr_drum(ctx,kw,kwc,&st,fieldp); break;
      case EAU_CHANNEL_MODE_FM: err=eggdev_eau_from_eaut_chhdr_fm(ctx,kw,kwc,&st,fieldp); break;
      case EAU_CHANNEL_MODE_SUB: err=eggdev_eau_from_eaut_chhdr_sub(ctx,kw,kwc,&st,fieldp); break;
      default: return eaut_error(ctx,kw,"Invalid command for unknown channel header type. Only 'post' and 'modecfg' are permitted.");
    }
    if (err<0) return err;
    fieldp++;
  }
  if (!gotpost) {
    int paylen=ctx->dst->c-paylenp-2;
    if ((paylen<0)||(paylen>0xffff)) return eaut_error(ctx,src,"Invalid channel payload length %d",paylen);
    ((uint8_t*)ctx->dst->v)[paylenp]=paylen>>8;
    ((uint8_t*)ctx->dst->v)[paylenp+1]=paylen;
    if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  }
  return 0;
}

/* Channel Header.
 */
 
static int eggdev_eau_from_eaut_chhdr(struct eggdev_convert_context *ctx,struct eaut_statement *st) {
  int err;
  
  int chid,trim,pan,modec,mode;
  const char *modestr;
  if ((err=eaut_statement_u8(&chid,st,ctx,"chid"))<0) return err;
  if ((err=eaut_statement_u8(&trim,st,ctx,"trim"))<0) return err;
  if ((err=eaut_statement_u8(&pan,st,ctx,"pan"))<0) return err;
  if ((modec=eaut_statement_next_token(&modestr,st))<1) return eaut_error(ctx,st->head,"Expected channel mode (fm,sub,drum).");
  if ((err=eaut_statement_finished(st,ctx,0))<0) return err;
  if (chid==0xff) return eaut_error(ctx,st->head,"Channel 255 is illegal."); // 16..254 are permitted but not addressable. We do use those for the default instruments.
  
  if ((modec==4)&&!memcmp(modestr,"noop",4)) mode=EAU_CHANNEL_MODE_NOOP;
  else if ((modec==4)&&!memcmp(modestr,"drum",4)) mode=EAU_CHANNEL_MODE_DRUM;
  else if ((modec==2)&&!memcmp(modestr,"fm",2)) mode=EAU_CHANNEL_MODE_FM;
  else if ((modec==3)&&!memcmp(modestr,"sub",3)) mode=EAU_CHANNEL_MODE_SUB;
  else if ((sr_int_eval(&mode,modestr,modec)>=2)&&(mode>=0)&&(mode<0x100)) ;
  else return eaut_error(ctx,st->head,"Unknown channel mode '%.*s'",modec,modestr);
  
  if (sr_encode_u8(ctx->dst,chid)<0) return -1;
  if (sr_encode_u8(ctx->dst,trim)<0) return -1;
  if (sr_encode_u8(ctx->dst,pan)<0) return -1;
  if (sr_encode_u8(ctx->dst,mode)<0) return -1;
  
  if ((err=eggdev_eau_from_eaut_chhdr_body(ctx,st->body,st->bodyc,mode))<0) return err;
  
  return 0;
}

/* EAU bin from text.
 */
 
int eggdev_eau_from_eaut(struct eggdev_convert_context *ctx) {
  int srcp=0,err;
  int globalsc=0,eventsc=0,chhdrc=0;
  int dstc0=ctx->dst->c,dstc_events=0;
  struct eaut_statement st;
  while (srcp<ctx->srcc) {
    if ((err=eaut_statement_next(&st,ctx->src+srcp,ctx->srcc-srcp,ctx))<0) return err;
    if (!err) break;
    srcp+=err;
    
    if ((st.headc==7)&&!memcmp(st.head,"globals",7)) {
      if (globalsc) return eaut_error(ctx,st.head,"Multiple 'globals' blocks.");
      if (eventsc||chhdrc) return eaut_error(ctx,st.head,"'globals' may only be the first block.");
      if ((err=eggdev_eau_from_eaut_globals(ctx,st.body,st.bodyc))<0) return err;
      globalsc++;
      
    } else if ((st.headc==6)&&!memcmp(st.head,"events",6)) {
      if (!eventsc) {
        if (!globalsc&&!chhdrc) {
          if ((err=eggdev_eau_from_eaut_default_globals(ctx))<0) return err;
        }
        if (chhdrc) {
          if (sr_encode_u8(ctx->dst,0xff)<0) return -1; // channel headers terminator
        }
        dstc_events=ctx->dst->c;
      } else if (eventsc==1) {
        int loopp=ctx->dst->c-dstc_events;
        if ((loopp<0)||(loopp>0xffff)) return eaut_error(ctx,st.head,"Invalid loop position %d",loopp);
        ((uint8_t*)ctx->dst->v)[dstc0+6]=loopp>>8;
        ((uint8_t*)ctx->dst->v)[dstc0+7]=loopp;
      } else {
        return eaut_error(ctx,st.head,"'events' may not appear more than twice.");
      }
      eventsc++;
      if ((err=eggdev_eau_from_eaut_events(ctx,st.body,st.bodyc))<0) return err;
      
    } else {
      if (eventsc) return eaut_error(ctx,st.head,"Channel header may not appear after 'events'.");
      if (!globalsc&&!chhdrc) {
        if ((err=eggdev_eau_from_eaut_default_globals(ctx))<0) return err;
      }
      chhdrc++;
      if ((err=eggdev_eau_from_eaut_chhdr(ctx,&st))<0) return err;
    }
  }
  if (chhdrc&&!eventsc) {
    if (sr_encode_u8(ctx->dst,0xff)<0) return -1; // channel headers terminator
  }
  return 0;
}
