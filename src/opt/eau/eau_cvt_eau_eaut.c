#include "eau.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

struct eautc;
static int eautc_file(struct eautc *ctx,const char *src,int srcc);

/* Namespace.
 */
 
struct eautc_ns {
  struct eautc_ns_entry {
    uint8_t chid,noteid;
    char *v;
    int c;
  } *v;
  int c,a;
};

static void eautc_ns_entry_cleanup(struct eautc_ns_entry *entry) {
  if (entry->v) free(entry->v);
}

static void eautc_ns_cleanup(struct eautc_ns *ns) {
  if (ns->v) {
    while (ns->c-->0) eautc_ns_entry_cleanup(ns->v+ns->c);
    free(ns->v);
  }
}

static int eautc_ns_search(const struct eautc_ns *ns,uint8_t chid,uint8_t noteid) {
  int lo=0,hi=ns->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eautc_ns_entry *q=ns->v+ck;
         if (chid<q->chid) hi=ck;
    else if (chid>q->chid) lo=ck+1;
    else if (noteid<q->noteid) hi=ck;
    else if (noteid>q->noteid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int eautc_ns_add(struct eautc_ns *ns,uint8_t chid,uint8_t noteid,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  if (srcc>0xff) return -1;
  int p=eautc_ns_search(ns,chid,noteid);
  if (p>=0) return 0; // Keep what we had already.
  p=-p-1;
  if (ns->c>=ns->a) {
    int na=ns->a+64;
    if (na>INT_MAX/sizeof(struct eautc_ns_entry)) return -1;
    void *nv=realloc(ns->v,sizeof(struct eautc_ns_entry)*na);
    if (!nv) return -1;
    ns->v=nv;
    ns->a=na;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  struct eautc_ns_entry *entry=ns->v+p;
  memmove(entry+1,entry,sizeof(struct eautc_ns_entry)*(ns->c-p));
  ns->c++;
  memset(entry,0,sizeof(struct eautc_ns_entry));
  entry->chid=chid;
  entry->noteid=noteid;
  entry->v=nv;
  entry->c=srcc;
  return p;
}

// Copy entries from (src) into (dst), yoinking as we go. Content strings don't get reallocated.
static int eautc_ns_copy(struct eautc_ns *dst,struct eautc_ns *src) {
  struct eautc_ns_entry *entry=src->v;
  int i=src->c;
  for (;i-->0;entry++) {
    int p=eautc_ns_search(dst,entry->chid,entry->noteid);
    if (p>=0) continue;
    p=-p-1;
    if (dst->c>=dst->a) {
      int na=dst->a+64;
      if (na>INT_MAX/sizeof(struct eautc_ns_entry)) return -1;
      void *nv=realloc(dst->v,sizeof(struct eautc_ns_entry)*na);
      if (!nv) return -1;
      dst->v=nv;
      dst->a=na;
    }
    struct eautc_ns_entry *dstentry=dst->v+p;
    memmove(dstentry+1,dstentry,sizeof(struct eautc_ns_entry)*(dst->c-p));
    dst->c++;
    *dstentry=*entry;
    entry->v=0;
    entry->c=0;
  }
  return 0;
}

/* Context.
 */
 
struct eautc {
  struct sr_encoder *dst;
  const char *osrc; // Outer source, for logging purposes.
  int osrcc;
  const char *src;
  int srcc;
  int srcp;
  const char *path;
  int error;
  eau_get_chdr_fn get_chdr;
  struct eautc_ns ns;
  int emitted_lead; // Zero or position in (dst) of the start of the lead's 4-byte payload.
  int events_done;
  int strip_names;
};

static void eautc_cleanup(struct eautc *ctx) {
  eautc_ns_cleanup(&ctx->ns);
}

/* Trivial text bits.
 */
 
static int eautc_lineno(const char *src,int srcc) {
  int nlc=1;
  for (;srcc-->0;src++) if (*src==0x0a) nlc++;
  return nlc;
}

static int eautc_isident(char ch) {
  if ((ch>='0')&&(ch<='9')) return 1;
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='A')&&(ch<='Z')) return 1;
  if (ch=='_') return 1;
  return 0;
}

/* Log error and fail.
 */
 
static int eautc_fail(struct eautc *ctx,const char *loc,const char *fmt,...) {
  if (ctx->error) return ctx->error;
  if (!ctx->path||!fmt) return -1;
  
  if (!loc) loc=ctx->src+ctx->srcp;
  int locp=loc-ctx->osrc;
  if ((locp<0)||(locp>ctx->osrcc)) locp=0;
  int lineno=eautc_lineno(ctx->osrc,locp);
  
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&(msg[msgc-1]==0x0a)) msgc--; // Remove trailing LFs because sometimes I forget.
  
  fprintf(stderr,"%s:%d: %.*s\n",ctx->path,lineno,msgc,msg);

  return ctx->error=-2;
}

/* Rewind readhead such that (token) will be the next token returned by eautc_next().
 * Fails if (token) is not among the text we've already processed.
 */
 
static int eautc_unread(struct eautc *ctx,const char *token) {
  int p=token-ctx->src;
  if ((p<0)||(p>=ctx->srcp)) return eautc_fail(ctx,0,"Illegal unread to %p. src=%p srcp=%d/%d.",token,ctx->src,ctx->srcp,ctx->srcc);
  ctx->srcp=p;
  return 0;
}

/* Skip whitespace and comments, and return the next token.
 * Zero at EOF.
 */
 
static int eautc_next(void *dstpp,struct eautc *ctx) {
  for (;;) {
    const char *token=ctx->src+ctx->srcp;
    int tokena=ctx->srcc-ctx->srcp;
    if (tokena<1) return 0;
    int tokenc=0;
    
    // Whitespace, including LF.
    if ((unsigned char)token[0]<=0x20) {
      ctx->srcp++;
      continue;
    }
    
    // Line comments.
    if (token[0]=='#') {
      while ((ctx->srcp<ctx->srcc)&&(ctx->src[ctx->srcp++]!=0x0a)) ;
      continue;
    }
    
    // Strings.
    if (token[0]=='"') {
      tokenc=1;
      ctx->srcp++;
      for (;;) {
        if (ctx->srcp>=ctx->srcc) return eautc_fail(ctx,token,"Unclosed string literal.");
        if (token[tokenc]==0x0a) return eautc_fail(ctx,token,"Unclosed string literal."); // Newline not permitted in string.
        if (token[tokenc]=='"') {
          ctx->srcp++;
          tokenc++;
          break;
        }
        if (token[tokenc]=='\\') {
          ctx->srcp+=2;
          tokenc+=2;
        } else {
          ctx->srcp+=1;
          tokenc+=1;
        }
      }
      *(const void**)dstpp=token;
      return tokenc;
    }
    
    // Identifiers and numbers.
    if (eautc_isident(token[0])) {
      tokenc=1;
      ctx->srcp++;
      while ((ctx->srcp<ctx->srcc)&&eautc_isident(token[tokenc])) { ctx->srcp++; tokenc++; }
      *(const void**)dstpp=token;
      return tokenc;
    }
    
    // Anything outside G0 is illegal (we've already accounted for low codes).
    if ((unsigned char)token[0]>=0x7f) return eautc_fail(ctx,token,"Unexpected byte 0x%02x.",(unsigned char)token[0]);
    
    // Everything else is one character of punctuation.
    *(const void**)dstpp=token;
    ctx->srcp++;
    return 1;
  }
}

