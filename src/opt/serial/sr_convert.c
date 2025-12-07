#include "serial.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* Logging.
 */
 
#define GETMSG \
  char msg[256]; \
  int msgc=0; \
  if (fmt&&fmt[0]) { \
    va_list vargs; \
    va_start(vargs,fmt); \
    msgc=vsnprintf(msg,sizeof(msg),fmt,vargs); \
    if ((msgc<0)||(msgc>sizeof(msg))) msgc=0; \
    while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; \
  }
  
#define DUMPMSG_NOLINE(cls) \
  if (ctx->errmsg) { \
    sr_encode_fmt(ctx->errmsg,"%s:%s: %.*s\n",ctx->refname,cls,msgc,msg); \
  } else { \
    fprintf(stderr,"%s:%s: %.*s\n",ctx->refname,cls,msgc,msg); \
  }

#define DUMPMSG_LINE(cls) \
  int lineno=sr_convert_lineno(ctx,refp); \
  if (ctx->errmsg) { \
    sr_encode_fmt(ctx->errmsg,"%s:%d:%s: %.*s\n",ctx->refname,lineno,cls,msgc,msg); \
  } else { \
    fprintf(stderr,"%s:%d:%s: %.*s\n",ctx->refname,lineno,cls,msgc,msg); \
  }
 
int sr_convert_error(struct sr_convert_context *ctx,const char *fmt,...) {
  if (ctx->error<0) return ctx->error;
  if (!ctx->refname) return ctx->error=-1;
  GETMSG
  DUMPMSG_NOLINE("ERROR")
  return ctx->error=-2;
}

void sr_convert_warning(struct sr_convert_context *ctx,const char *fmt,...) {
  if (!ctx->refname) return;
  GETMSG
  DUMPMSG_NOLINE("WARNING")
}

int sr_convert_error_at(struct sr_convert_context *ctx,const void *refp,const char *fmt,...) {
  if (ctx->error<0) return ctx->error;
  if (!ctx->refname) return ctx->error=-1;
  GETMSG
  DUMPMSG_LINE("ERROR")
  return ctx->error=-2;
}

void sr_convert_warning_at(struct sr_convert_context *ctx,const void *refp,const char *fmt,...) {
  if (!ctx->refname) return;
  GETMSG
  DUMPMSG_LINE("WARNING")
}

#undef GETMSG
#undef DUMPMSG_NOLINE
#undef DUMPMSG_LINE

/* Line number from pointer.
 */

int sr_convert_lineno(const struct sr_convert_context *ctx,const void *refp) {
  if (refp<ctx->src) return 0;
  if ((char*)refp>(char*)ctx->src+ctx->srcc) return 0;
  const char *src=ctx->src;
  int srcc=(char*)refp-(char*)ctx->src;
  int lineno=ctx->lineno0+1;
  for (;srcc-->0;src++) {
    if (*src==0x0a) lineno++;
  }
  return lineno;
}

/* Read options.
 */

int sr_convert_arg(void *dstpp,const struct sr_convert_context *ctx,const char *k,int kc) {
  if (!k) return -1;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (kc<1) return -1;
  int argi=1;
  while (argi<ctx->argc) {
  
    // Skip nulls and blanks.
    const char *arg=ctx->argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // Count dashes. Skip anything with no dashes or all dashes.
    int dashc=0;
    while (*arg=='-') { arg++; dashc++; }
    if (!dashc||!*arg) continue;
    
    // One dash is a single-letter short option.
    if (dashc==1) {
      const char *v=0;
      if (arg[1]) v=arg+1;
      else if ((argi<ctx->argc)&&ctx->argv[argi]&&ctx->argv[argi][0]&&(ctx->argv[argi][0]!='-')) v=ctx->argv[argi++];
      if (!v) v="1";
      if ((kc==1)&&(k[0]==arg[0])) {
        int vc=0;
        while (v[vc]) vc++;
        *(const void**)dstpp=v;
        return vc;
      }
      continue;
    }
    
    // Two or more dashes is a long option.
    const char *v=0;
    int akc=0;
    while (arg[akc]&&(arg[akc]!='=')) akc++;
    if (arg[akc]=='=') v=arg+akc+1;
    else if ((argi<ctx->argc)&&ctx->argv[argi]&&ctx->argv[argi][0]&&(ctx->argv[argi][0]!='-')) v=ctx->argv[argi++];
    int vc=0;
    if (v) {
      while (v[vc]) vc++;
    } else {
      if ((akc>=3)&&!memcmp(arg,"no-",3)) {
        arg+=3;
        akc-=3;
        v="0";
        vc=1;
      } else {
        v="1";
        vc=1;
      }
    }
    if ((kc==akc)&&!memcmp(k,arg,kc)) {
      *(const void**)dstpp=v;
      return vc;
    }
  }
  return -1;
}

