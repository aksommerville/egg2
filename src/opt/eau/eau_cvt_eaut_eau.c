#include "eau.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

struct eautd;
static int eautd_inner(struct eautd *ctx,const uint8_t *src,int srcc);

/* Namespace.
 * All strings are WEAK.
 */
 
struct eautd_ns {
  struct eautd_ns_entry {
    int chid,noteid; // noteid zero for the channel's name; note zero may exist but can't be named.
    const char *v;
    int c;
  } *v;
  int c,a;
};

static void eautd_ns_cleanup(struct eautd_ns *ns) {
  if (ns->v) free(ns->v);
}

static int eautd_ns_search(const struct eautd_ns *ns,int chid,int noteid) {
  int lo=0,hi=ns->c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eautd_ns_entry *q=ns->v+ck;
         if (chid<q->chid) hi=ck;
    else if (chid>q->chid) lo=ck+1;
    else if (noteid<q->noteid) hi=ck;
    else if (noteid>q->noteid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

// Does not add empties and does not replace existing. (noop success in those cases).
static int eautd_ns_add(struct eautd_ns *ns,int chid,int noteid,const char *v/*BORROW*/,int c) {
  if (!v) return 0;
  if (c<0) { c=0; while (v[c]) c++; }
  if (!c) return 0;
  int p=eautd_ns_search(ns,chid,noteid);
  if (p>=0) return 0;
  p=-p-1;
  if (ns->c>=ns->a) {
    int na=ns->a+128;
    if (na>INT_MAX/sizeof(struct eautd_ns_entry)) return -1;
    void *nv=realloc(ns->v,sizeof(struct eautd_ns_entry)*na);
    if (!nv) return -1;
    ns->v=nv;
    ns->a=na;
  }
  struct eautd_ns_entry *entry=ns->v+p;
  memmove(entry+1,entry,sizeof(struct eautd_ns_entry)*(ns->c-p));
  ns->c++;
  entry->chid=chid;
  entry->noteid=noteid;
  entry->v=v;
  entry->c=c;
  return 0;
}

/* Context.
 */
 
struct eautd {
  struct sr_encoder *dst;
  const char *path;
  int error;
  struct eautd_ns ns;
  int indent;
};

static void eautd_cleanup(struct eautd *ctx) {
  eautd_ns_cleanup(&ctx->ns);
}

/* Logging.
 */
 
static int eautd_fail(struct eautd *eautd,const char *fmt,...) {
  if (eautd->error<0) return eautd->error;
  if (!eautd->path||!fmt) return eautd->error=-1;
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&(msg[msgc-1]==0x0a)) msgc--;
  fprintf(stderr,"%s: %.*s\n",eautd->path,msgc,msg);
  return eautd->error=-2;
}
 
static void eautd_warn(struct eautd *eautd,const char *fmt,...) {
  if (!eautd->path||!fmt) return;
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&(msg[msgc-1]==0x0a)) msgc--;
  fprintf(stderr,"%s:WARNING: %.*s\n",eautd->path,msgc,msg);
}

/* Append output, managing indent and newline.
 * To assemble a line piecemeal, operate directly on (ctx->dst).
 */
 
static int eautd_indent(struct eautd *ctx) {
  char spaces[]="                                   ";
  int c=ctx->indent;
  if (c<0) c=0; else if (c>=sizeof(spaces)) c=sizeof(spaces)-1;
  return sr_encode_raw(ctx->dst,spaces,c);
}
 
static int eautd_out(struct eautd *ctx,const char *fmt,...) {
  if (eautd_indent(ctx)<0) return -1;
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  if (sr_encode_fmt(ctx->dst,"%.*s\n",msgc,msg)<0) return -1;
  return 0;
}

/* Single post stage.
 */
 