/* Consume and return an entire statement.
 * The delimiter (semicolon or brackets) is not returned.
 * We DO NOT report EOF; EOF is an error. Empty statements are perfectly legal.
 */
 
static int eautc_next_statement(void *dstpp,struct eautc *ctx) {
  const char *token;
  int tokenc=eautc_next(&token,ctx);
  if (tokenc<0) return tokenc;
  if (!tokenc) return eautc_fail(ctx,0,"Unexpected EOF.");
  
  // Leading semicolon: Return empty immediately. Do use the token, its address matters for logging purposes.
  if ((tokenc==1)&&(token[0]==';')) { *(const void**)dstpp=token; return 0; }
  
  // Brackets. Begin at the next token, and track depth until we consume its partner (which we do not report).
  if ((tokenc==1)&&(token[0]=='{')) {
    const char *opentoken=token;
    const char *starttoken=0;
    int depth=1;
    for (;;) {
      if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
      if (!tokenc) return eautc_fail(ctx,opentoken,"Unmatched bracket.");
      if (!starttoken) starttoken=token;
      if ((tokenc==1)&&(token[0]=='{')) {
        depth++;
      } else if ((tokenc==1)&&(token[0]=='}')) {
        depth--;
        if (!depth) break;
      }
    }
    *(const void**)dstpp=starttoken;
    return token-starttoken;
  }
  
  // Anything else, start the statement here, and consume the next semicolon.
  const char *start=token;
  for (;;) {
    if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
    if (!tokenc) return eautc_fail(ctx,0,"Semicolon required to complete statement.");
    if ((tokenc==1)&&(token[0]==';')) break;
  }
  *(const void**)dstpp=start;
  return token-start;
}

/* Evaluate plain integer with a range and default.
 */
 
static int eautc_int_eval(int *dst,struct eautc *ctx,const char *src,int srcc,int lo,int hi,int fallback) {
  if (srcc<1) { *dst=fallback; return 0; }
  if ((sr_int_eval(dst,src,srcc)<2)||(*dst<lo)||(*dst>hi)) {
    return eautc_fail(ctx,src,"Expected integer in %d..%d, found '%.*s'",lo,hi,srcc,src);
  }
  return 0;
}

/* Evaluate mode.
 */
 
static int eautc_mode_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if ((srcc==4)&&!memcmp(src,"noop",4)) return 0;
  if ((srcc==4)&&!memcmp(src,"drum",4)) return 1;
  if ((srcc==2)&&!memcmp(src,"fm",2)) return 2;
  if ((srcc==5)&&!memcmp(src,"harsh",5)) return 3;
  if ((srcc==4)&&!memcmp(src,"harm",4)) return 4;
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>=0)&&(v<=0xff)) return v;
  return -1;
}

/* Evaluate shape.
 */
 
static int eautc_shape_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  if ((srcc==4)&&!memcmp(src,"sine",4)) return 0;
  if ((srcc==6)&&!memcmp(src,"square",6)) return 1;
  if ((srcc==3)&&!memcmp(src,"saw",3)) return 2;
  if ((srcc==8)&&!memcmp(src,"triangle",8)) return 3;
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>=0)&&(v<=0xff)) return v;
  return -1;
}

/* Emit the signature chunk if we haven't done yet.
 */
 
static int eautc_require_lead(struct eautc *ctx) {
  if (ctx->emitted_lead) return 0;
  if (sr_encode_raw(ctx->dst,"\0EAU\0\0\0\4",8)<0) return -1;
  ctx->emitted_lead=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\x01\xf4\x00\x00",4)<0) return -1; // 500 ms/qnote, the default.
  return 0;
}

/* "tempo MS_PER_QNOTE ;"
 */
 
static int eautc_tempo(struct eautc *ctx,const char *kw) {
  if (ctx->emitted_lead) return eautc_fail(ctx,kw,"'tempo' may only appear before other global statements.");
  const char *token=0;
  int tokenc=eautc_next(&token,ctx);
  if (tokenc<0) return tokenc;
  if (!tokenc) return eautc_fail(ctx,kw,"Unexpected EOF.");
  int msperqnote=0;
  if ((sr_int_eval(&msperqnote,token,tokenc)<2)||(msperqnote<1)||(msperqnote>0xffff)) {
    return eautc_fail(ctx,token,"Expected 1..65535 (ms/qnote) for 'tempo', found '%.*s'.",tokenc,token);
  }
  if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
  if ((tokenc!=1)||(token[0]!=';')) return eautc_fail(ctx,token,"';' required to complete 'tempo' statement.");
  if (sr_encode_raw(ctx->dst,"\0EAU\0\0\0\4",8)<0) return -1;
  ctx->emitted_lead=ctx->dst->c;
  if (sr_encode_intbe(ctx->dst,msperqnote,2)<0) return -1;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1; // Placeholder for loop position.
  return 0;
}

/* modecfg for DRUM channels.
 */

