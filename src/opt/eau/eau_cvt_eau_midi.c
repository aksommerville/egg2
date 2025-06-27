#include "eau.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

/* Context.
 */
 
struct eau_midi_context {
  struct sr_encoder *dst;
  struct midi_file *midi;
  const char *path;
  eau_get_chdr_fn get_chdr;
  int logged_error;
  int tempo; // ms/qnote. 500 unless a Meta Set Tempo replaces it.
  int has_loop; // Populated during header collection. Actual position can't be known until we start emitting events.
  int looppp; // If (has_loop), position in (dst->v) of the 2-byte loop position.
  
  // May contain up to 256 channels, since our Meta 0x77 Channel Header can address the full 8 bits.
  struct eau_midi_channel {
    uint8_t chid; // Same as my index here.
    uint8_t trim; // 0..127 from Control 0x07, default 0x40.
    uint8_t pan; // 0..127 from Control 0x0a, default 0x40.
    int fqpid; // 0..0x001fffff from Bank Select and Program Change. Default -1.
    const void *chdr; // Meta 0x77 payload. Overrides (trim,pan,fqpid).
    int chdrc;
    int notec;
    int wheelc;
    uint8_t notev[16]; // Little-endian bits corresponding to noteid, which notes are used.
    int wheel; // Live value in EAU scale: 0..0x200..0x3ff
  } *channelv;
  int channelc,channela;
  
  struct hold {
    uint8_t chid,noteid;
    int eventp; // Position in (dst->v).
    double starttime;
  } *holdv;
  int holdc,holda;
};

static void eau_midi_context_cleanup(struct eau_midi_context *ctx) {
  midi_file_del(ctx->midi);
  if (ctx->channelv) free(ctx->channelv);
  if (ctx->holdv) free(ctx->holdv);
}

/* Log errors.
 * Safe to call again from outer scope; we'll only log the first one. (or none, if we don't have a path).
 * Always returns <0.
 */