static int eautd_post(struct eautd *ctx,int stageid,const uint8_t *src,int srcc) {
  switch (stageid) {
  
    case 1: {
        int period=0x0100,dry=0x80,wet=0x80,sto=0x80,fbk=0x80,sparkle=0x80;
        if (srcc==1) return eautd_fail(ctx,"Invalid length %d for delay post stage.",srcc);
        int srcp=0;
        if (srcp<=srcc-2) { period=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
        if (srcp<srcc) dry=src[srcp++];
        if (srcp<srcc) wet=src[srcp++];
        if (srcp<srcc) sto=src[srcp++];
        if (srcp<srcc) fbk=src[srcp++];
        if (srcp<srcc) sparkle=src[srcp++];
        if (eautd_out(ctx,"delay 0x%04x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x;",period,dry,wet,sto,fbk,sparkle)<0) return -1;
      } break;
      
    case 2: {
        if (srcc&1) return eautd_fail(ctx,"Invalid length %d for waveshaper payload; must be even.",srcc);
        if (eautd_indent(ctx)<0) return -1;
        if (sr_encode_raw(ctx->dst,"waveshaper",10)<0) return -1;
        int srcp=0; for (;srcp<srcc;srcp+=2) {
          int v=(src[srcp]<<8)|src[srcp+1];
          if (sr_encode_fmt(ctx->dst," 0x%04x",v)<0) return -1;
        }
        if (sr_encode_raw(ctx->dst,";\n",2)<0) return -1;
      } break;
      
    case 3: {
        int period=0x0100,depth=0xff,phase=0;
        if (srcc==1) return eautd_fail(ctx,"Invalid length %d for tremolo post stage.",srcc);
        int srcp=0;
        if (srcp<=srcc-2) { period=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
        if (srcp<srcc) depth=src[srcp++];
        if (srcp<srcc) phase=src[srcp++];
        if (eautd_out(ctx,"tremolo 0x%04x 0x%02x 0x%02x;",period,depth,phase)<0) return -1;
      } break;

    default: {
        if (eautd_indent(ctx)<0) return -1;
        if (sr_encode_fmt(ctx->dst,"%d",stageid)<0) return -1;
        if (srcc) {
          if (sr_encode_raw(ctx->dst," 0x",2)<0) return -1;
          int srcp=0; for (;srcp<srcc;srcp++) {
            if (sr_encode_fmt(ctx->dst,"%02x",src[srcp])<0) return -1;
          }
        }
        if (sr_encode_raw(ctx->dst,";\n",2)<0) return -1;
      }
  }
  return 0;
}

/* Enter an inner file.
 */
 
static int eautd_file(struct eautd *ctx,const uint8_t *src,int srcc) {
  struct eautd subctx=*ctx;
  memset(&subctx.ns,0,sizeof(struct eautd_ns));
  int err=eautd_inner(&subctx,src,srcc);
  eautd_cleanup(&subctx);
  return err;
}

/* Consume and emit one scalar field.
 */
 
static int eautd_scalar(struct eautd *ctx,const char *name,const uint8_t *src,int srcc,int size) {
  if (srcc<1) return 0;
  if (srcc<size) return eautd_fail(ctx,"Unexpected EOF around field '%s'.",name);
  int v=0,i=size;
  for (;i-->0;src++) {
    v<<=8;
    v|=*src;
  }
  if (eautd_out(ctx,"%s 0x%x;",name,v)<0) return -1;
  return size;
}

/* Consume and emit one envelope.
 */
 
static int eautd_env(struct eautd *ctx,const char *name,const uint8_t *src,int srcc) {

  // Empty or (0,0) means default, don't emit anything.
  if (srcc<1) return 0;
  if ((srcc>=2)&&!src[0]&&!src[1]) return 2;
  
  /* Read everything into a temporary model.
   */
  int srcp=0,initlo=0,inithi=0;
  struct point { int tlo,thi,vlo,vhi; } pointv[15];
  int flags=src[srcp++];
  if (flags&0x01) { // Initials.
    if (srcp>srcc-2) return eautd_fail(ctx,"Unexpected EOF in envelope.");
    initlo=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    if (flags&0x02) { // Velocity.
      if (srcp>srcc-2) return eautd_fail(ctx,"Unexpected EOF in envelope.");
      inithi=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    } else {
      inithi=initlo;
    }
  }
  if (srcp>=srcc) return eautd_fail(ctx,"Unexpected EOF in envelope.");
  int susp_ptc=src[srcp++];
  int susp=(flags&0x04)?(susp_ptc>>4):-1;
  int pointc=susp_ptc&15;
  int ptlen=(flags&0x02)?8:4;
  if (srcp>srcc-pointc*ptlen) return eautd_fail(ctx,"Unexpected EOF in envelope.");
  struct point *point=pointv;
  int i=0; for (;i<pointc;i++,point++) {
    point->tlo=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    point->vlo=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    if (flags&0x02) {
      point->thi=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
      point->vhi=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    }
  }
  
  /* Emit.
   */
  if (eautd_indent(ctx)<0) return -1;
  if (sr_encode_raw(ctx->dst,name,-1)<0) return -1;
  if (flags&0x01) {
    if (initlo!=inithi) {
      if (sr_encode_fmt(ctx->dst," =0x%04x..0x%04x",initlo,inithi)<0) return -1;
    } else {
      if (sr_encode_fmt(ctx->dst," =0x%04x",initlo)<0) return -1;
    }
  }
  for (i=0,point=pointv;i<pointc;i++,point++) {
    if (point->tlo!=point->thi) {
      if (sr_encode_fmt(ctx->dst," +%d..%d",point->tlo,point->thi)<0) return -1;
    } else {
      if (sr_encode_fmt(ctx->dst," +%d",point->tlo)<0) return -1;
    }
    const char *sus=(i==susp)?"*":"";
    if (point->vlo!=point->vhi) {
      if (sr_encode_fmt(ctx->dst," =0x%04x..0x%04x%s",point->vlo,point->vhi,sus)<0) return -1;
    } else {
      if (sr_encode_fmt(ctx->dst," =0x%04x%s",point->vlo,sus)<0) return -1;
    }
  }
  if (sr_encode_raw(ctx->dst,";\n",2)<0) return -1;
  return srcp;
}

/* Consume and emit harmonics list.
 */
 
static int eautd_harmonics(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  int srcp=0;
  int harmc=src[srcp++];
  if (srcp>srcc-harmc*2) return eautd_fail(ctx,"Unexpected EOF in harmonics.");
  if (eautd_indent(ctx)<0) return -1;
  if (sr_encode_raw(ctx->dst,"harmonics",-1)<0) return -1;
  int i=harmc; for (;i-->0;srcp+=2) {
    int v=(src[srcp]<<8)|src[srcp+1];
    if (sr_encode_fmt(ctx->dst," 0x%04x",v)<0) return -1;
  }
  if (sr_encode_raw(ctx->dst,";\n",2)<0) return -1;
  return srcp;
}

/* modecfg for drum
 */
 
static int eautd_modecfg_drum(struct eautd *ctx,const uint8_t *src,int srcc,int chid) {
  if (eautd_out(ctx,"modecfg {")<0) return -1;
  ctx->indent+=2;
  int srcp=0,err;
  while (srcp<srcc) {
    if (srcp>srcc-6) return eautd_fail(ctx,"Unexpected EOF in drum list.");
    int noteid=src[srcp++];
    int trimlo=src[srcp++];
    int trimhi=src[srcp++];
    int pan=src[srcp++];
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return eautd_fail(ctx,"Unexpected EOF in drum list.");
    const uint8_t *serial=src+srcp;
    srcp+=len;
    if (eautd_out(ctx,"drum {")<0) return -1;
    ctx->indent+=2;
    if (eautd_out(ctx,"noteid 0x%02x;",noteid)<0) return -1;
    
    int namep=eautd_ns_search(&ctx->ns,chid,noteid);
    if (namep>0) {
      const struct eautd_ns_entry *entry=ctx->ns.v+namep;
      char tmp[256];
      int tmpc=sr_string_repr(tmp,sizeof(tmp),entry->v,entry->c);
      if ((tmpc<0)||(tmpc>sizeof(tmp))) return eautd_fail(ctx,"Malformed drum name.");
      if (eautd_out(ctx,"name %.*s;",tmpc,tmp)<0) return -1;
    }
    
    if (eautd_out(ctx,"trim 0x%02x 0x%02x;",trimlo,trimhi)<0) return -1;
    if (eautd_out(ctx,"pan 0x%02x;",pan)<0) return -1;
    if (len) {
      if (eautd_out(ctx,"serial {")<0) return -1;
      ctx->indent+=2;
      if ((err=eautd_file(ctx,serial,len))<0) return err;
      ctx->indent-=2;
      if (eautd_out(ctx,"}")<0) return -1;
    }
    ctx->indent-=2;
    if (eautd_out(ctx,"}")<0) return -1;
  }
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* modecfg for fm
 */
 
static int eautd_modecfg_fm(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (eautd_out(ctx,"modecfg {")<0) return -1;
  ctx->indent+=2;
  int srcp=0,err;
  if (srcp<=srcc-2) {
    int rate=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (rate&0x8000) {
      if (eautd_out(ctx,"absrate 0x%04x;",rate&0x7fff)<0) return -1;
    } else {
      if (eautd_out(ctx,"rate 0x%04x;",rate)<0) return -1;
    }
  }
  if ((err=eautd_scalar(ctx,"range",src+srcp,srcc-srcp,2))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"levelenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"rangeenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"pitchenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"wheelrange",src+srcp,srcc-srcp,2))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"lforate",src+srcp,srcc-srcp,2))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"lfodepth",src+srcp,srcc-srcp,1))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"lfophase",src+srcp,srcc-srcp,1))<0) return err; srcp+=err;
  if (srcp<srcc) eautd_warn(ctx,"Discarding %d bytes of FM modecfg.",srcc-srcp);
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* modecfg for harsh
 */
 
static int eautd_modecfg_harsh(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (eautd_out(ctx,"modecfg {")<0) return -1;
  ctx->indent+=2;
  int srcp=0,err;
  if (srcp<srcc) {
    int shape=src[srcp++];
    switch (shape) {
      case 0: err=eautd_out(ctx,"shape sine;"); break;
      case 1: err=eautd_out(ctx,"shape square;"); break;
      case 2: err=eautd_out(ctx,"shape saw;"); break;
      case 3: err=eautd_out(ctx,"shape triangle;"); break;
      default: err=eautd_out(ctx,"shape %d;",shape);
    }
    if (err<0) return err;
  }
  if ((err=eautd_env(ctx,"levelenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"pitchenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"wheelrange",src+srcp,srcc-srcp,2))<0) return err; srcp+=err;
  if (srcp<srcc) eautd_warn(ctx,"Discarding %d bytes of HARSH modecfg.",srcc-srcp);
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* modecfg for harm
 */
 
static int eautd_modecfg_harm(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (eautd_out(ctx,"modecfg {")<0) return -1;
  ctx->indent+=2;
  int srcp=0,err;
  if ((err=eautd_harmonics(ctx,src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"levelenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_env(ctx,"pitchenv",src+srcp,srcc-srcp))<0) return err; srcp+=err;
  if ((err=eautd_scalar(ctx,"wheelrange",src+srcp,srcc-srcp,2))<0) return err; srcp+=err;
  if (srcp<srcc) eautd_warn(ctx,"Discarding %d bytes of HARM modecfg.",srcc-srcp);
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* generic modecfg
 */
 
static int eautd_modecfg_generic(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (eautd_indent(ctx)<0) return -1;
  if (sr_encode_raw(ctx->dst,"modecfg 0x",-1)<0) return -1;
  int srcp=0; for (;srcp<srcc;srcp++) {
    if (sr_encode_fmt(ctx->dst,"%02x",src[srcp])<0) return -1;
  }
  if (sr_encode_raw(ctx->dst,";\n",2)<0) return -1;
  return 0;
}

/* CHDR.
 */
 
static int eautd_chdr(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (srcc<1) return 0; // Empty CHDR is legal but noop. Ignore it.
  int srcp=0,err;
  int chid=src[srcp++];
  int trim=0x40,pan=0x80,mode=2,modecfgc=0,postc=0;
  const uint8_t *modecfg=0,*post=0;
  if (srcp<srcc) trim=src[srcp++];
  if (srcp<srcc) pan=src[srcp++];
  if (srcp<srcc) mode=src[srcp++];
  if (srcp<=srcc-2) {
    modecfgc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-modecfgc) return eautd_fail(ctx,"Unexpected EOF in CHDR.");
    modecfg=src+srcp;
    srcp+=modecfgc;
  }
  if (srcp<=srcc-2) {
    postc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-postc) return eautd_fail(ctx,"Unexpected EOF in CHDR.");
    post=src+srcp;
    srcp+=postc;
  }
  
  const char *name=0;
  int namec=0;
  int nsp=eautd_ns_search(&ctx->ns,chid,0);
  if (nsp>=0) {
    const struct eautd_ns_entry *entry=ctx->ns.v+nsp;
    name=entry->v;
    namec=entry->c;
  }
  
  if (eautd_out(ctx,"chdr {")<0) return -1;
  ctx->indent+=2;
  if (eautd_out(ctx,"chid 0x%02x;",chid)<0) return -1;
  if (name) {
    char tmp[256];
    int tmpc=sr_string_repr(tmp,sizeof(tmp),name,namec);
    if ((tmpc>2)&&(tmpc<=sizeof(tmp))) {
      if (eautd_out(ctx,"name %.*s;",tmpc,tmp)<0) return -1;
    }
  }
  if ((trim!=0x40)&&(eautd_out(ctx,"trim 0x%02x;",trim)<0)) return -1;
  if ((pan!=0x80)&&(eautd_out(ctx,"pan 0x%02x;",pan)<0)) return -1;
  switch (mode) {
    case 0: if (eautd_out(ctx,"mode noop;")<0) return -1; break;
    case 1: if (eautd_out(ctx,"mode drum;")<0) return -1; break;
    case 2: if (eautd_out(ctx,"mode fm;")<0) return -1; break;
    case 3: if (eautd_out(ctx,"mode harsh;")<0) return -1; break;
    case 4: if (eautd_out(ctx,"mode harm;")<0) return -1; break;
    default: if (eautd_out(ctx,"mode %d;",mode)<0) return -1;
  }
  
  if (modecfgc) {
    switch (mode) {
      case 1: err=eautd_modecfg_drum(ctx,modecfg,modecfgc,chid); break;
      case 2: err=eautd_modecfg_fm(ctx,modecfg,modecfgc); break;
      case 3: err=eautd_modecfg_harsh(ctx,modecfg,modecfgc); break;
      case 4: err=eautd_modecfg_harm(ctx,modecfg,modecfgc); break;
      default: err=eautd_modecfg_generic(ctx,modecfg,modecfgc); break;
    }
  }
  
  if (postc) {
    if (eautd_out(ctx,"post {")<0) return -1;
    ctx->indent+=2;
    int postp=0;
    while (postp<postc) {
      if (postp>postc-2) return eautd_fail(ctx,"Unexpected EOF in CHDR post.");
      int stageid=post[postp++];
      int len=post[postp++];
      if (postp>postc-len) return eautd_fail(ctx,"Unexpected EOF in CHDR post.");
      if ((err=eautd_post(ctx,stageid,post+postp,len))<0) return err;
      postp+=len;
    }
    ctx->indent-=2;
    if (eautd_out(ctx,"}")<0) return -1;
  }
  
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* EVTS.
 */
 
static int eautd_events(struct eautd *ctx,const uint8_t *src,int srcc) {
  if (eautd_out(ctx,"events {")<0) return -1;
  ctx->indent+=2;
  struct eau_event_reader reader={.v=src,.c=srcc};
  struct eau_event event;
  int err;
  while ((err=eau_event_reader_next(&event,&reader))>0) {
    switch (event.type) {
      case 'd': if (eautd_out(ctx,"delay %d;",event.delay)<0) return -1; break;
      case 'n': if (eautd_out(ctx,"note %d %d %d %d;",event.note.chid,event.note.noteid,event.note.velocity,event.note.durms)<0) return -1; break;
      case 'w': if (eautd_out(ctx,"wheel %d %d;",event.wheel.chid,event.wheel.v+512)<0) return -1; break;
    }
  }
  if (err<0) return eautd_fail(ctx,"Malformed event stream around %d/%d.",reader.p,srcc);
  ctx->indent-=2;
  if (eautd_out(ctx,"}")<0) return -1;
  return 0;
}

/* Full file, in context.
 */
 
static int eautd_inner(struct eautd *ctx,const uint8_t *src,int srcc) {
  // We can't use eau_file_decode() because it restricts to 16 channels, and we must support larger ones (eg standard instruments).
  
  /* First pass, read the introducer chunk and locate EVTS and TEXT.
   * Don't touch CHDR yet because we need the text first.
   */
  struct eau_file_reader reader={.v=src,.c=srcc};
  struct eau_file_chunk chunk;
  int err;
  int tempo=500,loopp=0;
  const uint8_t *evts=0,*text=0;
  int evtsc=0,textc=0;
  while ((err=eau_file_reader_next(&chunk,&reader))>0) {
    if (!memcmp(chunk.id,"\0EAU",4)) {
      if (chunk.c>=2) {
        tempo=(((uint8_t*)chunk.v)[0]<<8)|((uint8_t*)chunk.v)[1];
      }
      if (chunk.c>=4) {
        loopp=(((uint8_t*)chunk.v)[2]<<8)|((uint8_t*)chunk.v)[3];
      }
    } else if (!memcmp(chunk.id,"EVTS",4)) {
      if (evts) return eautd_fail(ctx,"Multiple EVTS.");
      evts=chunk.v;
      evtsc=chunk.c;
    } else if (!memcmp(chunk.id,"TEXT",4)) {
      if (text) return eautd_fail(ctx,"Multiple TEXT.");
      text=chunk.v;
      textc=chunk.c;
    }
  }
  if (err<0) return eautd_fail(ctx,"Malformed EAU framing.");
  if (loopp>evtsc) return eautd_fail(ctx,"Invalid loopp %d for EVTS length %d.",loopp,evtsc);
  
  /* If there's a TEXT chunk, populate our namespace.
   */
  if (text) {
    int textp=0;
    while (textp<textc) {
      if (textp>textc-3) return eautd_fail(ctx,"Unexpected EOF in TEXT chunk.");
      int chid=text[textp++];
      int noteid=text[textp++];
      int len=text[textp++];
      if (textp>textc-len) return eautd_fail(ctx,"Unexpected EOF in TEXT chunk.");
      const char *v=(char*)text+textp;
      textp+=len;
      if (eautd_ns_add(&ctx->ns,chid,noteid,v,len)<0) return -1;
    }
  }
  
  /* Begin output. Tempo, if not 500.
   */
  if (tempo!=500) {
    if (eautd_out(ctx,"tempo %d;",tempo)<0) return -1;
  }
  
  /* Run thru the chunks again and emit each CHDR.
   */
  reader.p=0;
  while (eau_file_reader_next(&chunk,&reader)>0) {
    if (memcmp(chunk.id,"CHDR",4)) continue;
    if ((err=eautd_chdr(ctx,chunk.v,chunk.c))<0) return err;
  }
  
  /* Events in 0, 1, or 2 chunks.
   */
  if (loopp>0) {
    if ((err=eautd_events(ctx,evts,loopp))<0) return -1;
  }
  if (loopp<evtsc) {
    if ((err=eautd_events(ctx,evts+loopp,evtsc-loopp))<0) return -1;
  }
  return 0;
}

/* EAU-Text from EAU, main entry point.
 */
 
int eau_cvt_eaut_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr) {
  struct eautd ctx={.dst=dst,.path=path};
  int err=eautd_inner(&ctx,src,srcc);
  eautd_cleanup(&ctx);
  return err;
}