static int eautc_modecfg_drum_1(struct eautc *ctx,const char *src,int srcc,int chid) {
  int err;

  /* Prep a subcontext and read members of the 'drum' block.
   * No need to clean up (subctx).
   */
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  struct drum_inputs {
    const char *name,*noteid,*trimlo,*trimhi,*pan,*serial;
    int namec,noteidc,trimloc,trimhic,panc,serialc;
  } inputs={0};
  for (;;) {
    const char *token;
    int tokenc=eautc_next(&token,&subctx);
    if (tokenc<0) return tokenc;
    if (!tokenc) break;
    // Trim is not like the others: Read it as two tokens.
    if ((tokenc==4)&&!memcmp(token,"trim",4)) {
      if ((inputs.trimloc=eautc_next(&inputs.trimlo,&subctx))<1) return eautc_fail(ctx,token,"Expected '0..255 0..255' for drum trim.");
      if ((inputs.trimhic=eautc_next(&inputs.trimhi,&subctx))<1) return eautc_fail(ctx,token,"Expected '0..255 0..255' for drum trim.");
      tokenc=eautc_next(&token,&subctx);
      if ((tokenc!=1)||(token[0]!=';')) return eautc_fail(ctx,token,"Expected ';' to complete drum trim statement.");
      continue;
    }
    // Other fields, read as a statement.
    #define _(tag) if ((tokenc==sizeof(#tag)-1)&&!memcmp(token,#tag,tokenc)) { \
      if ((inputs.tag##c=eautc_next_statement(&inputs.tag,&subctx))<0) return inputs.tag##c; \
      continue; \
    }
    _(name)
    _(noteid)
    _(pan)
    _(serial)
    #undef _
    // And even otherer fields, choke.
    return eautc_fail(ctx,token,"Unexpected command '%.*s' in drum body. (name,noteid,trim,pan,serial)",tokenc,token);
  }
  
  // Evaluate scalars, easy peasy.
  int noteid,trimlo,trimhi,pan;
  if ((err=eautc_int_eval(&noteid,ctx,inputs.noteid,inputs.noteidc,0,255,-1))<0) {
    return eautc_fail(ctx,inputs.noteid?inputs.noteid:src,"Expected 'noteid 0..255;' in drum block.");
  }
  if ((err=eautc_int_eval(&trimlo,ctx,inputs.trimlo,inputs.trimloc,0,255,0xff))<0) return err;
  if ((err=eautc_int_eval(&trimhi,ctx,inputs.trimhi,inputs.trimhic,0,255,trimlo))<0) return err;
  if ((err=eautc_int_eval(&pan,ctx,inputs.pan,inputs.panc,0,255,0x80))<0) return err;
  
  // If we got a name, add it to (ctx). NB not (subctx).
  if (inputs.name) {
    if (!noteid) return eautc_fail(ctx,inputs.name,"Note zero can't have a name.");
    char tmp[256];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),inputs.name,inputs.namec);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return eautc_fail(ctx,inputs.name,"Malformed string for drum name.");
    if (tmpc) eautc_ns_add(&ctx->ns,chid,noteid,tmp,tmpc);
  }
  
  // Emit preamble.
  if (sr_encode_u8(ctx->dst,noteid)<0) return -1;
  if (sr_encode_u8(ctx->dst,trimlo)<0) return -1;
  if (sr_encode_u8(ctx->dst,trimhi)<0) return -1;
  if (sr_encode_u8(ctx->dst,pan)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  
  // Enter a new file for (serial).
  if ((err=eautc_file(ctx,inputs.serial,inputs.serialc))<0) return err;
  
  // Fill in length.
  int len=ctx->dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return eautc_fail(ctx,inputs.serial,"Invalid length %d for drum's inner EAU file.",len);
  ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len;
  
  return 0;
}
 
static int eautc_modecfg_drum(struct eautc *ctx,int chid) {
  for (;;) {
    const char *token;
    int tokenc=eautc_next(&token,ctx);
    if (tokenc<=0) return tokenc;
    
    if ((tokenc==4)&&!memcmp(token,"drum",4)) {
      const char *src;
      int srcc=eautc_next_statement(&src,ctx);
      int err=eautc_modecfg_drum_1(ctx,src,srcc,chid);
      if (err<0) return err;
      continue;
    }
    
    return eautc_fail(ctx,token,"Unexpected command '%.*s' in modecfg for drum channel.",tokenc,token);
  }
  return 0;
}

/* Envelope.
 */

// Caller is responsible for the leading '=' or '+', and the optional trailing '*'. Don't give them to us.
static int eautc_env_point_eval(int *lo,int *hi,struct eautc *ctx,const char *src,int srcc) {
  int i=0;
  for (;i<srcc;i++) {
    if (src[i]=='.') {
      if (
        (i>srcc-3)||(src[i+1]!='.')||
        (sr_int_eval(lo,src,i)<2)||(*lo<0)||(*lo>0xffff)||
        (sr_int_eval(hi,src+i+2,srcc-2-i)<2)||(*hi<0)||(*hi>0xffff)
      ) return eautc_fail(ctx,src,"Malformed envelope token '%.*s'.",srcc,src);
      return 0;
    }
  }
  if (sr_int_eval(lo,src,srcc)<2) return eautc_fail(ctx,src,"Expected integer or 'LO..HI', found '%.*s'.",srcc,src);
  if ((*lo<0)||(*lo>0xffff)) return eautc_fail(ctx,src,"Envelope times and levels must be in 0..65535, found '%.*s'",srcc,src);
  *hi=*lo;
  return 0;
}
 
static int eautc_env(struct eautc *ctx,const char *src,int srcc) {

  // Temporary model to read into.
  uint8_t flags=0;
  int susp=-1;
  int initlo=0,inithi=0;
  struct point {
    int tlo,thi,vlo,vhi;
  } pointv[15];
  int pointc=0;
  
  // Subcontext to read from.
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=ctx->src,
    .srcc=ctx->srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  const char *token;
  int tokenc,err;
  
  /* If the first token begins '=', it's an initial level.
   * Otherwise, put it back.
   */
  if ((tokenc=eautc_next(&token,&subctx))<0) return tokenc;
  if (tokenc&&(token[0]=='=')) {
    if ((err=eautc_env_point_eval(&initlo,&inithi,ctx,token+1,tokenc-1))<0) return err;
    flags|=0x01; // Initials.
    if (initlo!=inithi) flags|=0x02; // Velocity.
  } else {
    eautc_unread(ctx,token);
  }
  
  /* Remaining tokens are in (TIME,LEVEL) pairs, each pair corresponding to one point.
   */
  for (;;) {
    if ((tokenc=eautc_next(&token,&subctx))<=0) break;
    if (pointc>=15) return eautc_fail(ctx,token,"Too many points in envelope, limit 15.");
    struct point *point=pointv+pointc++;
    if (token[0]!='+') return eautc_fail(ctx,token,"Expected envelope leg time with leading '+', found '%.*s'",tokenc,token);
    if ((err=eautc_env_point_eval(&point->tlo,&point->thi,ctx,token+1,tokenc-1))<0) return err;
    if ((tokenc=eautc_next(&token,&subctx))<=0) return eautc_fail(ctx,token,"Expected envelope leg level.");
    if (token[0]!='=') return eautc_fail(ctx,token,"Expected envelope leg level with leading '=', found '%.*s'",tokenc,token);
    if (token[tokenc-1]=='*') {
      if (susp>=0) return eautc_fail(ctx,token,"Multiple sustain points in envelope.");
      flags|=0x04; // Sustain.
      susp=pointc-1;
      tokenc--;
    }
    if ((err=eautc_env_point_eval(&point->vlo,&point->vhi,ctx,token+1,tokenc-1))<0) return err;
    // Acquire the Velocity flag if times or levels are different.
    if ((point->tlo!=point->thi)||(point->vlo!=point->vhi)) flags|=0x02;
  }
  if (tokenc<0) return tokenc;
  
  /* And we're ready to emit.
   */
  if (sr_encode_u8(ctx->dst,flags)<0) return -1;
  if (flags&0x01) {
    if (sr_encode_intbe(ctx->dst,initlo,2)<0) return -1;
    if (flags&0x02) {
      if (sr_encode_intbe(ctx->dst,inithi,2)<0) return -1;
    }
  }
  if (sr_encode_u8(ctx->dst,(susp<<4)|pointc)<0) return -1;
  struct point *pt=pointv;
  int i=pointc;
  for (;i-->0;pt++) {
    if (sr_encode_intbe(ctx->dst,pt->tlo,2)<0) return -1;
    if (sr_encode_intbe(ctx->dst,pt->vlo,2)<0) return -1;
    if (flags&0x02) {
      if (sr_encode_intbe(ctx->dst,pt->thi,2)<0) return -1;
      if (sr_encode_intbe(ctx->dst,pt->vhi,2)<0) return -1;
    }
  }
  
  return 0;
}