static int fail(struct eau_midi_context *ctx,const char *fmt,...) {
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

/* Add channel.
 */
 
static struct eau_midi_channel *eau_midi_require_channel(struct eau_midi_context *ctx,int chid) {
  if (chid<0) return 0;
  if (chid<ctx->channelc) return ctx->channelv+chid;
  if (chid>=ctx->channela) {
    int na=(chid+16)&~15;
    if (na>INT_MAX/sizeof(struct eau_midi_channel)) return 0;
    void *nv=realloc(ctx->channelv,sizeof(struct eau_midi_channel)*na);
    if (!nv) return 0;
    ctx->channelv=nv;
    ctx->channela=na;
  }
  while (chid>=ctx->channelc) {
    struct eau_midi_channel *channel=ctx->channelv+ctx->channelc++;
    memset(channel,0,sizeof(struct eau_midi_channel));
    channel->chid=ctx->channelc-1;
    channel->trim=0x40;
    channel->pan=0x40;
    channel->fqpid=-1;
    channel->wheel=0x200;
  }
  return ctx->channelv+chid;
}

/* Stream events from MIDI file, collecting headers into the context.
 */
 
static int eau_midi_collect_headers(struct eau_midi_context *ctx) {
  midi_file_reset(ctx->midi);
  ctx->tempo=500;
  int err;
  double now=0.0;
  struct midi_event event;
  while ((err=midi_file_next(&event,ctx->midi))>0) {
    switch (event.type) {
      case 'd': now+=event.delay.s; break;
      case 'c': {
          struct eau_midi_channel *channel=eau_midi_require_channel(ctx,event.cv.chid);
          if (!channel) return -1;
          switch (event.cv.opcode) {
            case 0x90: channel->notec++; channel->notev[event.cv.a>>3]|=1<<(event.cv.a&7); break;
            case 0xb0: switch (event.cv.a) {
                case 0x00: if (channel->fqpid<0) channel->fqpid=0; channel->fqpid=(channel->fqpid&0x3fff)|(event.cv.b<<14); break; // Bank Select MSB
                case 0x07: channel->trim=event.cv.b; break; // Volume MSB
                case 0x0a: channel->pan=event.cv.b; break; // Pan MSB
                case 0x20: if (channel->fqpid<0) channel->fqpid=0; channel->fqpid=(channel->fqpid&0x1fc07f)|(event.cv.b<<7); break; // Bank Select LSB
              } break;
            case 0xc0: if (channel->fqpid<0) channel->fqpid=0; channel->fqpid=(channel->fqpid&0x1fff80)|event.cv.a; break;
            case 0xe0: channel->wheelc++; break;
          }
        } break;
      case 'b': {
          if (event.block.opcode==0xff) switch (event.block.type) {
            case 0x07: if ((event.block.c==4)&&!memcmp(event.block.v,"LOOP",4)) ctx->has_loop=1; break;
            case 0x51: { // Set Tempo
                if (event.block.c>=3) {
                  const uint8_t *v=event.block.v;
                  if ((ctx->tempo=((v[0]<<16)|(v[1]<<8)|v[2])/1000)<1) ctx->tempo=1;
                }
              } break;
            case 0x77: { // Egg Channel Header
                if (event.block.c>=1) {
                  int chid=((uint8_t*)event.block.v)[0];
                  struct eau_midi_channel *channel=eau_midi_require_channel(ctx,chid);
                  if (!channel) return -1;
                  if (channel->chdr&&ctx->path) fprintf(stderr,"%s:WARNING: Multiple CHDR for channel %d. Using the last.\n",ctx->path,chid);
                  channel->chdr=event.block.v;
                  channel->chdrc=event.block.c;
                }
              } break;
          }
        } break;
    }
  }
  if (err<0) return fail(ctx,"Error streaming MIDI events.");
  return 0;
}

/* Copy a drum channel header, starting at (mode).
 * (src) is the entire header, including the (chid,trim,pan) that we will ignore.
 */
 
static int eau_midi_copy_drums_modecfg(struct eau_midi_context *ctx,struct eau_midi_channel *channel,const uint8_t *src,int srcc) {
  int srcp=0;
  for (;;) {
    if (srcp>srcc-6) return 0;
    int paylen=(src[srcp+4]<<8)|src[srcp+5];
    if (srcp>srcc-paylen-6) return -1;
    uint8_t noteid=src[srcp];
    if ((noteid<0x80)&&(channel->notev[noteid>>3]&(1<<(noteid&7)))) {
      sr_encode_raw(ctx->dst,src+srcp,6+paylen);
    }
    srcp+=6+paylen;
  }
  return 0;
}
 
static int eau_midi_copy_drums(struct eau_midi_context *ctx,struct eau_midi_channel *channel,const uint8_t *src,int srcc) {
  sr_encode_u8(ctx->dst,0x01); // mode=DRUM
  int srcp=4;
  if (srcp>srcc-2) return 0;
  int srcmodecfglen=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  if (srcp>srcc-srcmodecfglen) return -1;
  int modecfglenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0",2);
  if (eau_midi_copy_drums_modecfg(ctx,channel,src+srcp,srcmodecfglen)<0) return fail(ctx,"Failed to copy drum kit.");
  srcp+=srcmodecfglen;
  int modecfglen=ctx->dst->c-modecfglenp-2;
  if ((modecfglen<0)||(modecfglen>0xffff)) fail(ctx,"Invalid drums modecfg length %d.",modecfglen);
  ((uint8_t*)ctx->dst->v)[modecfglenp]=modecfglen>>8;
  ((uint8_t*)ctx->dst->v)[modecfglenp+1]=modecfglen;
  sr_encode_raw(ctx->dst,src+srcp,srcc-srcp);
  return 0;
}

/* Generate CHDR body for a channel that didn't have an explicit one.
 * (chid,trim,pan) must be emitted first by the caller. We start at (mode), and complete the chunk.
 * There's plenty of opportunity for magic here, but I want to keep it reasonably simple.
 * I also want a bias for using SDK instruments when possible, rather than allowing the default.
 */
 
static int eau_midi_use_fqpid(struct eau_midi_context *ctx,struct eau_midi_channel *channel,int fqpid) {
  const uint8_t *src=0;
  int srcc=ctx->get_chdr(&src,fqpid);
  if ((srcc<0)||!src) return -1;
  if (srcc<=3) return 0;
  if (src[3]==0x01) { // Drum kit.
    return eau_midi_copy_drums(ctx,channel,src,srcc);
  } else {
    return sr_encode_raw(ctx->dst,src+3,srcc-3);
  }
}
 
static int eau_midi_generate_chdr(struct eau_midi_context *ctx,struct eau_midi_channel *channel) {

  // No SDK callback. Default is all we can do.
  if (!ctx->get_chdr) return 0;
  
  // We'll search first for the explicitly-selected program.
  // If there was no Program Change or Bank Select, default to 0x80 for 9 (conventionally drums) or 0x00 for everything else.
  int fqpid=channel->fqpid;
  if (fqpid<0) fqpid=(channel->chid==9)?0x80:0;
  if (eau_midi_use_fqpid(ctx,channel,fqpid)>=0) return 0;
  
  // Try the first in its group of 8.
  if (fqpid&7) {
    if (eau_midi_use_fqpid(ctx,channel,fqpid&~7)>=0) return 0;
  }
  
  // We recommend 0x80..0x8f for drum kits. If it's in 0x88..0x8f, try 0x80.
  if ((fqpid>=0x88)&&(fqpid<=0x8f)) {
    if (eau_midi_use_fqpid(ctx,channel,0x80)>=0) return 0;
  }
  
  // If it has a Bank greater than 1, try the same index in Bank Zero or One, ie the low 8 bits.
  // GM only discusses Bank Zero, but Egg offers some extra conventions for Bank One.
  if (fqpid>=0x100) {
    if (eau_midi_use_fqpid(ctx,channel,fqpid&0xff)>=0) return 0;
  }
  
  // Try program zero, which all instrument sets are strongly encouraged to implement.
  if (eau_midi_use_fqpid(ctx,channel,0)>=0) return 0;
  
  // Any other case, leave it unset from (mode) onward. Runtime will use its default.
  return 0;
}

/* With headers collected, emit the "\0EAU" and "CHDR" chunks.
 */
 
static int eau_midi_emit_headers(struct eau_midi_context *ctx) {

  sr_encode_raw(ctx->dst,"\0EAU",4);
  if (ctx->has_loop) {
    sr_encode_intbe(ctx->dst,4,4);
    sr_encode_intbe(ctx->dst,ctx->tempo,2);
    ctx->looppp=ctx->dst->c;
    sr_encode_intbe(ctx->dst,0,2);
  } else {
    sr_encode_intbe(ctx->dst,2,4);
    sr_encode_intbe(ctx->dst,ctx->tempo,2);
  }
  
  struct eau_midi_channel *channel=ctx->channelv;
  int i=ctx->channelc;
  for (;i-->0;channel++) {
  
    // If there's an explicit CHDR, we emit it verbatim, end of story. Even for high chid or no notes.
    if (channel->chdr) {
      sr_encode_raw(ctx->dst,"CHDR",4);
      sr_encode_intbe(ctx->dst,channel->chdrc,4);
      sr_encode_raw(ctx->dst,channel->chdr,channel->chdrc);
      continue;
    }
    
    // Channels with no notes or high chid are not addressed, so ignore them.
    // Note that a noop channel may have wheel events. We'll filter those out during event streaming.
    if (!channel->notec||(channel->chid>=16)) continue;
    
    // OK, we're making up a CHDR. (chid,trim,pan) come from what's stored in (channel) already.
    // Mind that (trim,pan) are 7-bit and we emit 8. Do this with a sticky low bit: That preserves the low, middle, and high values.
    sr_encode_raw(ctx->dst,"CHDR",4);
    int chdrlenp=ctx->dst->c;
    sr_encode_raw(ctx->dst,"\0\0\0\0",4);
    sr_encode_u8(ctx->dst,channel->chid);
    sr_encode_u8(ctx->dst,(channel->trim<<1)|(channel->trim&1));
    sr_encode_u8(ctx->dst,(channel->pan<<1)|(channel->pan&1));
    
    // (mode,modecfg,post) are much more complex. Call out for those.
    int err=eau_midi_generate_chdr(ctx,channel);
    if (err<0) return err;
    
    int len=ctx->dst->c-chdrlenp-4;
    if (len<0) return -1;
    ((uint8_t*)ctx->dst->v)[chdrlenp++]=len>>24;
    ((uint8_t*)ctx->dst->v)[chdrlenp++]=len>>16;
    ((uint8_t*)ctx->dst->v)[chdrlenp++]=len>>8;
    ((uint8_t*)ctx->dst->v)[chdrlenp++]=len;
  }
  
  return 0;
}

/* Emit zero or more delay events.
 * Return any remaining delay that we didn't emit (<1 ms).
 */
 
static double eau_midi_write_delay(struct eau_midi_context *ctx,double s) {
  int ms=(int)(s*1000.0);
  s-=(ms/1000.0);
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
  return s;
}

/* Add a hold record.
 */
 
static struct hold *eau_midi_add_hold(struct eau_midi_context *ctx) {
  if (ctx->holdc>=ctx->holda) {
    int na=ctx->holda+32;
    if (na>INT_MAX/sizeof(struct hold)) return 0;
    void *nv=realloc(ctx->holdv,sizeof(struct hold)*na);
    if (!nv) return 0;
    ctx->holdv=nv;
    ctx->holda=na;
  }
  struct hold *hold=ctx->holdv+ctx->holdc++;
  memset(hold,0,sizeof(struct hold));
  return hold;
}

/* Encode events, no framing.
 */
 
static int eau_midi_encode_events(struct eau_midi_context *ctx) {
  int dstc0=ctx->dst->c;
  struct midi_event event;
  int err;
  double delay=0.0,rtime=0.0;
  while ((err=midi_file_next(&event,ctx->midi))>0) {
    switch (event.type) {
      case 'd': delay+=event.delay.s; rtime+=event.delay.s; break; // Collect delays, don't emit until we need to.
      case 'c': switch (event.cv.opcode) {
      
          case 0x80: {
              int i=0;
              struct hold *hold=ctx->holdv;
              for (;i<ctx->holdc;i++,hold++) {
                if (hold->chid!=event.cv.chid) continue;
                if (hold->noteid!=event.cv.a) continue;
                if ((hold->eventp>=0)&&(hold->eventp<=ctx->dst->c-4)) {
                  int dur=(int)((rtime-hold->starttime)*1000.0)>>2;
                  if (dur>0) {
                    if (dur>0xfff) {
                      if (ctx->path) fprintf(stderr,
                        "%s:WARNING: Reducing duration of note 0x%02x on channel %d around time %.03f to 4095 ms from %d.\n",
                        ctx->path,hold->noteid,hold->chid,hold->starttime,dur
                      );
                      dur=0xff;
                    }
                    uint8_t *dst=((uint8_t*)ctx->dst->v)+hold->eventp+2;
                    dst[0]=(dst[0]&0xf0)|(dur>>8);
                    dst[1]=dur;
                  }
                }
                ctx->holdc--;
                memmove(hold,hold+1,sizeof(struct hold)*(ctx->holdc-i));
                break;
              }
            } break;
            
          case 0x90: {
              delay=eau_midi_write_delay(ctx,delay);
              struct hold *hold=eau_midi_add_hold(ctx);
              if (!hold) {
                if (ctx->path) fprintf(stderr,
                  "%s:WARNING: Failed to add hold. Note 0x%02x on channel %d around %.03f will have minimum duration.\n",
                  ctx->path,event.cv.a,event.cv.chid,rtime
                );
              } else {
                hold->chid=event.cv.chid;
                hold->noteid=event.cv.a;
                hold->starttime=rtime;
                hold->eventp=ctx->dst->c;
              }
              sr_encode_u8(ctx->dst,0x80|(event.cv.chid<<2)|(event.cv.a>>5));
              sr_encode_u8(ctx->dst,(event.cv.a<<3)|(event.cv.b>>4));
              sr_encode_u8(ctx->dst,event.cv.b<<4);
              sr_encode_u8(ctx->dst,0);
            } break;
            
          case 0xe0: {
              if (event.cv.chid>=ctx->channelc) break;
              struct eau_midi_channel *channel=ctx->channelv+event.cv.chid;
              if (!channel->notec) break; // Channel was probably not emitted, and doesn't produce sound. Don't bother with the wheel.
              int v=event.cv.a|(event.cv.b<<7);
              v>>=4;
              if (v==channel->wheel) break;
              channel->wheel=v;
              delay=eau_midi_write_delay(ctx,delay);
              sr_encode_u8(ctx->dst,0xc0|(event.cv.chid<<2)|(v>>8));
              sr_encode_u8(ctx->dst,v);
            } break;
            
        } break;
      case 'b': if (event.block.opcode==0xff) switch (event.block.type) {
          case 0x07: if ((event.block.c==4)&&!memcmp(event.block.v,"LOOP",4)) {
              if (ctx->looppp>0) {
                int loopp=ctx->dst->c-dstc0;
                if ((loopp<0)||(loopp>0xffff)) return -1;
                ((uint8_t*)ctx->dst->v)[ctx->looppp]=loopp>>8;
                ((uint8_t*)ctx->dst->v)[ctx->looppp+1]=loopp;
              }
            } break;
        } break;
    }
  }
  if (err<0) return fail(ctx,"Error streaming MIDI events.");
  eau_midi_write_delay(ctx,delay);
  if (ctx->holdc&&ctx->path) fprintf(stderr,"%s:WARNING: %d notes were not released.\n",ctx->path,ctx->holdc);
  return 0;
}

/* Stream events a second time, this time collecting into an "EVTS" chunk.
 */

static int eau_midi_events(struct eau_midi_context *ctx) {
  midi_file_reset(ctx->midi);
  sr_encode_raw(ctx->dst,"EVTS",4);
  int lenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0\0\0",4);
  int err=eau_midi_encode_events(ctx);
  if (err<0) return err;
  int len=ctx->dst->c-lenp-4;
  if (len<0) return -1;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>24;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>16;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp++]=len;
  return 0;
}

/* Convert, in context.
 */
 
static int eau_cvt_eau_midi_inner(struct eau_midi_context *ctx,const void *src,int srcc) {
  int err;
  if (!(ctx->midi=midi_file_new(src,srcc))) return fail(ctx,"Failed to decode MIDI file.");
  if ((err=eau_midi_collect_headers(ctx))<0) return err;
  if ((err=eau_midi_emit_headers(ctx))<0) return err;
  if ((err=eau_midi_events(ctx))<0) return err;
  if (sr_encoder_assert(ctx->dst)<0) return -1;
  return 0;
}

/* EAU from MIDI, main entry point.
 */
 
int eau_cvt_eau_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr) {
  struct eau_midi_context ctx={.dst=dst,.path=path,.get_chdr=get_chdr};
  int err=eau_cvt_eau_midi_inner(&ctx,src,srcc);
  eau_midi_context_cleanup(&ctx);
  return err;
}
