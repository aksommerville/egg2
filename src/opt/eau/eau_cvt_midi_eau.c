#include "eau.h"
#include "opt/serial/serial.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

/* Context.
 */
 
struct midi_from_eau {
  struct sr_encoder *dst; // WEAK
  const uint8_t *src;
  int srcc;
  const char *path;
  int strip_names;
  struct sr_encoder *errmsg;
  int status;
  struct midi_from_eau_hold {
    uint8_t chid,noteid;
    int offtime;
  } *holdv;
  int holdc,holda;
};

static void midi_from_eau_cleanup(struct midi_from_eau *ctx) {
  if (ctx->holdv) free(ctx->holdv);
}

/* Logging.
 */
 
static int midi_from_eau_error(struct midi_from_eau *ctx,const char *fmt,...) {
  if (ctx->status<0) return ctx->status; // Already logged.
  if (ctx->errmsg||ctx->path) { // Logging enabled or captured.
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; // eg trailing LF, I might include by accident.
    if (ctx->errmsg) {
      sr_encode_fmt(ctx->errmsg,"%s: %.*s\n",ctx->path?ctx->path:"midi_from_eau",msgc,msg);
    } else if (ctx->path) {
      fprintf(stderr,"%s: %.*s\n",ctx->path,msgc,msg);
    } else { // Not currently reachable.
      fprintf(stderr,"%.*s\n",msgc,msg);
    }
    ctx->status=-2;
  } else { // No logging, just fail.
    ctx->status=-1;
  }
  return ctx->status;
}

/* Add hold.
 */
 
static int midi_from_eau_add_hold(struct midi_from_eau *ctx,uint8_t chid,uint8_t noteid,int offtime) {

  // They must be sorted by (offtime), which is not necessarily the order we receive them.
  int lo=0,hi=ctx->holdc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int qtime=ctx->holdv[ck].offtime;
         if (offtime<qtime) hi=ck;
    else if (offtime>qtime) lo=ck+1;
    else { lo=ck; break; }
  }
  int p=lo;

  if (ctx->holdc>=ctx->holda) {
    int na=ctx->holda+16;
    if (na>INT_MAX/sizeof(struct midi_from_eau_hold)) return -1;
    void *nv=realloc(ctx->holdv,sizeof(struct midi_from_eau_hold)*na);
    if (!nv) return -1;
    ctx->holdv=nv;
    ctx->holda=na;
  }
  struct midi_from_eau_hold *hold=ctx->holdv+p;
  memmove(hold+1,hold,sizeof(struct midi_from_eau_hold)*(ctx->holdc-p));
  ctx->holdc++;
  
  hold->chid=chid;
  hold->noteid=noteid;
  hold->offtime=offtime;
  return 0;
}

/* MIDI from EAU, events only.
 * We will emit the Meta 0x2f End Of Track at the end, but will not do the chunk framing.
 */
 
