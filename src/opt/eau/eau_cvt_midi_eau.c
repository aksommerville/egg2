#include "eau.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

/* Context.
 */
 
struct midi_eau_context {
  struct sr_encoder *dst;
  const char *path;
  int logged_error;
  
  /* We will arrange for the MIDI file's ticks to be exactly 1 ms each.
   * So the intrinsic timing on each side is both in milliseconds.
   * This probably yields larger MIDI files than necessary, but let's not worry about that.
   */
  int tempo; // ms/qnote. Guaranteed nonzero after "\0EAU" is received.
  int loopp;
  const uint8_t *eventv;
  int eventc;
  
  // (channelv) is indexed by (chid) and may contain dummies.
  // (modecfg) will be non-null for channels with a CHDR (and may still be empty in that case, just not null).
  struct eau_file_channel *channelv;
  int channelc,channela;
  
  struct midi_eau_event {
    int time; // Absolute time in ms.
    uint8_t chid,opcode,a,b; // Phrased for MIDI.
  } *mevtv;
  int mevtc,mevta;
  
  const void *text;
  int textc;
  int strip_names;
  struct sr_encoder *errmsg; // WEAK
};

static void midi_eau_context_cleanup(struct midi_eau_context *ctx) {
  if (ctx->channelv) free(ctx->channelv);
  if (ctx->mevtv) free(ctx->mevtv);
}

/* Log error.
 */
 
static int fail(struct midi_eau_context *ctx,const char *fmt,...) {
  if (ctx->logged_error) return -2;
  if (!ctx->path) return -1;
  char msg[256];
  va_list vargs;
  va_start(vargs,fmt);
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  if (ctx->errmsg) sr_encode_fmt(ctx->errmsg,"%s: %.*s\n",ctx->path,msgc,msg);
  else fprintf(stderr,"%s: %.*s\n",ctx->path,msgc,msg);
  ctx->logged_error=1;
  return -2;
}

/* Add channel if necessary, and fill in defaults.
 */
 
static struct eau_file_channel *midi_eau_channel_require(struct midi_eau_context *ctx,int chid) {
  if ((chid<0)||(chid>0xff)) return 0;
  if (chid<ctx->channelc) return ctx->channelv+chid;
  if (chid>=ctx->channela) {
    int na=(ctx->channela+16)&~15;
    void *nv=realloc(ctx->channelv,sizeof(struct eau_file_channel)*na);
    if (!nv) return 0;
    ctx->channelv=nv;
    ctx->channela=na;
  }
  while (ctx->channelc<=chid) {
    struct eau_file_channel *channel=ctx->channelv+ctx->channelc++;
    memset(channel,0,sizeof(struct eau_file_channel));
    channel->chid=ctx->channelc-1;
    channel->trim=0x80;
    channel->pan=0x80;
  }
  return ctx->channelv+chid;
}

/* Receive chunks.
 */
 
static int midi_eau_receive_EAU(struct midi_eau_context *ctx,const uint8_t *src,int srcc) {
  if (ctx->tempo) return fail(ctx,"Multiple \"\\0EAU\" chunks.");
  if (srcc>=2) {
    ctx->tempo=(src[0]<<8)|src[1];
    if (!ctx->tempo) return fail(ctx,"Invalid tempo.");
    // Our tempo becomes the MIDI "Division" exactly, so it can't touch the high bit.
    // So our limit is 32k instead of 64k. That's fine, 32k is still crazy high. (1k would be a perfectly sane limit).
    if (ctx->tempo>=0x8000) return fail(ctx,"Tempo %d ms/qnote will not work for our cheesy MIDI conversion. Please reduce to below 32768.",ctx->tempo);
    if (srcc>=4) {
      ctx->loopp=(src[2]<<8)|src[3];
    }
  } else {
    ctx->tempo=500;
  }
  return 0;
}
 
static int midi_eau_receive_CHDR(struct midi_eau_context *ctx,const uint8_t *src,int srcc) {
  if (srcc<1) return 0; // I'll grudgingly call empty CHDR legal, but it's definitely noop.
  struct eau_file_channel *channel=midi_eau_channel_require(ctx,src[0]);
  if (!channel) return -1;
  if (channel->modecfg) return fail(ctx,"Multiple CHDR for channel %d.",src[0]);
  if (eau_file_channel_decode(channel,src,srcc)<0) return fail(ctx,"Failed to decode CHDR for channel %d.",src[0]);
  if (!channel->modecfg) channel->modecfg=""; // It's not required to exist in the serial, but we're using it as a "present" flag.
  return 0;
}
 
static int midi_eau_receive_EVTS(struct midi_eau_context *ctx,const uint8_t *src,int srcc) {
  if (ctx->eventv) return fail(ctx,"Multiple \"EVTS\" chunks.");
  ctx->eventv=src;
  ctx->eventc=srcc;
  return 0;
}

