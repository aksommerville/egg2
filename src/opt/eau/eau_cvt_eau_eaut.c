#include "eau.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

struct eau_eaut_context;
static int eau_cvt_eau_eaut_inner(struct eau_eaut_context *ctx,const char *src,int srcc);

static int eau_lineno(const char *src,int srcc) {
  if (!src||(srcc<1)) return 1;
  int lineno=1,srcp=0;
  for (;srcp<srcc;srcp++) if (src[srcp]==0x0a) lineno++;
  return lineno;
}

/* Context.
 */
 
struct eau_eaut_context {
  struct sr_encoder *dst;
  const char *path;
  const char *src;
  int srcc;
  int logged_error;
};

static void eau_eaut_context_cleanup(struct eau_eaut_context *ctx) {
}

/* Tokenizer.
 * We are a recursive language, so there will be multiple tokenizers in play, each at its own level of the call stack.
 * A token is one of:
 *  - Space-delimited number, keyword, etc.
 *  - Quoted string.
 *  - Parenthesized argument list (the whole construction).
 *  - Bracketted statement list (the whole construction).
 *  - A single punctuation char, eg quote if string measurement fails.
 * Tokenizer skips whitespace and comments for you.
 * Initialize (v,c) with (p==0). No cleanup necessary.
 * No errors. Returns zero at EOF.
 */
 
struct eaut_tokenizer {
  const char *v;
  int c,p;
};

int eaut_tokenizer_next(void *dstpp,struct eaut_tokenizer *t) {
  for (;;) {
  
    // Skippables.
    if (t->p>=t->c) return 0;
    if ((unsigned char)t->v[t->p]<=0x20) {
      t->p++;
      continue;
    }
    if (t->v[t->p]=='#') {
      while (t->v[t->p++]!=0x0a) ;
      continue;
    }
    *(const void**)dstpp=t->v+t->p;
    int c=1;
    
    // String.
    if (t->v[t->p]=='"') {
      if ((c=sr_string_measure(t->v+t->p,t->c-t->p,0))<1) c=1;
      t->p+=c;
      return c;
    }
    
    // Parens.
    if (t->v[t->p]=='(') {
      int depth=1;
      for (;;) {
        if (t->p>=t->c-c) return 1;
        if (t->v[t->p+c]==')') {
          depth--;
          if (!depth) {
            c++;
            break;
          }
        } else if (t->v[t->p+c]=='(') {
          depth++;
        }
        c++;
      }
      t->p+=c;
      return c;
    }
    
    // Blocks.
    if (t->v[t->p]=='{') {
      struct eaut_tokenizer sub={.v=t->v,.c=t->c,.p=t->p+1};
      const char *token;
      for (;;) {
        int tokenc=eaut_tokenizer_next(&token,&sub);
        if (tokenc<1) {
          t->p++;
          return 1;
        }
        if ((tokenc==1)&&(token[0]=='}')) {
          tokenc=sub.p-t->p;
          t->p=sub.p;
          return tokenc;
        }
      }
    }
    
    // A few explicit single-byte tokens.
    if (
      (t->v[t->p]=='(')||
      (t->v[t->p]=='{')||
      (t->v[t->p]==',')||
      (t->v[t->p]=='}')||
      (t->v[t->p]==')')
    ) {
      t->p+=1;
      return 1;
    }
    
    // Space-delimited token.
    while (t->p<t->c-c) {
      char ch=t->v[t->p+c];
      if ((unsigned char)ch<=0x20) break;
      if (ch=='(') break;
      if (ch=='{') break;
      if (ch==',') break;
      c++;
    }
    t->p+=c;
    return c;
  }
}

/* Log error.
 */