int sr_convert_arg_int(int *dst,const struct sr_convert_context *ctx,const char *k,int kc) {
  const char *v=0;
  int vc=sr_convert_arg(&v,ctx,k,kc);
  if (vc<0) return vc;
  if (sr_int_eval(dst,v,vc)>=1) return 1;
  if (!vc) { *dst=0; return 0; }
  if ((vc==2)&&!memcmp(v,"no",2)) { *dst=0; return 1; }
  if ((vc==3)&&!memcmp(v,"yes",3)) { *dst=1; return 1; }
  if ((vc==5)&&!memcmp(v,"false",5)) { *dst=0; return 1; }
  if ((vc==4)&&!memcmp(v,"true",4)) { *dst=1; return 1; }
  return -1;
}

/* argv from http query.
 */
 
int sr_convert_argv_from_http_query(struct sr_convert_context *ctx,const char *src,int srcc) {
  if (ctx->argv||ctx->argc) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc&&(src[0]=='?')) { src++; srcc--; }
  int arga=0,srcp=0;
  while (srcp<srcc) {
    const char *kpre=src+srcp;
    int kprec=0;
    while ((srcp<srcc)&&(src[srcp]!='=')&&(src[srcp]!='&')) { srcp++; kprec++; }
    const char *vpre=0;
    int vprec=0;
    if ((srcp<srcc)&&(src[srcp]=='=')) {
      srcp++;
      vpre=src+srcp;
      while ((srcp<srcc)&&(src[srcp]!='&')) { srcp++; vprec++; }
    }
    struct sr_encoder tmp={0};
    sr_encode_raw(&tmp,"--",2);
    sr_encode_urldecode(&tmp,kpre,kprec); // If (k) contains '=', this will confuse it.
    sr_encode_u8(&tmp,'=');
    sr_encode_urldecode(&tmp,vpre,vprec);
    if (sr_encoder_terminate(&tmp)<0) {
      sr_encoder_cleanup(&tmp);
      sr_convert_free_argv(ctx);
      return -1;
    }
    if (ctx->argc>=arga) {
      int na=arga+4;
      if (na>INT_MAX/sizeof(void*)) {
        sr_encoder_cleanup(&tmp);
        sr_convert_free_argv(ctx);
        return -1;
      }
      void *nv=realloc(ctx->argv,sizeof(void*)*na);
      if (!nv) {
        sr_encoder_cleanup(&tmp);
        sr_convert_free_argv(ctx);
        return -1;
      }
      ctx->argv=nv;
      arga=na;
    }
    if (!ctx->argc) { // Insert a leading null. By convention the first argument must be ignored.
      ctx->argv[ctx->argc++]=0;
    }
    ctx->argv[ctx->argc++]=tmp.v; // HANDOFF
  }
  // In the POSIX world, (argv) itself must always be null-terminated. None of my code expects that, so don't bother.
  return 0;
}

/* Free argv.
 */
 
void sr_convert_free_argv(struct sr_convert_context *ctx) {
  if (ctx->argv) {
    while (ctx->argc-->0) {
      void *v=ctx->argv[ctx->argc];
      if (v) free(v);
    }
    free(ctx->argv);
    ctx->argv=0;
    ctx->argc=0;
  }
}