static int midi_eau_receive_TEXT(struct midi_eau_context *ctx,const uint8_t *src,int srcc) {
  if (ctx->strip_names) return 0; // Stripping names is just this; pretend we didn't see them here at intake.
  ctx->text=src;
  ctx->textc=srcc;
  return 0;
}

/* Produce MIDI events at time zero for the tempo and channel headers.
 */
 
static int midi_eau_header_events(struct midi_eau_context *ctx) {

  // Our Division is already in ms/qnote, so Meta 0x51 Set Tempo is just that times 1000.
  sr_encode_raw(ctx->dst,"\0\xff\x51\3",4);
  sr_encode_intbe(ctx->dst,ctx->tempo*1000,3);
  
  // Meta 0x78 for the text index, if we have one. Dump it verbatim, it's already encoded the way we want it.
  if (ctx->textc) {
    sr_encode_raw(ctx->dst,"\0\xff\x78",3);
    sr_encode_vlq(ctx->dst,ctx->textc);
    sr_encode_raw(ctx->dst,ctx->text,ctx->textc);
  }
  
  // Meta 0x77 for each CHDR that existed in the input file, whether it's used or not.
  struct eau_file_channel *channel=ctx->channelv;
  int i=ctx->channelc;
  for (;i-->0;channel++) {
    if (!channel->modecfg) continue;
    int len=8+channel->modecfgc+channel->postc;
    sr_encode_raw(ctx->dst,"\0\xff\x77",3);
    sr_encode_vlq(ctx->dst,len);
    sr_encode_u8(ctx->dst,channel->chid);
    sr_encode_u8(ctx->dst,channel->trim);
    sr_encode_u8(ctx->dst,channel->pan);
    sr_encode_u8(ctx->dst,channel->mode);
    sr_encode_intbe(ctx->dst,channel->modecfgc,2);
    sr_encode_raw(ctx->dst,channel->modecfg,channel->modecfgc);
    sr_encode_intbe(ctx->dst,channel->postc,2);
    sr_encode_raw(ctx->dst,channel->post,channel->postc);
  }
  
  // If we felt like it, we could also emit Control Change 0x07 Volume MSB and 0x0a Pan MSB,
  // so non-Egg decoders can at least get those two things right per channel.
  // I don't think there's much value in it.
  return 0;
}

/* Insert a new (mevt) at the given time.
 * If events already exist at this time, we guarantee to insert after them.
 */
 
static struct midi_eau_event *midi_eau_event_insert(struct midi_eau_context *ctx,int time) {
  int lo=0,hi=ctx->mevtc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct midi_eau_event *q=ctx->mevtv+ck;
         if (time<q->time) hi=ck;
    else if (time>q->time) lo=ck+1;
    else {
      lo=ck;
      while ((lo<hi)&&(q->time==time)) { lo++; q++; }
      break;
    }
  }
  if (ctx->mevtc>=ctx->mevta) {
    int na=ctx->mevta+128;
    if (na>INT_MAX/sizeof(struct midi_eau_event)) return 0;
    void *nv=realloc(ctx->mevtv,sizeof(struct midi_eau_event)*na);
    if (!nv) return 0;
    ctx->mevtv=nv;
    ctx->mevta=na;
  }
  struct midi_eau_event *event=ctx->mevtv+lo;
  memmove(event+1,event,sizeof(struct midi_eau_event)*(ctx->mevtc-lo));
  ctx->mevtc++;
  memset(event,0,sizeof(struct midi_eau_event));
  event->time=time;
  return event;
}

/* Produce MIDI events for note and wheel, and the final Meta 0x2f End Of Track.
 * It would be most efficient to do this in a streaming manner, caching Note Off times and paying them out as delays get written.
 * That's pretty complicated so I'm opting for a simpler, less efficient approach: Generate an intermediate list of MIDI events first.
 * By similar rationale, we're not using Running Status or the velocity-zero trick. Keep it simple.
 */
 