static int fail(struct eau_eaut_context *ctx,const char *loc,const char *fmt,...) {
  if (ctx->logged_error) return -2;
  if (!ctx->path) return -1;
  ctx->logged_error=1;
  int lineno=0;
  if (loc) {
    int p=loc-ctx->src;
    if ((p>=0)&&(p<=ctx->srcc)) lineno=eau_lineno(ctx->src,p);
  }
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  if (lineno) fprintf(stderr,"%s:%d: %.*s\n",ctx->path,lineno,msgc,msg);
  else fprintf(stderr,"%s: %.*s\n",ctx->path,msgc,msg);
  return -2;
}

/* Split a parameter list, integers only.
 * (src) are comma-delimited tokens with no outer parens.
 * We fail if there's more than (dsta) args, but a short count is allowed.
 */
 
static int eau_eaut_params(int *dstv,int dsta,struct eau_eaut_context *ctx,const char *src,int srcc) {
  struct eaut_tokenizer t={.v=src,.c=srcc};
  int dstc=0;
  for (;;) {
    const char *token;
    int tokenc=eaut_tokenizer_next(&token,&t);
    if (tokenc<1) return dstc;
    // Don't bother checking that commas are in the right place (or even present). They're decorative.
    if ((tokenc==1)&&(token[0]==',')) continue;
    if (dstc>=dsta) return fail(ctx,token,"Expected no more than %d parameters.",dsta);
    // Anything else must be an integer.
    if (sr_int_eval(dstv+dstc,token,tokenc)<2) {
      return fail(ctx,token,"Failed to evaluate '%.*s' as integer.",tokenc,token);
    }
    dstc++;
  }
}

/* Functions.
 */
    
static int eau_eaut_fn_u8(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v,err;
  if ((err=eau_eaut_params(&v,1,ctx,params,paramc))<0) return err;
  if (!err||(v<0)||(v>0xff)) return fail(ctx,params,"Expected 0..255.");
  sr_encode_intbe(ctx->dst,v,1);
  return 0;
}

static int eau_eaut_fn_u16(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v,err;
  if ((err=eau_eaut_params(&v,1,ctx,params,paramc))<0) return err;
  if (!err||(v<0)||(v>0xffff)) return fail(ctx,params,"Expected 0..65535.");
  sr_encode_intbe(ctx->dst,v,2);
  return 0;
}

static int eau_eaut_fn_u24(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v,err;
  if ((err=eau_eaut_params(&v,1,ctx,params,paramc))<0) return err;
  if (!err||(v<0)||(v>0xffffff)) return fail(ctx,params,"Expected 0..16777215.");
  sr_encode_intbe(ctx->dst,v,3);
  return 0;
}

static int eau_eaut_fn_u32(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v,err;
  if ((err=eau_eaut_params(&v,1,ctx,params,paramc))<0) return err;
  if (!err) return fail(ctx,params,"Expected integer.");
  sr_encode_intbe(ctx->dst,v,4);
  return 0;
}

static int eau_eaut_fn_name(struct eau_eaut_context *ctx,const char *params,int paramc) {
  // Noop. We could verify that there's one argument and it's a string, but whatever.
  return 0;
}

static int eau_eaut_fn_delay(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int ms,err;
  if ((err=eau_eaut_params(&ms,1,ctx,params,paramc))<0) return err;
  if (!err||(ms<0)) return fail(ctx,params,"Expected positive integer.");
  while (ms>=4096) {
    sr_encode_u8(ctx->dst,0x7f);
    ms-=4096;
  }
  if (ms>=64) {
    sr_encode_u8(ctx->dst,0x40|((ms>>6)-1));
    ms&=0x3f;
  }
  if (ms>0) {
    sr_encode_u8(ctx->dst,ms);
  }
  return 0;
}