static int midi_from_eau_events(struct midi_from_eau *ctx,const uint8_t *src,int srcc) {
  int wtime=0; // Cumulative time of the last event we emitted.
  int rtime=0; // Sum of delays as we gather them.
  int srcp=0;
  
  // Call to advance (wtime) up to (rtime), emitting outstanding Note Offs as needed.
  // We will emit a delay, which *must* be followed by a MIDI event.
  #define FLUSH_DELAY { \
    /* Holds are sorted chronologically. Pop from the front while they're <=wtime. */ \
    while (ctx->holdc&&(ctx->holdv[0].offtime<=rtime)) { \
      int delay=ctx->holdv[0].offtime-wtime; \
      wtime=ctx->holdv[0].offtime; \
      if (delay<0) delay=0; \
      sr_encode_vlq(ctx->dst,delay); \
      sr_encode_u8(ctx->dst,0x80|ctx->holdv[0].chid); \
      sr_encode_u8(ctx->dst,ctx->holdv[0].noteid); \
      sr_encode_u8(ctx->dst,0x40); /* Off Velocity not recorded, and not used. */ \
      ctx->holdc--; \
      memmove(ctx->holdv,ctx->holdv+1,sizeof(struct midi_from_eau_hold)*ctx->holdc); \
    } \
    /* And with the holds sorted, now emit one dangling delay. */ \
    int delay=rtime-wtime; \
    wtime=rtime; \
    if (delay<0) delay=0; \
    sr_encode_vlq(ctx->dst,delay); \
  }
  
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    
    // High bit unset is a delay, one of two forms.
    if (!(lead&0x80)) {
      if (lead&0x40) rtime+=((lead&0x3f)+1)<<6;
      else rtime+=lead;
      continue;
    }
    
    // Everything else, the high 4 bits of lead distinguish the event.
    switch (lead&0xf0) {
    
      // Note Off, Note On, and Wheel can pass straight thru; MIDI and EAU are identical.
      // We're not emitting Running Status. If we ever want to, to reduce file sizes, would need some more involved logic here.
      case 0x80: case 0x90: case 0xe0: {
          if (srcp>srcc-2) return midi_from_eau_error(ctx,"Unexpected EOF reading MIDI event");
          uint8_t a=src[srcp++];
          uint8_t b=src[srcp++];
          FLUSH_DELAY
          sr_encode_u8(ctx->dst,lead);
          sr_encode_u8(ctx->dst,a);
          sr_encode_u8(ctx->dst,b);
        } break;
        
      case 0xa0: { // Note Once
          if (srcp>srcc-3) return midi_from_eau_error(ctx,"Unexpected EOF reading Note Once event");
          uint8_t a=src[srcp++];
          uint8_t b=src[srcp++];
          uint8_t c=src[srcp++];
          FLUSH_DELAY
          sr_encode_u8(ctx->dst,0x90|(lead&0x0f));
          sr_encode_u8(ctx->dst,a>>1);
          sr_encode_u8(ctx->dst,((a&1)<<6)|(b>>2));
          int durms=(((b&3)<<8)|c)<<4;
          if (midi_from_eau_add_hold(ctx,lead&0x0f,a>>1,wtime+durms)<0) return -1;
        } break;
        
      case 0xf0: { // Marker
          // The only marker we preserve is 0x00 Loop Point. We don't have a generic way to wrap markers for MIDI (TODO should we add a new Meta for this?)
          switch (lead&0x0f) {
            case 0x00: {
                FLUSH_DELAY
                sr_encode_u8(ctx->dst,0xff); // Meta
                sr_encode_u8(ctx->dst,0x07); // Cue Point
                sr_encode_u8(ctx->dst,4);
                sr_encode_raw(ctx->dst,"LOOP",4);
              } break;
          }
        } break;
        
      default: return midi_from_eau_error(ctx,"Unexpected lead 0x%02x around %d/%d in events.",lead,srcp-1,srcc);
    }
  }
  
  /* Meta 0x2f End Of Track, and Off any outstanding Note Ons.
   * This gets a little weird, because we'll optimistically emit the Meta's delay,
   * but then maybe recycle it for those Note Offs.
   */
  FLUSH_DELAY
  #undef FLUSH_DELAY
  const struct midi_from_eau_hold *hold=ctx->holdv;
  int i=ctx->holdc;
  for (;i-->0;hold++) {
    sr_encode_u8(ctx->dst,0x80|hold->chid);
    sr_encode_u8(ctx->dst,hold->noteid);
    sr_encode_u8(ctx->dst,0x40);
    sr_encode_u8(ctx->dst,0); // Delay for next event
  }
  sr_encode_u8(ctx->dst,0xff); // Meta
  sr_encode_u8(ctx->dst,0x2f); // End Of Track
  sr_encode_u8(ctx->dst,0); // Empty payload.
  
  return 0;
}

/* MIDI from EAU, in context.
 */
 
