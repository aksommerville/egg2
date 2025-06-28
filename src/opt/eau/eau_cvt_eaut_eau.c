#include "eau.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

struct eaut_eau_context ctx;
static int eau_cvt_eaut_eau_inner(struct eaut_eau_context *ctx,const void *src,int srcc);

/* Context.
 */
 
struct eaut_eau_context {
  struct sr_encoder *dst;
  const char *path;
  int logged_error;
  int indent;
};

static void eaut_eau_context_cleanup(struct eaut_eau_context *ctx) {
}

/* Log error.
 */

static int fail(struct eaut_eau_context *ctx,const char *fmt,...) {
  if (ctx->logged_error) return -2;
  if (!ctx->path) return -1;
  ctx->logged_error=1;
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  fprintf(stderr,"%s: %.*s\n",ctx->path,msgc,msg);
  return -2;
}

/* Measure envelope in binary.
 */
 
static int eau_env_measure(const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  int srcp=0;
  uint8_t flags=src[srcp++];
  if (flags&0x01) { // Initials.
    if (flags&0x02) srcp+=4; // Velocity.
    else srcp+=2;
  }
  if (srcp>=srcc) return 0;
  int pointc=src[srcp++]&15;
  int ptlen=(flags&0x02)?8:4;
  ptlen*=pointc;
  if (srcp>srcc-ptlen) return 0;
  return srcp+ptlen;
}

/* Emit the indentation. every line of output must begin with this.
 */
 
static void eaut_eau_indent(struct eaut_eau_context *ctx) {
  if (ctx->indent<1) return;
  char space[]="                                     ";
  int spacec=ctx->indent;
  if (spacec>=sizeof(space)) spacec=sizeof(space)-1;
  sr_encode_raw(ctx->dst,space,spacec);
}

/* Hexdump with a trailing newline.
 */
 
static int eaut_eau_hexdump(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int linelen=40; // There's indent, so this actually exceeds the conventional 80 chars (a convention I don't care much for, so there).
  int srcp=0;
  while (srcp<srcc) {
    int cpc=srcc-srcp;
    if (cpc>linelen) cpc=linelen;
    eaut_eau_indent(ctx);
    while (cpc-->0) {
      sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]>>4]);
      sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]&15]);
      srcp++;
    }
    sr_encode_u8(ctx->dst,0x0a);
  }
  return 0;
}

/* Modecfg DRUM.
 */
 
static int eaut_eau_modecfg_DRUM(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return -1;
    uint8_t noteid=src[srcp++];
    uint8_t trimlo=src[srcp++];
    uint8_t trimhi=src[srcp++];
    uint8_t pan=src[srcp++];
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return -1;
    const uint8_t *body=src+srcp;
    srcp+=len;
    // One line to introduce each note.
    eaut_eau_indent(ctx);
    sr_encode_fmt(ctx->dst,"%02x %02x %02x %02x len(2) {\n",noteid,trimlo,trimhi,pan);
    ctx->indent+=2;
    int err=eau_cvt_eaut_eau_inner(ctx,body,len);
    ctx->indent-=2;
    sr_encode_raw(ctx->dst,"}\n",2);
  }
  return 0;
}

/* Helper macros for modecfg.
 */

#define FIXED(size,label) { \
  if (srcp<=srcc-size) { \
    int i=size; while (i-->0) { \
      sr_encode_fmt(ctx->dst,"%02x ",src[srcp++]); \
    } \
    if (label[0]) { \
      sr_encode_fmt(ctx->dst,"# %s\n",label); \
    } else { \
      sr_encode_u8(ctx->dst,0x0a); \
    } \
  } \
}

#define ENV(label) { \
  int size=eau_env_measure(src+srcp,srcc-srcp); \
  if (size>0) { \
    FIXED(size,label) \
  } \
}

/* Modecfg FM.
 */
 
static int eaut_eau_modecfg_FM(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  #define FIXED(size,label) { \
    if (srcp<=srcc-size) { \
      int i=size; while (i-->0) { \
        sr_encode_fmt(ctx->dst,"%02x ",src[srcp++]); \
      } \
      if (label[0]) { \
        sr_encode_fmt(ctx->dst,"# %s\n",label); \
      } else { \
        sr_encode_u8(ctx->dst,0x0a); \
      } \
    } \
  }
  #define ENV(label) { \
    int size=eau_env_measure(src+srcp,srcc-srcp); \
    if (size>0) { \
      FIXED(size,label) \
    } \
  }
  FIXED(2,"rate")
  FIXED(2,"range")
  ENV("level")
  ENV("range")
  ENV("pitch")
  FIXED(2,"wheel range")
  FIXED(2,"lfo rate")
  FIXED(1,"lfo depth")
  FIXED(1,"lfo phase")
  if (srcp<srcc) {
    eaut_eau_indent(ctx);
    for (;srcp<srcc;srcp++) sr_encode_fmt(ctx->dst,"%02x ",src[srcp]);
    sr_encode_u8(ctx->dst,0x0a);
  }
  return 0;
}