static int eau_eaut_fn_note(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v[4],err;
  if ((err=eau_eaut_params(v,4,ctx,params,paramc))<0) return err;
  if (err!=4) return fail(ctx,params,"Expected (chid,noteid,velocity,durms).");
  int chid=v[0]; if ((chid<0)||(chid>15)) return fail(ctx,params,"Expected chid in 0..15, found %d.",chid);
  int noteid=v[1]; if ((noteid<0)||(noteid>127)) return fail(ctx,params,"Expected noteid in 0..127, found %d.",noteid);
  int velocity=v[2]; if ((velocity<0)||(velocity>127)) return fail(ctx,params,"Expected velocity in 0..127, found %d.",velocity);
  int durms=v[3]; if (durms<0) return fail(ctx,params,"The nature of time currently precludes negative duration.");
  // Unlike the other limits, duration limit is not obvious and may well go over if we were converted from MIDI originally.
  // Also, it's fairly reasonable just to clamp it to the limit when over.
  // So warn, don't fail.
  if (durms>16380) {
    if (ctx->path) {
      int lineno=0;
      int p=params-ctx->src;
      if ((p>=0)&&(p<=ctx->srcc)) lineno=eau_lineno(ctx->src,p);
      fprintf(stderr,"%s:%d:WARNING: Clamping duration %d ms to the limit 16380 ms.\n",ctx->path,lineno,durms);
    }
    durms=16380;
  }
  durms>>=2;
  sr_encode_u8(ctx->dst,0x80|(chid<<2)|(noteid>>5));
  sr_encode_u8(ctx->dst,(noteid<<3)|(velocity>>4));
  sr_encode_u8(ctx->dst,(velocity<<4)|(durms>>8));
  sr_encode_u8(ctx->dst,durms);
  return 0;
}

static int eau_eaut_fn_wheel(struct eau_eaut_context *ctx,const char *params,int paramc) {
  int v[2],err;
  if ((err=eau_eaut_params(v,2,ctx,params,paramc))<0) return err;
  if (err!=2) return fail(ctx,params,"Expected (chid,wheel).");
  int chid=v[0]; if ((chid<0)||(chid>15)) return fail(ctx,params,"Expected chid in 0..15, found %d.",chid);
  int wheel=v[1]; if ((wheel<-512)||(wheel>=512)) return fail(ctx,params,"Expected wheel in -512..511, found %d.",wheel);
  wheel+=512;
  sr_encode_u8(ctx->dst,0xc0|(chid<<2)|(wheel>>8));
  sr_encode_u8(ctx->dst,wheel);
  return 0;
}

/* len(SIZE) { ... }
 */
 
static int eau_eaut_fn_len(struct eau_eaut_context *ctx,const char *params,int paramc,const char *body,int bodyc) {
  int size,err;
  if ((err=eau_eaut_params(&size,1,ctx,params,paramc))<0) return err;
  if ((err!=1)||(size<1)||(size>4)) return fail(ctx,params,"Expected length field size in bytes (1..4).");
  sr_encode_raw(ctx->dst,"\0\0\0\0",size);
  int dstc0=ctx->dst->c;
  if ((err=eau_cvt_eau_eaut_inner(ctx,body,bodyc))<0) return err;
  int len=ctx->dst->c-dstc0;
  if ((len<0)||((size<4)&&(len>=1<<(size*8)))) return fail(ctx,body,"Invalid block length %d (%d-byte size).",len,size);
  int i=size; while (i-->0) {
    ((uint8_t*)ctx->dst->v)[--dstc0]=len;
    len>>=8;
  }
  return 0;
}

/* Main, in context.
 * (src,srcc) should be contained within (ctx->src). But it won't cause an error if not.
 */
 