static int midi_from_eau_internal(struct midi_from_eau *ctx) {
  if ((ctx->srcc<6)||memcmp(ctx->src,"\0EAU",4)) return midi_from_eau_error(ctx,"Malformed EAU");
  int tempo=(ctx->src[4]<<8)|ctx->src[5];
  int srcp=6;
  
  // Grab the three chunks. Allow them to be missing.
  #define CHUNK(name) \
    const uint8_t *name=0; \
    int name##_len=0; \
    if (srcp<=ctx->srcc-4) { \
      name##_len=(ctx->src[srcp]<<24)|(ctx->src[srcp+1]<<16)|(ctx->src[srcp+2]<<8)|ctx->src[srcp+3]; \
      srcp+=4; \
      if ((name##_len<0)||(srcp>ctx->srcc-name##_len)) { \
        return midi_from_eau_error(ctx,"%s chunk overruns EOF",#name); \
      } \
      name=ctx->src+srcp; \
      srcp+=name##_len; \
    }
  CHUNK(chdr)
  CHUNK(events)
  CHUNK(text)
  #undef CHUNK
  
  /* Emit MThd and start MTrk. There will be just one track.
   * Since EAU times everything in ms, we'll keep things simple and arrange timing such that a tick is one millisecond.
   * That's fast but not unusual for a MIDI file.
   */
  if ((tempo<1)||(tempo>0x7fff)) return midi_from_eau_error(ctx,"Tempo %s yields an invalid MIDI division. Please keep in 1..32767.",tempo);
  sr_encode_raw(ctx->dst,"MThd\0\0\0\6",8);
  sr_encode_intbe(ctx->dst,1,2); // Format
  sr_encode_intbe(ctx->dst,1,2); // Track Count
  sr_encode_intbe(ctx->dst,tempo,2); // Division
  sr_encode_raw(ctx->dst,"MTrk",4);
  int mtrklenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0\0\0",4);
  
  /* If we have any channel headers, which we must for anything to play, emit them as Meta 0x77.
   */
  if (chdr_len) {
    sr_encode_u8(ctx->dst,0); // delay
    sr_encode_u8(ctx->dst,0xff); // Meta
    sr_encode_u8(ctx->dst,0x77); // type: Egg Channel Headers
    sr_encode_vlq(ctx->dst,chdr_len); // Payload is verbatim.
    sr_encode_raw(ctx->dst,chdr,chdr_len);
  }
  
  /* If we have text, and not instructed to strip it, emit as Meta 0x78.
   */
  if (text_len&&!ctx->strip_names) {
    sr_encode_u8(ctx->dst,0); // delay
    sr_encode_u8(ctx->dst,0xff); // Meta
    sr_encode_u8(ctx->dst,0x78); // type: Egg Text
    sr_encode_vlq(ctx->dst,text_len); // Payload is verbatim.
    sr_encode_raw(ctx->dst,text,text_len);
  }
  
  /* Meta 0x51 Set Tempo, to tell it that ticks are milliseconds.
   */
  sr_encode_u8(ctx->dst,0); // delay
  sr_encode_u8(ctx->dst,0xff); // Meta
  sr_encode_u8(ctx->dst,0x51); // type: Set Tempo
  sr_encode_u8(ctx->dst,3); // length
  sr_encode_intbe(ctx->dst,tempo*1000,3); // us, u24
  
  /* Events are much more complicated; call out.
   * This will also emit the Meta 0x2f End Of Track.
   */
  int err=midi_from_eau_events(ctx,events,events_len);
  if (err<0) return err;
  
  /* Fill in MTrk length.
   */
  int mtrklen=ctx->dst->c-mtrklenp-4;
  if (mtrklen<0) return midi_from_eau_error(ctx,"Ended up with invalid length %d for MTrk",mtrklen);
  ((uint8_t*)ctx->dst->v)[mtrklenp+0]=mtrklen>>24;
  ((uint8_t*)ctx->dst->v)[mtrklenp+1]=mtrklen>>16;
  ((uint8_t*)ctx->dst->v)[mtrklenp+2]=mtrklen>>8;
  ((uint8_t*)ctx->dst->v)[mtrklenp+3]=mtrklen;
  
  return sr_encoder_assert(ctx->dst);
}

/* MIDI from EAU, main entry point.
 */

int eau_cvt_midi_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg) {
  struct midi_from_eau ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .strip_names=strip_names,
    .errmsg=errmsg,
  };
  int err=midi_from_eau_internal(&ctx);
  midi_from_eau_cleanup(&ctx);
  return err;
}