/* Modecfg HARSH.
 */
 
static int eaut_eau_modecfg_HARSH(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp<srcc) { // shape, use symbols
    eaut_eau_indent(ctx);
    uint8_t shape=src[srcp++];
    switch (shape) {
      case 0: sr_encode_raw(ctx->dst,"SHAPE_SINE\n",-1); break;
      case 1: sr_encode_raw(ctx->dst,"SHAPE_SQUARE\n",-1); break;
      case 2: sr_encode_raw(ctx->dst,"SHAPE_SAW\n",-1); break;
      case 3: sr_encode_raw(ctx->dst,"SHAPE_TRIANGLE\n",-1); break;
      default: sr_encode_fmt(ctx->dst,"%02x # shape\n",shape);
    }
  }
  ENV("level")
  ENV("pitch")
  FIXED(2,"wheel range")
  if (srcp<srcc) {
    eaut_eau_indent(ctx);
    for (;srcp<srcc;srcp++) sr_encode_fmt(ctx->dst,"%02x ",src[srcp]);
    sr_encode_u8(ctx->dst,0x0a);
  }
  return 0;
}

/* Modecfg HARM.
 */
 
static int eaut_eau_modecfg_HARM(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  // Alas we can't use "len(){}" for the harmonics, because it's a count of 2-byte words.
  // Could create a new function for that but meh.
  int srcp=0;
  if (srcp<srcc) {
    int harmc=src[srcp++];
    if (srcp>srcc-harmc*2) return fail(ctx,"Malformed CHDR for wave harmonics.");
    eaut_eau_indent(ctx);
    sr_encode_fmt(ctx->dst,"%02x ",harmc);
    int i=harmc; while (i-->0) {
      int v=(src[srcp]<<8)|src[srcp+1];
      srcp+=2;
      sr_encode_fmt(ctx->dst,"%04x ",v);
    }
    sr_encode_raw(ctx->dst,"# harmonics\n",-1);
  }
  ENV("level")
  ENV("pitch")
  FIXED(2,"wheel range")
  if (srcp<srcc) {
    eaut_eau_indent(ctx);
    for (;srcp<srcc;srcp++) sr_encode_fmt(ctx->dst,"%02x ",src[srcp]);
    sr_encode_u8(ctx->dst,0x0a);
  }
  return 0;
}

#undef FIXED
#undef ENV

/* Modecfg, dispatch.
 */
 
static int eaut_eau_modecfg(struct eaut_eau_context *ctx,uint8_t mode,const uint8_t *src,int srcc) {
  int err=0;
  switch (mode) {
    case 1: err=eaut_eau_modecfg_DRUM(ctx,src,srcc); break;
    case 2: err=eaut_eau_modecfg_FM(ctx,src,srcc); break;
    case 3: err=eaut_eau_modecfg_HARSH(ctx,src,srcc); break;
    case 4: err=eaut_eau_modecfg_HARM(ctx,src,srcc); break;
    default: err=eaut_eau_hexdump(ctx,src,srcc);
  }
  return err;
}

/* Post.
 */
 
static int eaut_eau_post(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<=srcc-2) {
    uint8_t stageid=src[srcp++];
    uint8_t len=src[srcp++];
    if (srcp>srcc-len) return fail(ctx,"Post length overruns.");
    const uint8_t *body=src+srcp;
    srcp+=len;
    // Each stage on one line. Don't use general hexdump because that's multi-line, and posts can't go over 255 bytes (typically under 10).
    const char *stagename=0;
    switch (stageid) {
      case 0: stagename="POST_NOOP"; break;
      case 1: stagename="POST_DELAY"; break;
      case 2: stagename="POST_WAVESHAPER"; break;
      case 3: stagename="POST_TREMOLO"; break;
    }
    eaut_eau_indent(ctx);
    if (stagename) sr_encode_raw(ctx->dst,stagename,-1);
    else sr_encode_fmt(ctx->dst,"%02x",stageid);
    sr_encode_raw(ctx->dst," len(1) { ",-1);
    int i=0; for (;i<len;i++) sr_encode_fmt(ctx->dst,"%02x ",body[i]);
    sr_encode_raw(ctx->dst,"}\n",2);
  }
  return 0;
}

/* "\0EAU" chunk.
 */
 
static int eaut_eau_chunk_EAU(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp<=srcc-2) {
    eaut_eau_indent(ctx);
    sr_encode_fmt(ctx->dst,"u16(%d) # ms/qnote\n",(src[srcp]<<8)|src[srcp+1]);
    srcp+=2;
    if (srcp<=srcc-4) {
      eaut_eau_indent(ctx);
      sr_encode_fmt(ctx->dst,"u16(%d) # loopp\n",(src[srcp]<<8)|src[srcp+1]);
      srcp+=2;
    }
  }
  if (eaut_eau_hexdump(ctx,src+srcp,srcc-srcp)<0) return -1;
  return 0;
}

/* "CHDR" chunk.
 */
 