static int eau_cvt_eau_eaut_inner(struct eau_eaut_context *ctx,const char *src,int srcc) {
  int err;
  struct eaut_tokenizer t={.v=src,.c=srcc};
  for (;;) {
    const char *token=0;
    int tokenc=eaut_tokenizer_next(&token,&t);
    if (tokenc<1) return 0;
    
    // String?
    if ((tokenc>=2)&&(token[0]=='"')&&(token[tokenc-1]=='"')) {
      for (;;) {
        if ((err=sr_string_eval((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,token,tokenc))<0) {
          return fail(ctx,token,"Malformed string token.");
        }
        if (ctx->dst->c<=ctx->dst->a-err) {
          ctx->dst->c+=err;
          break;
        }
        if (sr_encoder_require(ctx->dst,err)<0) return -1;
      }
      continue;
    }
    
    // "len" is a special function, it's the only time brackets are used.
    if ((tokenc==3)&&!memcmp(token,"len",3)) {
      const char *params=token;
      int paramsc=eaut_tokenizer_next(&params,&t);
      if ((paramsc<2)||(params[0]!='(')||(params[paramsc-1]!=')')) {
        return fail(ctx,params,"Expected parenthesized parameters after 'len'.");
      }
      const char *body=params;
      int bodyc=eaut_tokenizer_next(&body,&t);
      if ((bodyc<2)||(body[0]!='{')||(body[bodyc-1]!='}')) {
        return fail(ctx,body,"Expected bracketted body after 'len(...)'.");
      }
      if ((err=eau_eaut_fn_len(ctx,params+1,paramsc-2,body+1,bodyc-2))<0) return err;
      continue;
    }
    
    // Function or symbol? Must check for these before general hexdump.
    #define FN(tag) if ((tokenc==sizeof(#tag)-1)&&!memcmp(token,#tag,tokenc)) { \
      const char *params=token; \
      int paramsc=eaut_tokenizer_next(&params,&t); \
      if ((paramsc<2)||(params[0]!='(')||(params[paramsc-1]!=')')) { \
        return fail(ctx,params,"Expected parenthesized parameters after '%.*s'.",tokenc,token); \
      } \
      if ((err=eau_eaut_fn_##tag(ctx,params+1,paramsc-2))<0) return err; \
      continue; \
    }
    FN(u8)
    FN(u16)
    FN(u24)
    FN(u32)
    FN(name)
    FN(delay)
    FN(note)
    FN(wheel)
    #undef FN
    #define SYM(tag,v) if ((tokenc==sizeof(tag)-1)&&!memcmp(token,tag,tokenc)) { \
      sr_encode_raw(ctx->dst,v,sizeof(v)-1); \
      continue; \
    }
    SYM("MODE_NOOP","\0")
    SYM("MODE_DRUM","\1")
    SYM("MODE_FM","\2")
    SYM("MODE_HARSH","\3")
    SYM("MODE_HARM","\4")
    SYM("POST_NOOP","\0")
    SYM("POST_DELAY","\1")
    SYM("POST_WAVESHAPER","\2")
    SYM("POST_TREMOLO","\3")
    SYM("DEFAULT_ENV","\0\0")
    SYM("SHAPE_SINE","\0")
    SYM("SHAPE_SQUARE","\1")
    SYM("SHAPE_SAW","\2")
    SYM("SHAPE_TRIANGLE","\3")
    #undef SYM
    
    // Any other token must be composed entirely of hexadecimal digits, an even count.
    if (!(tokenc&1)) {
      // Allow it to start "0x" in case I Freudian slip that in.
      if ((tokenc>2)&&(!memcmp(token,"0x",2)||!memcmp(token,"0X",2))) {
        token+=2;
        tokenc-=2;
      }
      int tp=0; while (tp<tokenc) {
        int hi=sr_digit_eval(token[tp++]);
        int lo=sr_digit_eval(token[tp++]);
        // "Unexpected token" if any digit is wrong, rather than calling out the specific digit.
        // They're more likely to have misspelled a function name than to have forgotten which digits are legal in a hex dump.
        if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) goto _unexpected_;
        sr_encode_u8(ctx->dst,(hi<<4)|lo);
      }
      continue;
    }
    
   _unexpected_:;
    return fail(ctx,token,"Unexpected token '%.*s'.",tokenc,token);
  }
  return 0;
}

/* EAU from EAU-Text, main entry point.
 */
 
int eau_cvt_eau_eaut(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr) {
  struct eau_eaut_context ctx={.dst=dst,.src=src,.srcc=srcc,.path=path};
  int err=eau_cvt_eau_eaut_inner(&ctx,src,srcc);
  eau_eaut_context_cleanup(&ctx);
  return err;
}