/* Harmonics.
 */
 
static int eautc_harmonics(struct eautc *ctx,const char *src,int srcc) {
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=ctx->src,
    .srcc=ctx->srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  int cp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,0)<0) return -1;
  int harmc=0;
  const char *token;
  int tokenc;
  while ((tokenc=eautc_next(&token,&subctx))>0) {
    if ((tokenc>2)&&(!memcmp(token,"0x",2)||!memcmp(token,"0X",2))) { tokenc-=2; token+=2; }
    int v=0;
    for (;tokenc-->0;token++) {
      int digit=sr_digit_eval(*token);
      if ((digit<0)||(digit>15)) return eautc_fail(ctx,token,"Invalid digit '%c' in hex word.",*token);
      v<<=4;
      v|=digit;
      if (v>0xffff) return eautc_fail(ctx,token,"Harmonics args must be hexadecimal integers in 0..ffff.");
    }
    if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  }
  eautc_cleanup(&subctx); // Shouldn't be necessary, we're only reading tokens, but cleaning up is good form.
  if (tokenc<0) return tokenc;
  if (harmc>0xff) return eautc_fail(ctx,src,"Too many harmonics (%d, limit 255)",harmc);
  ((uint8_t*)ctx->dst->v)[cp]=harmc;
  return 0;
}

/* Modecfg for FM, HARSH, HARM.
 * Second stage.
 */

struct modecfg_voice_inputs {
  const char *shape,*harmonics,*rate,*absrate,*range,*levelenv,*rangeenv,*pitchenv,*wheelrange,*lforate,*lfodepth,*lfophase;
  int shapec,harmonicsc,ratec,absratec,rangec,levelenvc,rangeenvc,pitchenvc,wheelrangec,lforatec,lfodepthc,lfophasec;
};

static int eautc_modecfg_fm(struct eautc *ctx,const struct modecfg_voice_inputs *inputs) {
  int v,err;
  int fieldc=
    inputs->lfophase?  9:
    inputs->lfodepth?  8:
    inputs->lforate?   7:
    inputs->wheelrange?6:
    inputs->pitchenv?  5:
    inputs->rangeenv?  4:
    inputs->levelenv?  3:
    inputs->range?     2:
    (inputs->rate||inputs->absrate)?1:
  0;
  
  if (fieldc<1) return 0;
  if (inputs->rate) {
    if ((err=eautc_int_eval(&v,ctx,inputs->rate,inputs->ratec,0,0x7fff,0))<0) return err;
  } else {
    if ((err=eautc_int_eval(&v,ctx,inputs->absrate,inputs->absratec,0,0x7fff,0))<0) return err;
    v|=0x8000;
  }
  if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  
  if (fieldc<2) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->range,inputs->rangec,0,0xffff,0))<0) return err;
  if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  
  if (fieldc<3) return 0;
  if ((err=eautc_env(ctx,inputs->levelenv,inputs->levelenvc))<0) return err;
  
  if (fieldc<4) return 0;
  if ((err=eautc_env(ctx,inputs->rangeenv,inputs->rangeenvc))<0) return err;
  
  if (fieldc<5) return 0;
  if ((err=eautc_env(ctx,inputs->pitchenv,inputs->pitchenvc))<0) return err;
  
  if (fieldc<6) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->wheelrange,inputs->wheelrangec,0,0xffff,200))<0) return err;
  if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  
  if (fieldc<7) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->lforate,inputs->lforatec,0,0xffff,0))<0) return err;
  if (sr_encode_intbe(ctx->dst,v,2)<0) return -1;
  
  if (fieldc<8) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->lfodepth,inputs->lfodepthc,0,0xff,0xff))<0) return err;
  if (sr_encode_u8(ctx->dst,v)<0) return -1;
  
  if (fieldc<9) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->lfophase,inputs->lfophasec,0,0xff,0))<0) return err;
  if (sr_encode_u8(ctx->dst,v)<0) return -1;
  
  return 0;
}

static int eautc_modecfg_harsh(struct eautc *ctx,const struct modecfg_voice_inputs *inputs) {
  int v,err;
  int fieldc=
    inputs->wheelrange?4:
    inputs->pitchenv?  3:
    inputs->levelenv?  2:
    inputs->shape?     1:
  0;
  
  if (fieldc<1) return 0;
  if ((v=eautc_shape_eval(inputs->shape,inputs->shapec))<0) {
    return eautc_fail(ctx,inputs->shape,"Expected 0..255 or (sine,sqaure,saw,triangle) for shape, found '%.*s'.",inputs->shapec,inputs->shape);
  }
  if (sr_encode_u8(ctx->dst,v)<0) return -1;
  
  if (fieldc<2) return 0;
  if ((err=eautc_env(ctx,inputs->levelenv,inputs->levelenvc))<0) return err;
  
  if (fieldc<3) return 0;
  if ((err=eautc_env(ctx,inputs->pitchenv,inputs->pitchenvc))<0) return err;
  
  if (fieldc<4) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->wheelrange,inputs->wheelrangec,0,0xffff,200))<0) return err;
  
  return 0;
}