static int eaut_eau_chunk_CHDR(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  struct eau_file_channel channel={0};
  if (eau_file_channel_decode(&channel,src,srcc)<0) return fail(ctx,"Failed to decode CHDR.");
  
  /* (chid,trim,pan,mode) on one line to keep it tidy.
   * Emit these four fields even if they're defaulted.
   */
  eaut_eau_indent(ctx);
  sr_encode_fmt(ctx->dst,"%02x %02x %02x ",channel.chid,channel.trim,channel.pan);
  switch (channel.mode) {
    case 0: sr_encode_raw(ctx->dst,"MODE_NOOP",9); break;
    case 1: sr_encode_raw(ctx->dst,"MODE_DRUM",9); break;
    case 2: sr_encode_raw(ctx->dst,"MODE_FM",7); break;
    case 3: sr_encode_raw(ctx->dst,"MODE_HARSH",10); break;
    case 4: sr_encode_raw(ctx->dst,"MODE_HARM",9); break;
    default: sr_encode_fmt(ctx->dst,"%02x",channel.mode);
  }
  sr_encode_u8(ctx->dst,0x0a);
  
  /* (modecfg,post) as "len" blocks, but only emit if we need to.
   */
  if (channel.modecfgc||channel.postc) {
    eaut_eau_indent(ctx);
    sr_encode_raw(ctx->dst,"len(2) {\n",-1);
    ctx->indent+=2;
    int err=eaut_eau_modecfg(ctx,channel.mode,channel.modecfg,channel.modecfgc);
    ctx->indent-=2;
    if (err<0) return err;
    eaut_eau_indent(ctx);
    sr_encode_raw(ctx->dst,"}\n",2);
  }
  if (channel.postc) {
    eaut_eau_indent(ctx);
    sr_encode_raw(ctx->dst,"len(2) {\n",-1);
    ctx->indent+=2;
    int err=eaut_eau_post(ctx,channel.post,channel.postc);
    ctx->indent-=2;
    if (err<0) return err;
    eaut_eau_indent(ctx);
    sr_encode_raw(ctx->dst,"}\n",2);
  }
  
  return 0;
}

/* "EVTS" chunk.
 */
 
static int eaut_eau_chunk_EVTS(struct eaut_eau_context *ctx,const uint8_t *src,int srcc) {
  struct eau_event_reader reader={.v=src,.c=srcc};
  struct eau_event event;
  int err;
  while ((err=eau_event_reader_next(&event,&reader))>0) {
    eaut_eau_indent(ctx);
    switch (event.type) {
      case 'd': sr_encode_fmt(ctx->dst,"delay(%d)\n",event.delay); break;
      case 'n': sr_encode_fmt(ctx->dst,"note(%d,%d,%d,%d)\n",event.note.chid,event.note.noteid,event.note.velocity,event.note.durms); break;
      case 'w': sr_encode_fmt(ctx->dst,"wheel(%d,%d)\n",event.wheel.chid,event.wheel.v); break;
      default: return fail(ctx,"Unexpected EAU event.");
    }
  }
  if (err<0) return fail(ctx,"Failed to decode EAU events.");
  return 0;
}

/* In context.
 */
 
static int eau_cvt_eaut_eau_inner(struct eaut_eau_context *ctx,const void *src,int srcc) {
  struct eau_file_reader reader={.v=src,.c=srcc};
  struct eau_file_chunk chunk;
  int err;
  while ((err=eau_file_reader_next(&chunk,&reader))>0) {
    char token[32];
    int tokenc=sr_string_repr(token,sizeof(token),chunk.id,4);
    if ((tokenc<2)||(tokenc>sizeof(token))) return -1;
    eaut_eau_indent(ctx);
    sr_encode_raw(ctx->dst,token,tokenc);
    sr_encode_raw(ctx->dst," len(4) {\n",-1);
    ctx->indent+=2;
         if (!memcmp(chunk.id,"\0EAU",4)) err=eaut_eau_chunk_EAU(ctx,chunk.v,chunk.c);
    else if (!memcmp(chunk.id,"CHDR",4)) err=eaut_eau_chunk_CHDR(ctx,chunk.v,chunk.c);
    else if (!memcmp(chunk.id,"EVTS",4)) err=eaut_eau_chunk_EVTS(ctx,chunk.v,chunk.c);
    else err=eaut_eau_hexdump(ctx,chunk.v,chunk.c);
    ctx->indent-=2;
    if (err<0) return err;
    sr_encode_raw(ctx->dst,"}\n",2);
  }
  if (err<0) return fail(ctx,"Malformed EAU.");
  return sr_encoder_assert(ctx->dst);
}

/* EAU-Text from EAU, main entry point.
 */
 
int eau_cvt_eaut_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr) {
  struct eaut_eau_context ctx={.dst=dst,.path=path};
  int err=eau_cvt_eaut_eau_inner(&ctx,src,srcc);
  eaut_eau_context_cleanup(&ctx);
  return err;
}