static int midi_eau_stream_events(struct midi_eau_context *ctx) {

  /* Build up (mevtv).
   */
  ctx->mevtc=0;
  struct eau_event_reader reader={.v=ctx->eventv,.c=ctx->eventc};
  struct eau_event event;
  int err,time=0;
  while ((err=eau_event_reader_next(&event,&reader))>0) {
    switch (event.type) {
      case 'd': time+=event.delay; break;
      case 'n': {
          struct midi_eau_event *mevt;
          if (!(mevt=midi_eau_event_insert(ctx,time))) return -1;
          mevt->opcode=0x90;
          mevt->chid=event.note.chid;
          mevt->a=event.note.noteid;
          mevt->b=event.note.velocity;
          if (!(mevt=midi_eau_event_insert(ctx,time+event.note.durms))) return -1;
          mevt->opcode=0x80;
          mevt->chid=event.note.chid;
          mevt->a=event.note.noteid;
          mevt->b=0x40;
        } break;
      case 'w': {
          struct midi_eau_event *mevt=midi_eau_event_insert(ctx,time);
          if (!mevt) return -1;
          mevt->opcode=0xe0;
          mevt->chid=event.wheel.chid;
          event.wheel.v+=512;
          event.wheel.v=(event.wheel.v<<4)|(event.wheel.v&15);
          mevt->a=event.wheel.v&0x7f;
          mevt->b=event.wheel.v>>7;
        } break;
    }
  }
  if (err<0) return fail(ctx,"Malformed EAU events.");
  
  /* Add the Meta 0x2f End Of Track.
   * This is important because it communicates the trailing delay.
   * But be careful. The EAU could contain notes whose tail exceeds the total delays. (they're not supposed to but allow that it's possible).
   * If that's the case, push the EOT time out to the final Note Off time.
   */
  int eottime=time;
  if ((ctx->mevtc>0)&&(ctx->mevtv[ctx->mevtc-1].time>eottime)) eottime=ctx->mevtv[ctx->mevtc-1].time;
  struct midi_eau_event *mevt=midi_eau_event_insert(ctx,eottime);
  if (!mevt) return -1;
  mevt->opcode=0xff;
  mevt->a=0x2f;
  
  /* And now encode the MIDI events for real.
   * This part is trivial.
   */
  int i=ctx->mevtc;
  for (mevt=ctx->mevtv,time=0;i-->0;mevt++) {
  
    int delay=mevt->time-time;
    sr_encode_vlq(ctx->dst,delay);
    time=mevt->time;
    
    if (mevt->opcode==0xff) {
      sr_encode_raw(ctx->dst,"\xff\x2f\x00",3);
    } else {
      sr_encode_u8(ctx->dst,mevt->opcode|mevt->chid);
      sr_encode_u8(ctx->dst,mevt->a);
      sr_encode_u8(ctx->dst,mevt->b);
    }
  }
  return 0;
}

/* Main op in context.
 */
 
static int midi_eau_inner(struct midi_eau_context *ctx,const void *src,int srcc) {

  /* Read the file chunkwise, recording the interesting bits.
   */
  struct eau_file_reader reader={.v=src,.c=srcc};
  struct eau_file_chunk chunk;
  int err;
  while ((err=eau_file_reader_next(&chunk,&reader))>0) {
    if (!memcmp(chunk.id,"\0EAU",4)) {
      if ((err=midi_eau_receive_EAU(ctx,chunk.v,chunk.c))<0) return err;
    } else if (!memcmp(chunk.id,"CHDR",4)) {
      if ((err=midi_eau_receive_CHDR(ctx,chunk.v,chunk.c))<0) return err;
    } else if (!memcmp(chunk.id,"EVTS",4)) {
      if ((err=midi_eau_receive_EVTS(ctx,chunk.v,chunk.c))<0) return err;
    } else if (!memcmp(chunk.id,"TEXT",4)) {
      if ((err=midi_eau_receive_TEXT(ctx,chunk.v,chunk.c))<0) return err;
    } else {
      if (ctx->path) {
        char cname[32];
        int cnamec=sr_string_repr(cname,sizeof(cname),chunk.id,4);
        if ((cnamec<0)||(cnamec>sizeof(cname))) cnamec=0;
        fprintf(stderr,"%s:WARNING: Ignoring %d-byte chunk %.*s.\n",ctx->path,chunk.c,cnamec,cname);
      }
    }
  }
  if (err<0) return fail(ctx,"Failed to decode EAU file.");
  if (!ctx->tempo) return fail(ctx,"Missing \"\\0EAU\" chunk.");
  
  /* Emit MThd and begin MTrk -- we're going to dump everything in one track.
   */
  sr_encode_raw(ctx->dst,"MThd\0\0\0\6",8);
  sr_encode_intbe(ctx->dst,1,2); // Format
  sr_encode_intbe(ctx->dst,1,2); // Track Count
  sr_encode_intbe(ctx->dst,ctx->tempo,2); // Division
  sr_encode_raw(ctx->dst,"MTrk",4);
  int mtrklenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0\0\0",4);
  
  if ((err=midi_eau_header_events(ctx))<0) return err;
  if ((err=midi_eau_stream_events(ctx))<0) return err;
  
  int len=ctx->dst->c-mtrklenp-4;
  if (len<0) return fail(ctx,"Invalid MTrk length.");
  ((uint8_t*)ctx->dst->v)[mtrklenp++]=len>>24;
  ((uint8_t*)ctx->dst->v)[mtrklenp++]=len>>16;
  ((uint8_t*)ctx->dst->v)[mtrklenp++]=len>>8;
  ((uint8_t*)ctx->dst->v)[mtrklenp++]=len;
  
  return sr_encoder_assert(ctx->dst);
}

/* MIDI from EAU, main entry point.
 */
 
int eau_cvt_midi_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg) {
  struct midi_eau_context ctx={.dst=dst,.path=path,.strip_names=strip_names,.errmsg=errmsg};
  int err=midi_eau_inner(&ctx,src,srcc);
  midi_eau_context_cleanup(&ctx);
  return err;
}