static int eautc_modecfg_harm(struct eautc *ctx,const struct modecfg_voice_inputs *inputs) {
  int v,err;
  int fieldc=
    inputs->wheelrange?4:
    inputs->pitchenv?  3:
    inputs->levelenv?  2:
    inputs->harmonics? 1:
  0;
  
  if (fieldc<1) return 0;
  if ((err=eautc_harmonics(ctx,inputs->harmonics,inputs->harmonicsc))<0) return err;
  
  if (fieldc<2) return 0;
  if ((err=eautc_env(ctx,inputs->levelenv,inputs->levelenvc))<0) return err;
  
  if (fieldc<3) return 0;
  if ((err=eautc_env(ctx,inputs->pitchenv,inputs->pitchenvc))<0) return err;
  
  if (fieldc<4) return 0;
  if ((err=eautc_int_eval(&v,ctx,inputs->wheelrange,inputs->wheelrangec,0,0xffff,200))<0) return err;
  
  return 0;
}

/* modecfg for FM, HARSH, or HARM channels.
 * These have a few fields in common (levelenv,pitchenv,wheelrange), and the other fields we can just emit if they exist and skip if not.
 * Caller should provide the specific mode, just so we can fail when some other mode's field is provided.
 */
 
static int eautc_modecfg_voice(struct eautc *ctx,int mode) {

  /* Collect statements without emitting anything, similar to what we do at 'chdr' level.
   */
  struct modecfg_voice_inputs inputs={0};
  for (;;) {
    const char *kw;
    int kwc=eautc_next(&kw,ctx);
    if (kwc<0) return kwc;
    if (!kwc) break;
    #define _(tag) if ((kwc==sizeof(#tag)-1)&&!memcmp(kw,#tag,kwc)) { \
      if (inputs.tag) return eautc_fail(ctx,kw,"Duplicate field '%.*s' in modecfg.",kwc,kw); \
      if ((inputs.tag##c=eautc_next_statement(&inputs.tag,ctx))<0) return inputs.tag##c; \
      continue; \
    }
    _(shape)
    _(harmonics)
    _(rate)
    _(absrate)
    _(range)
    _(levelenv)
    _(rangeenv)
    _(pitchenv)
    _(wheelrange)
    _(lforate)
    _(lfodepth)
    _(lfophase)
    #undef _
    return eautc_fail(ctx,kw,"Unexpected command '%.*s' in modecfg.",kwc,kw);
  }
  
  /* Enforce per-mode restrictions.
   * Most of the fields belong to fm, 3 are shared amongst all, and harsh and harm each have one of their own.
   */
  #define FORBID(tag) if (inputs.tag) return eautc_fail(ctx,inputs.tag,"Modecfg command '%.*s' not valid for mode %d.",inputs.tag##c,inputs.tag,mode);
  if (mode!=2) {
    FORBID(rate)
    FORBID(absrate)
    FORBID(range)
    FORBID(rangeenv)
    FORBID(lforate)
    FORBID(lfodepth)
    FORBID(lfophase)
  }
  if (mode!=3) {
    FORBID(shape)
  }
  if (mode!=4) {
    FORBID(harmonics)
  }
  #undef FORBID
  
  /* Pass along to the mode-specific decoder.
   */
  switch (mode) {
    case 2: return eautc_modecfg_fm(ctx,&inputs);
    case 3: return eautc_modecfg_harsh(ctx,&inputs);
    case 4: return eautc_modecfg_harm(ctx,&inputs);
  }
  return eautc_fail(ctx,0,"Internal error. Entered %s with mode %d.",__func__,mode);
}

/* modecfg for unknown mode.
 * Content is a simple hex dump.
 */
 
static int eautc_modecfg_generic(struct eautc *ctx) {
  for (;;) {
    const char *token;
    int tokenc=eautc_next(&token,ctx);
    if (tokenc<=0) return tokenc;
    if ((tokenc>=2)&&(token[0]=='0')&&((token[1]=='x')||(token[1]=='X'))) {
      token+=2;
      tokenc-=2;
    }
    if (tokenc&1) return eautc_fail(ctx,token,"Hex dump tokens must have even length.");
    while (tokenc>=2) {
      int hi=sr_digit_eval(token[0]);
      int lo=sr_digit_eval(token[1]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) {
        return eautc_fail(ctx,token,"Invalid hex byte '%.2s'",token);
      }
      if (sr_encode_u8(ctx->dst,(hi<<4)|lo)<0) return -1;
      tokenc-=2;
      token+=2;
    }
  }
}

/* Evaluate and emit a 'modecfg' statement, including its leading length.
 */
 
static int eautc_modecfg(struct eautc *ctx,const char *src,int srcc,int mode,int chid) {
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .srcp=0,
    .path=ctx->path,
    .error=0,
    .get_chdr=ctx->get_chdr,
    .ns={0},
    .emitted_lead=0,
    .events_done=0,
    .strip_names=ctx->strip_names,
  };
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  int err;
  switch (mode) {
    case 1: err=eautc_modecfg_drum(&subctx,chid); break;
    case 2: err=eautc_modecfg_voice(&subctx,mode); break;
    case 3: err=eautc_modecfg_voice(&subctx,mode); break;
    case 4: err=eautc_modecfg_voice(&subctx,mode); break;
    default: err=eautc_modecfg_generic(&subctx); break;
  }
  if (err<0) {
    eautc_cleanup(&subctx);
    return err;
  }
  int len=ctx->dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return eautc_fail(ctx,src,"Modecfg length %d exceeds 64 kB limit.",len);
  ((uint8_t*)ctx->dst->v)[lenp+0]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len;
  err=eautc_ns_copy(&ctx->ns,&subctx.ns);
  eautc_cleanup(&subctx);
  return err;
}

/* delay PERIOD_U88 DRY WET STO FBK SPARKLE ; # 0..255
 */
 
static int eautc_post_delay(struct eautc *ctx,const char *src,int srcc) {
  int tokenc,err,period,dry,wet,sto,fbk,sparkle;
  const char *token;
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  if (sr_encode_u8(ctx->dst,0x01)<0) return -1;
  if (sr_encode_u8(ctx->dst,7)<0) return -1;
  
  if ((tokenc=eautc_next(&token,&subctx))<0) return tokenc;
  if (!tokenc) return eautc_fail(ctx,src,"Expected u8.8 period in qnotes.");
  if ((err=eautc_int_eval(&period,ctx,token,tokenc,0,0xffff,0))<0) return err;
  if (sr_encode_intbe(ctx->dst,period,2)<0) return -1;
  
  if ((tokenc=eautc_next(&dry,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&dry,ctx,token,tokenc,0,0xff,0x80))<0) return err;
  if (sr_encode_u8(ctx->dst,dry)<0) return -1;
  
  if ((tokenc=eautc_next(&wet,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&wet,ctx,token,tokenc,0,0xff,0x80))<0) return err;
  if (sr_encode_u8(ctx->dst,wet)<0) return -1;
  
  if ((tokenc=eautc_next(&sto,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&sto,ctx,token,tokenc,0,0xff,0x80))<0) return err;
  if (sr_encode_u8(ctx->dst,sto)<0) return -1;
  
  if ((tokenc=eautc_next(&fbk,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&fbk,ctx,token,tokenc,0,0xff,0x80))<0) return err;
  if (sr_encode_u8(ctx->dst,fbk)<0) return -1;
  
  if ((tokenc=eautc_next(&sparkle,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&sparkle,ctx,token,tokenc,0,0xff,0x80))<0) return err;
  if (sr_encode_u8(ctx->dst,sparkle)<0) return -1;
  
  return 0;
}

/* tremolo PERIOD_U88 DEPTH PHASE ; # 0..255
 */
 
static int eautc_post_tremolo(struct eautc *ctx,const char *src,int srcc) {
  int depth=0xff,phase=0;
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  const char *token;
  int tokenc,err;
  if (sr_encode_u8(ctx->dst,0x03)<0) return -1;
  if (sr_encode_u8(ctx->dst,4)<0) return -1; // We can leave off depth and phase but whatever, it's just two bytes.
  int period;
  if ((tokenc=eautc_next(&token,&subctx))<0) return tokenc;
  if (!tokenc) return eautc_fail(ctx,src,"Expected u8.8 period in qnotes.");
  if ((err=eautc_int_eval(&period,ctx,token,tokenc,0,0xffff,0))<0) return err;
  if (sr_encode_intbe(ctx->dst,period,2)<0) return -1;
  if ((tokenc=eautc_next(&token,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&depth,ctx,token,tokenc,0,0xff,0xff))<0) return err;
  if (sr_encode_u8(ctx->dst,depth)<0) return -1;
  if ((tokenc=eautc_next(&token,&subctx))<0) return tokenc;
  if ((err=eautc_int_eval(&phase,ctx,token,tokenc,0,0xff,0))<0) return err;
  if (sr_encode_u8(ctx->dst,phase)<0) return -1;
  return 0;
}

/* STAGEID HEXDUMP ;
 */
 
static int eautc_post_generic(struct eautc *ctx,const char *kw,int kwc,const char *src,int srcc) {
  int stageid,err;
  if ((err=eautc_int_eval(&stageid,ctx,kw,kwc,0,255,-1))<0) return err;
  if (stageid<0) return eautc_fail(ctx,src,"Expected 'delay', 'tremolo', 'waveshaper', or 0..255 for post stageid.");
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  if (sr_encode_u8(ctx->dst,stageid)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_u8(ctx->dst,0)<0) return -1;
  for (;;) {
    const char *token;
    int tokenc=eautc_next(&token,&subctx);
    if (tokenc<0) return tokenc;
    if (!tokenc) break;
    if (tokenc&1) return eautc_fail(ctx,token,"Hexdump tokens must have even length ('%.*s')",tokenc,token);
    if ((tokenc>=2)&&(!memcmp(token,"0x",2)||!memcmp(token,"0X",2))) { token+=2; tokenc-=2; }
    int tokenp=0;
    while (tokenp<tokenc) {
      int hi=sr_digit_eval(token[tokenp++]);
      int lo=sr_digit_eval(token[tokenp++]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return eautc_fail(ctx,token,"Invalid hex token.");
      if (sr_encode_u8(ctx->dst,(hi<<4)|lo)<0) return -1;
    }
  }
  int len=ctx->dst->c-lenp-1;
  if ((len<0)||(len>0xff)) return eautc_fail(ctx,kw,"Invalid length %d for post stage payload, must be in 0..255.",len);
  ((uint8_t*)ctx->dst->v)[lenp]=len;
  return 0;
}

/* Evaluate and emit a 'post' statement, including its leading length.
 */
 
static int eautc_post(struct eautc *ctx,const char *src,int srcc) {
  int lenp=ctx->dst->c,err;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .path=ctx->path,
    .strip_names=ctx->strip_names,
  };
  for (;;) {
    const char *token;
    int tokenc=eautc_next(&token,&subctx);
    if (tokenc<0) return tokenc;
    if (!tokenc) break;
    const char *params;
    int paramsc=eautc_next_statement(&params,&subctx);
    if (paramsc<0) return paramsc;
    
    if ((tokenc==5)&&!memcmp(token,"delay",5)) err=eautc_post_delay(ctx,params,paramsc);
    else if ((tokenc==7)&&!memcmp(token,"tremolo",7)) err=eautc_post_tremolo(ctx,params,paramsc);
    else if ((tokenc==10)&&!memcmp(token,"waveshaper",10)) err=eautc_post_generic(ctx,"2",1,params,paramsc); // waveshaper payload is exactly the same as generic
    else err=eautc_post_generic(ctx,token,tokenc,params,paramsc);
    if (err<0) return err;
  }
  int len=ctx->dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return eautc_fail(ctx,src,"Post too long (%d, limit 65535).",len);
  ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len;
  return 0;
}

/* "chdr { ... }"
 */
 
static int eautc_chdr(struct eautc *ctx,const char *kw) {
  int err=eautc_require_lead(ctx);
  if (err<0) return err;
  
  // First, collect all the child statements. Don't evaluate anything yet, except statement framing.
  const char *token;
  int tokenc=eautc_next(&token,ctx);
  if (tokenc<0) return tokenc;
  if ((tokenc!=1)||(token[0]!='{')) return eautc_fail(ctx,kw,"Expected bracketted body after 'chdr'.");
  const char *opentoken=token;
  struct {
    const char *name,*chid,*trim,*pan,*mode,*modecfg,*post;
    int namec,chidc,trimc,panc,modec,modecfgc,postc;
  } inputs={0};
  for (;;) {
    if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
    if (!tokenc) return eautc_fail(ctx,opentoken,"Unclosed 'chdr' block.");
    if ((tokenc==1)&&(token[0]=='}')) break;
    #define _(tag) if ((tokenc==sizeof(#tag)-1)&&!memcmp(token,#tag,tokenc)) { \
      if (inputs.tag) return eautc_fail(ctx,token,"Multiple '%s' in 'chdr' block.",#tag); \
      if ((inputs.tag##c=eautc_next_statement(&inputs.tag,ctx))<0) return inputs.tag##c; \
      continue; \
    }
    _(name)
    _(chid)
    _(trim)
    _(pan)
    _(mode)
    _(modecfg)
    _(post)
    #undef _
    return eautc_fail(ctx,token,"Unexpected keyword '%.*s' in 'chdr' block. ('name', 'chid', 'trim', 'pan', 'mode', 'modecfg', 'post')",tokenc,token);
  }
  
  // chid, trim, pan, and mode are required. We can default all but (chid).
  if (!inputs.chid) return eautc_fail(ctx,kw,"'chdr' block must contain a 'chid' statement.");
  int chid,trim,pan,mode;
  if ((sr_int_eval(&chid,inputs.chid,inputs.chidc)<2)||(chid<0)||(chid>0xff)) {
    return eautc_fail(ctx,inputs.chid,"Expected integer in 0..255 for 'chid', found '%.*s'",inputs.chidc,inputs.chid);
  }
  if ((err=eautc_int_eval(&trim,ctx,inputs.trim,inputs.trimc,0,0xff,0x40))<0) return err;
  if ((err=eautc_int_eval(&pan,ctx,inputs.pan,inputs.panc,0,0xff,0x80))<0) return -1;
  if (!inputs.mode) mode=2;
  else if ((mode=eautc_mode_eval(inputs.mode,inputs.modec))<0) {
    return eautc_fail(ctx,inputs.mode,"Expected integer in 0..255 or (noop,drum,fm,harsh,harm) for 'mode', found '%.*s'",inputs.modec,inputs.mode);
  }
  
  // Intern name if we got one.
  if (inputs.name) {
    char tmp[256];
    int tmpc=sr_string_eval(tmp,sizeof(tmp),inputs.name,inputs.namec);
    if ((tmpc<0)||(tmpc>sizeof(tmp))) return eautc_fail(ctx,inputs.name,"Malformed channel name string.");
    if (tmpc) eautc_ns_add(&ctx->ns,chid,0,tmp,tmpc);
  }
  
  // Emit and evaluate.
  if (sr_encode_raw(ctx->dst,"CHDR",4)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  
  if (sr_encode_u8(ctx->dst,chid)<0) return -1;
  if (sr_encode_u8(ctx->dst,trim)<0) return -1;
  if (sr_encode_u8(ctx->dst,pan)<0) return -1;
  if (sr_encode_u8(ctx->dst,mode)<0) return -1;
  if (inputs.modecfg||inputs.post) {
    if ((err=eautc_modecfg(ctx,inputs.modecfg,inputs.modecfgc,mode,chid))<0) return err;
  }
  if (inputs.post) {
    if ((err=eautc_post(ctx,inputs.post,inputs.postc))<0) return err;
  }

  int len=ctx->dst->c-lenp-4;
  if (len<0) return eautc_fail(ctx,kw,"Invalid length for 'chdr' block."); // unlikely
  ((uint8_t*)ctx->dst->v)[lenp+0]=len>>24;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len>>16;
  ((uint8_t*)ctx->dst->v)[lenp+2]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+3]=len;
  return 0;
}

/* "delay MS ; "
 */
 
static int eautc_delay(struct eautc *ctx,const char *kw) {
  const char *token;
  int tokenc=eautc_next(&token,ctx);
  if (tokenc<0) return tokenc;
  if (!tokenc) return eautc_fail(ctx,kw,"Unexpected EOF.");
  int ms,err;
  // 0xffff is not a technical limit, just a sanity limit. 64 seconds of delay is ridiculous and obviously an error.
  if ((err=eautc_int_eval(&ms,ctx,token,tokenc,0,0xffff,0))<0) return err;
  while (ms>4096) {
    if (sr_encode_u8(ctx->dst,0x7f)<0) return -1;
    ms-=4096;
  }
  if (ms>0x3f) {
    if (sr_encode_u8(ctx->dst,0x40|((ms>>6)-1))<0) return -1;
    ms&=0x3f;
  }
  if (ms) {
    if (sr_encode_u8(ctx->dst,ms)<0) return -1;
  }
  if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
  if ((tokenc!=1)||(token[0]!=';')) return eautc_fail(ctx,token,"';' required to complete delay event.");
  return 0;
}

/* "note CHID NOTEID VELOCITY DURMS ; "
 */
 
static int eautc_note(struct eautc *ctx,const char *kw) {
  const char *token;
  int tokenc,err,chid,noteid,velocity,dur;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&chid,ctx,token,tokenc,0,15,0))<0) return err;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&noteid,ctx,token,tokenc,0,0x7f,0x40))<0) return err;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&velocity,ctx,token,tokenc,0,0x7f,0x40))<0) return err;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&dur,ctx,token,tokenc,0,16380,0))<0) return err;
  dur>>=4;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((tokenc!=1)||(token[0]!=';')) return eautc_fail(ctx,token,"';' required to complete note event.");
  
  // 10ccccnn nnnnnvvv vvvvdddd dddddddd : Note (n) on channel (c), velocity (v), duration (d*4) ms.
  if (sr_encode_u8(ctx->dst,0x80|(chid<<2)|(noteid>>5))<0) return -1;
  if (sr_encode_u8(ctx->dst,(noteid<<3)|(velocity>>4))<0) return -1;
  if (sr_encode_u8(ctx->dst,(velocity<<4)|(dur>>8))<0) return -1;
  if (sr_encode_u8(ctx->dst,dur)<0) return -1;
  
  return 0;
}

/* "wheel CHID 0..512..1023 ; "
 */
 
static int eautc_wheel(struct eautc *ctx,const char *kw) {
  const char *token;
  int tokenc,err,chid,wheel;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&chid,ctx,token,tokenc,0,15,0))<0) return err;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((err=eautc_int_eval(&wheel,ctx,token,tokenc,0,1023,512))<0) return err;
  
  if ((tokenc=eautc_next(&token,ctx))<0) return -1;
  if ((tokenc!=1)||(token[0]!=';')) return eautc_fail(ctx,token,"';' required to complete wheel event.");
  
  // 11ccccww wwwwwwww : Wheel on channel (c), (w)=0..512..1023 = -1..0..1
  if (sr_encode_u8(ctx->dst,0xc0|(chid<<2)|(wheel>>8))<0) return -1;
  if (sr_encode_u8(ctx->dst,wheel)<0) return -1;
  
  return 0;
}

/* "events { ... }"
 */
 
static int eautc_events(struct eautc *ctx,const char *kw) {
  if (ctx->events_done) return eautc_fail(ctx,kw,"'events' blocks can't be more than 2 and must be adjacent.");
  int err=eautc_require_lead(ctx);
  if (err<0) return err;
  if (sr_encode_raw(ctx->dst,"EVTS",4)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  const char *token;
  int tokenc=eautc_next(&token,ctx);
  if (tokenc<0) return tokenc;
  if ((tokenc!=1)||(token[0]!='{')) return eautc_fail(ctx,kw,"Expected bracketted body after 'events'.");
  const char *opentoken=token;
  int looped=0;
  for (;;) {
    if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
    if (!tokenc) return eautc_fail(ctx,opentoken,"Unclosed 'events' block.");
    
    if ((tokenc==1)&&(token[0]=='}')) {
      // Peek at the next token. If it's "events" again, emit the loop point and continue into that next block.
      // This is why we require "events" chunks to be adjacent.
      if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
      if (!looped&&(tokenc==6)&&!memcmp(token,"events",6)) {
        looped=1;
        if ((tokenc=eautc_next(&token,ctx))<0) return tokenc;
        if ((tokenc!=1)||(token[0]!='{')) return eautc_fail(ctx,token,"Expected bracketted body after 'events'.");
        opentoken=token;
        int loopp=ctx->dst->c-lenp-4;
        if ((loopp<0)||(loopp>0xffff)) return eautc_fail(ctx,token,"Invalid loop position. (limit 64 kB from start of events).");
        ((uint8_t*)ctx->dst->v)[ctx->emitted_lead+2]=loopp>>8;
        ((uint8_t*)ctx->dst->v)[ctx->emitted_lead+3]=loopp;
        continue;
      } else {
        if ((err=eautc_unread(ctx,token))<0) return err;
        break;
      }
    }
    
    if ((tokenc==5)&&!memcmp(token,"delay",5)) err=eautc_delay(ctx,token);
    else if ((tokenc==4)&&!memcmp(token,"note",4)) err=eautc_note(ctx,token);
    else if ((tokenc==5)&&!memcmp(token,"wheel",5)) err=eautc_wheel(ctx,token);
    else return eautc_fail(ctx,token,"Unexpected keyword '%.*s' in 'events' block. ('delay', 'note', 'wheel').",tokenc,token);
    if (err<0) return err;
  }
  int len=ctx->dst->c-lenp-4;
  if (len<0) return eautc_fail(ctx,kw,"Invalid length for 'events' block."); // unlikely
  ((uint8_t*)ctx->dst->v)[lenp+0]=len>>24;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len>>16;
  ((uint8_t*)ctx->dst->v)[lenp+2]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+3]=len;
  return 0;
}

/* Compile one statement at global scope.
 * Zero at EOF, >0 if compiled, <0 for errors.
 */
 
static int eautc_global(struct eautc *ctx) {
  const char *kw=0;
  int kwc=eautc_next(&kw,ctx);
  if (kwc<=0) return kwc;
  int err;
  if ((kwc==5)&&!memcmp(kw,"tempo",5)) err=eautc_tempo(ctx,kw);
  else if ((kwc==4)&&!memcmp(kw,"chdr",4)) err=eautc_chdr(ctx,kw);
  else if ((kwc==6)&&!memcmp(kw,"events",6)) err=eautc_events(ctx,kw);
  else return eautc_fail(ctx,kw,"Expected 'tempo', 'chdr', or 'events' at global scope. Found '%.*s'.",kwc,kw);
  if (err<0) return err;
  return 1;
}

/* Wrap up after completing the global scope.
 * Emits the "\0EAU" and "TEXT" chunks if we haven't yet.
 */
 
static int eautc_finish(struct eautc *ctx) {

  // "\0EAU" chunk if we don't have it yet. That could only happen in a completely empty file (which is legal).
  int err=eautc_require_lead(ctx);
  if (err<0) return err;
  
  // "TEXT" chunk if our namespace is not empty and we're not stripping.
  if (!ctx->strip_names&&(ctx->ns.c>0)) {
    if (sr_encode_raw(ctx->dst,"TEXT",4)<0) return -1;
    int lenp=ctx->dst->c;
    if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
    const struct eautc_ns_entry *entry=ctx->ns.v;
    int i=ctx->ns.c;
    for (;i-->0;entry++) {
      if (sr_encode_u8(ctx->dst,entry->chid)<0) return -1;
      if (sr_encode_u8(ctx->dst,entry->noteid)<0) return -1;
      if (sr_encode_u8(ctx->dst,entry->c)<0) return -1;
      if (sr_encode_raw(ctx->dst,entry->v,entry->c)<0) return -1;
    }
    int len=ctx->dst->c-lenp-4;
    if (len<0) return eautc_fail(ctx,0,"Invalid TEXT length.");
    ((uint8_t*)ctx->dst->v)[lenp+0]=len>>24;
    ((uint8_t*)ctx->dst->v)[lenp+1]=len>>16;
    ((uint8_t*)ctx->dst->v)[lenp+2]=len>>8;
    ((uint8_t*)ctx->dst->v)[lenp+3]=len;
  }
  
  return 0;
}

/* Compile one complete EAU-Text file, appending to (ctx->dst).
 * (src) is not necessarily (ctx->osrc). We can be re-entered recursively.
 * But it must be contained within (ctx->osrc).
 */
 
static int eautc_file(struct eautc *ctx,const char *src,int srcc) {
  int err=0;
  if ((srcc<0)||(src<ctx->osrc)||(src+srcc>ctx->osrc+ctx->osrcc)) return eautc_fail(ctx,0,"Invalid inner file %p:%d in %p:%d.",src,srcc,ctx->osrc,ctx->osrcc);
  struct eautc subctx={
    .dst=ctx->dst,
    .osrc=ctx->osrc,
    .osrcc=ctx->osrcc,
    .src=src,
    .srcc=srcc,
    .srcp=0,
    .path=ctx->path,
    .error=0,
    .get_chdr=ctx->get_chdr,
    .ns={0},
    .emitted_lead=0,
    .events_done=0,
    .strip_names=ctx->strip_names,
  };
  for (;;) {
    if ((err=eautc_global(&subctx))<0) {
      eautc_cleanup(&subctx);
      return ctx->error=err;
    }
    if (!err) break;
  }
  if ((err=eautc_finish(&subctx))<0) {
    eautc_cleanup(&subctx);
    return ctx->error=err;
  }
  eautc_cleanup(&subctx);
  return 0;
}

/* EAU from EAU-Text, main entry point.
 */
 
int eau_cvt_eau_eaut(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names) {
  // This outermost context doesn't really do anything; eautc_file() creates a new one.
  struct eautc ctx={.dst=dst,.osrc=src,.osrcc=srcc,.src=src,.srcc=srcc,.path=path,.strip_names=strip_names};
  int err=eautc_file(&ctx,src,srcc);
  eautc_cleanup(&ctx);
  return err;
}
