#include "eau.h"
#include "opt/serial/serial.h"
#include "opt/midi/midi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

/* Context.
 */
 
struct eau_from_midi_channel {
  uint8_t trim; // 0..255, NB not MIDI's 7-bit.
  uint8_t pan; // 0..128..255, NB not MIDI's 7-bit.
  uint32_t fqpid; // 0..2097151: Bank Select MSB, Bank Select LSB, and Program Change.
  int pgmset; // Nonzero if there was a Program Change at all (so we can distinguish explicit zero from unset).
  int notec;
  uint8_t notebits[16]; // Little-endian bits per noteid.
};
 
struct eau_from_midi {
  struct sr_encoder *dst;
  const void *src;
  int srcc;
  const char *path;
  eau_get_chdr_fn get_chdr;
  struct sr_encoder *errmsg;
  int status;
  int strip_names;
  struct midi_file *midi;
  int tempop; // Postition in (dst), points to 2 bytes.
  struct eau_from_midi_channel channelv[16];
  const void *chdr,*text; // WEAK; point into (src) for explicit chunks.
  int chdrc,textc;
};

static void eau_from_midi_cleanup(struct eau_from_midi *ctx) {
  midi_file_del(ctx->midi);
}

/* Logging.
 */
 
static int eau_from_midi_error(struct eau_from_midi *ctx,const char *fmt,...) {
  if (ctx->status<0) return ctx->status; // Already logged.
  if (ctx->errmsg||ctx->path) { // Logging enabled or captured.
    va_list vargs;
    va_start(vargs,fmt);
    char msg[256];
    int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
    if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
    while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--; // eg trailing LF, I might include by accident.
    if (ctx->errmsg) {
      sr_encode_fmt(ctx->errmsg,"%s: %.*s\n",ctx->path?ctx->path:"eau_from_midi",msgc,msg);
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

/* Names storage.
 */
 
static int eau_from_midi_add_name(struct eau_from_midi *ctx,int chid,int noteid,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  fprintf(stderr,"%s: %d,%d = '%.*s'\n",__func__,chid,noteid,srcc,src);//TODO
  return 0;
}

/* Generate one CHDR entry: Default tuned instrument.
 */
 
static int eau_from_midi_generate_default_instrument(struct eau_from_midi *ctx,int chid,struct eau_from_midi_channel *channel) {
  sr_encode_u8(ctx->dst,chid);
  sr_encode_u8(ctx->dst,channel->trim);
  sr_encode_u8(ctx->dst,channel->pan);
  sr_encode_u8(ctx->dst,1); // TRIVIAL
  sr_encode_intbe(ctx->dst,0,2); // modecfg len
  sr_encode_intbe(ctx->dst,0,2); // post len
  return 0;
}

/* Generate one entry in the default drum modecfg.
 */
 
static int eau_from_midi_generate_default_drum(struct eau_from_midi *ctx,int noteid) {

  //TODO Try to use the SDK's default drum kit.

  sr_encode_u8(ctx->dst,noteid);
  sr_encode_u8(ctx->dst,0x40); // trimlo
  sr_encode_u8(ctx->dst,0xff); // trimhi
  sr_encode_u8(ctx->dst,0x80); // pan
  int lenp=ctx->dst->c;
  if (sr_encode_intbe(ctx->dst,0,2)<0) return -1;
  
  // Inner content is another EAU file.
  const uint8_t inner_chdr[]={
    0x00,0xff,0x80, // chid,trim,pan
    2, // mode=FM
    0,21, // modecfglen
      0, // levelenv, accept default
      0,0, // wheelrange, irrelevant
      0x02,0x20,0x00, // wavea
      0, // waveb, irrelevant
      0, // mixenv, accept default, ie wavea only
      0,0,0,0, // modrate,modrange: modulation off
      0, // rangeenv, irrelevant
        0x09, // pitchenv flags: Initials,Present
        0x80,0x00, // pitchenv initlo
        0x01, // pitchenv susp|ptc
          0x01,0x00, 0x78,0x00, // end point
    0,0, // postlen
  };
  const uint8_t inner_events[]={
    0x90,noteid,0x40, // Note On (forgoing Note Once for legibility's sake)
    0x80,noteid,0x40, // Note Off
    0x44, // Delay 320 ms
  };
  sr_encode_raw(ctx->dst,"\0EAU",4);
  sr_encode_intbe(ctx->dst,500,2); // tempo
  sr_encode_intbe(ctx->dst,sizeof(inner_chdr),4);
  sr_encode_raw(ctx->dst,inner_chdr,sizeof(inner_chdr));
  sr_encode_intbe(ctx->dst,sizeof(inner_events),4);
  sr_encode_raw(ctx->dst,inner_events,sizeof(inner_events));
  
  int len=ctx->dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return eau_from_midi_error(ctx,"Invalid length %d for drum %d.",len,noteid);
  ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len;
  
  return 0;
}

/* Generate one CHDR entry: Default drum kit.
 */
 
static int eau_from_midi_generate_default_drums(struct eau_from_midi *ctx,int chid,struct eau_from_midi_channel *channel) {
  sr_encode_u8(ctx->dst,chid);
  sr_encode_u8(ctx->dst,channel->trim);
  sr_encode_u8(ctx->dst,channel->pan);
  sr_encode_u8(ctx->dst,4); // DRUM;
  
  int modecfglenp=ctx->dst->c;
  if (sr_encode_intbe(ctx->dst,0,2)<0) return -1;
  int major=0;
  for (;major<16;major++) {
    if (!channel->notebits[major]) continue;
    int minor=0,mask=1;
    for (;minor<8;minor++,mask<<=1) {
      if (!(channel->notebits[major]&mask)) continue;
      int noteid=(major<<3)|minor;
      int err=eau_from_midi_generate_default_drum(ctx,noteid);
      if (err<0) return err;
    }
  }
  int modecfglen=ctx->dst->c-modecfglenp-2;
  if ((modecfglen<0)||(modecfglen>0xffff)) return eau_from_midi_error(ctx,"Invalid length %d for drums modecfg.",modecfglen);
  ((uint8_t*)ctx->dst->v)[modecfglenp]=modecfglen>>8;
  ((uint8_t*)ctx->dst->v)[modecfglenp+1]=modecfglen;
  
  sr_encode_intbe(ctx->dst,0,2); // post len
  return 0;
}

/* Generate one CHDR entry for a channel with explicit program.
 */
 
static int eau_from_midi_generate_chdr_from_fqpid(struct eau_from_midi *ctx,int chid,struct eau_from_midi_channel *channel) {

  // Look up (channel->fqpid) in the SDK's standard instruments.
  fprintf(stderr,"[%s:%d] TODO Look up fqpid 0x%08x\n",__FILE__,__LINE__,channel->fqpid);//TODO
  
  // If it's in Bank One, assume it's a drum kit.
  if ((channel->fqpid&0x00003f80)==0x00000080) {
    return eau_from_midi_generate_default_drums(ctx,chid,channel);
  }
  
  // Anything else, use the default instrument.
  return eau_from_midi_generate_default_instrument(ctx,chid,channel);
}

/* Encode the Channel Headers block and capture text into the context.
 * Caller is responsible for the length field.
 */
 
static int eau_from_midi_chdr(struct eau_from_midi *ctx) {
  midi_file_reset(ctx->midi);
  
  /* Set initial defaults for channels.
   */
  memset(ctx->channelv,0,sizeof(ctx->channelv));
  struct eau_from_midi_channel *channel=ctx->channelv;
  int i=16;
  for (;i-->0;channel++) {
    channel->trim=0x40;
    channel->pan=0x80;
  }
  
  /* Scan time-zero events for Set Tempo, Channel Headers, and Text.
   */
  for (;;) {
    struct midi_event event={0};
    if (midi_file_next(&event,ctx->midi)) break; // Error, EOF, or Delay.
    if (event.chid<16) channel=ctx->channelv+event.chid;
    else channel=0;
    switch (event.opcode) {
      case 0x90: { // Note On
          if (channel&&(event.a<0x80)) {
            channel->notec++;
            channel->notebits[event.a>>3]|=1<<(event.a&7);
          }
        } break;
      case 0xb0: {
          if (!channel) break;
          switch (event.a) { // Control Change
            case 0x00: channel->fqpid=(channel->fqpid&0xffe03fff)|(event.b<<14); break; // Bank Select MSB
            case 0x07: channel->trim=(event.b<<1)|(event.b&1); break; // Volume MSB
            case 0x0a: channel->pan=(event.b<<1)|(event.b&1); break; // Pan MSB
            case 0x20: channel->fqpid=(channel->fqpid&0xffffc07f)|(event.b<<7); break; // Bank Select LSB
          }
        } break;
      case 0xc0: { // Program Change
          if (!channel) break;
          channel->pgmset=1;
          channel->fqpid=(channel->fqpid&0xffffff80)|event.a;
        } break;
      case MIDI_OPCODE_META: switch (event.a) {
          case 0x03: case 0x04: { // Instrument Name, Track Name.
              if (!channel) break;
              eau_from_midi_add_name(ctx,event.chid,0xff,event.v,event.c);
            } break;
          case 0x51: { // Set Tempo.
              if (event.c!=3) break;
              const uint8_t *V=event.v;
              int usperqnote=(V[0]<<16)|(V[1]<<8)|V[2];
              int msperqnote=usperqnote/1000;
              ((uint8_t*)ctx->dst->v)[ctx->tempop+0]=msperqnote>>8;
              ((uint8_t*)ctx->dst->v)[ctx->tempop+1]=msperqnote;
            } break;
          case 0x77: { // Egg Channel Headers.
              if (ctx->chdr) return eau_from_midi_error(ctx,"Multiple Meta 0x77 Egg Channel Headers.");
              ctx->chdr=event.v;
              ctx->chdrc=event.c;
            } break;
          case 0x78: { // Egg Text.
              if (ctx->text) return eau_from_midi_error(ctx,"Multiple Meta 0x78 Egg Text.");
              ctx->text=event.v;
              ctx->textc=event.c;
            } break;
        } break;
    }
  }
  
  /* If there's an explicit Meta 0x77 Egg Channel Headers, emit it and we're done.
   * Our other job, collecting text, that's already done too.
   */
  if (ctx->chdr) {
    return sr_encode_raw(ctx->dst,ctx->chdr,ctx->chdrc);
  }
  
  /* Continue scanning thru EOF, looking only for Note On and counting per channel.
   */
  for (;;) {
    struct midi_event event={0};
    int err=midi_file_next(&event,ctx->midi);
    if (err<0) break;
    if (err) {
      midi_file_advance(ctx->midi,err);
      continue;
    }
    switch (event.opcode) {
      case 0x90: {
          if ((event.chid<16)&&(event.a<0x80)) {
            channel=ctx->channelv+event.chid;
            channel->notec++;
            channel->notebits[event.a>>3]|=1<<(event.a&7);
          }
        } break;
    }
  }
  
  /* Generate a Channel Header for any channel with notes on it.
   * If a channel only contains wheel or control events, we drop it.
   */
  int chid=0,err;
  for (channel=ctx->channelv;chid<16;chid++,channel++) {
    if (!channel->notec) continue;
    if (channel->pgmset) {
      if ((err=eau_from_midi_generate_chdr_from_fqpid(ctx,chid,channel))<0) return err;
    } else if (chid==9) {
      if ((err=eau_from_midi_generate_default_drums(ctx,chid,channel))<0) return err;
    } else {
      if ((err=eau_from_midi_generate_default_instrument(ctx,chid,channel))<0) return err;
    }
  }
  
  return 0;
}

/* Stream events and translate into output.
 * Caller is responsible for the length field.
 */
 
static int eau_from_midi_events(struct eau_from_midi *ctx) {
  midi_file_reset(ctx->midi);
  
  int now=0; // elapsed ms per input
  int wtime=0; // written delay, <=now.
  #define FLUSH_DELAY { \
    int delay=now-wtime; \
    if (delay>0) { \
      while (delay>=4096) { \
        sr_encode_u8(ctx->dst,0x7f); \
        delay-=4096; \
      } \
      if (delay>=64) { \
        sr_encode_u8(ctx->dst,0x40|((delay>>6)-1)); \
        delay&=0x3f; \
      } \
      if (delay>0) { \
        sr_encode_u8(ctx->dst,delay); \
      } \
      wtime=now; \
    } \
  }
  
  for (;;) {
    struct midi_event event={0};
    int err=midi_file_next(&event,ctx->midi);
    if (err<0) break; // Error or EOF.
    if (err>0) {
      now+=err;
      if (midi_file_advance(ctx->midi,err)<0) return eau_from_midi_error(ctx,"Unknown error reading MIDI file.");
      continue;
    }
    switch (event.opcode) {
    
      case 0x80: { // Note Off
          //TODO Track holds and produce Note Once events where possible. For now, using only explicit Note On / Note Off (which is legal but wasteful).
          FLUSH_DELAY
          sr_encode_u8(ctx->dst,0x80|event.chid);
          sr_encode_u8(ctx->dst,event.a);
          sr_encode_u8(ctx->dst,event.b);
        } break;
        
      case 0x90: { // Note On
          //TODO Track holds and produce Note Once events where possible. For now, using only explicit Note On / Note Off (which is legal but wasteful).
          FLUSH_DELAY
          sr_encode_u8(ctx->dst,0x90|event.chid);
          sr_encode_u8(ctx->dst,event.a);
          sr_encode_u8(ctx->dst,event.b);
        } break;
        
      case 0xe0: { // Wheel. EAU matches MIDI exactly.
          FLUSH_DELAY
          sr_encode_u8(ctx->dst,0xe0|event.chid);
          sr_encode_u8(ctx->dst,event.a);
          sr_encode_u8(ctx->dst,event.b);
        } break;
        
      case MIDI_OPCODE_META: switch (event.a) {
          case 0x07: { // Meta 0x07 Cue Point
              if ((event.c==4)&&!memcmp(event.v,"LOOP",4)) {
                FLUSH_DELAY
                sr_encode_u8(ctx->dst,0xf0); // Marker:Loop
              }
            } break;
        } break;
    }
  }
  if (!midi_file_is_finished(ctx->midi)) return eau_from_midi_error(ctx,"Unknown error reading MIDI file.");
  FLUSH_DELAY
  #undef FLUSH_DELAY
  return 0;
}

/* Encode the entire Text block, from content captured during eau_from_midi_chdr().
 * Caller is responsible for the length field.
 */
 
static int eau_from_midi_text(struct eau_from_midi *ctx) {
  fprintf(stderr,"TODO %s [%s:%d]\n",__func__,__FILE__,__LINE__);//TODO
  return 0;
}

/* EAU from MIDI, in context.
 */
 
static int eau_from_midi_inner(struct eau_from_midi *ctx) {
  int err;

  // Create the MIDI stream. "1000" means it will report timing in milliseconds, convenient for EAU.
  if (!(ctx->midi=midi_file_new(ctx->src,ctx->srcc,1000))) {
    return eau_from_midi_error(ctx,"Failed to decode MIDI file.");
  }
  
  // Output begins with signature and a default tempo. We may return here to update the tempo.
  sr_encode_raw(ctx->dst,"\0EAU",4);
  ctx->tempop=ctx->dst->c;
  sr_encode_intbe(ctx->dst,ctx->midi->usperqnote/1000,2);
  
  /* Channel headers length then payload.
   * This will makes its own pass over the input, but stops reading after time zero.
   */
  int chdrlenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  if ((err=eau_from_midi_chdr(ctx))<0) return err;
  int chdrlen=ctx->dst->c-chdrlenp-4;
  if (chdrlen<0) return eau_from_midi_error(ctx,"Impossible length %d for channel headers.",chdrlen);
  ((uint8_t*)ctx->dst->v)[chdrlenp+0]=chdrlen>>24;
  ((uint8_t*)ctx->dst->v)[chdrlenp+1]=chdrlen>>16;
  ((uint8_t*)ctx->dst->v)[chdrlenp+2]=chdrlen>>8;
  ((uint8_t*)ctx->dst->v)[chdrlenp+3]=chdrlen;
  
  /* Then the same idea, for events.
   */
  int evtlenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  if ((err=eau_from_midi_events(ctx))<0) return err;
  int evtlen=ctx->dst->c-evtlenp-4;
  if (evtlen<0) return eau_from_midi_error(ctx,"Impossible length %d for events block.",chdrlen);
  ((uint8_t*)ctx->dst->v)[evtlenp+0]=evtlen>>24;
  ((uint8_t*)ctx->dst->v)[evtlenp+1]=evtlen>>16;
  ((uint8_t*)ctx->dst->v)[evtlenp+2]=evtlen>>8;
  ((uint8_t*)ctx->dst->v)[evtlenp+3]=evtlen;
  
  /* And finally text, if we're doing that.
   * Content is collected into the context during the headers pass.
   */
  int textlenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  if (!ctx->strip_names) {
    if ((err=eau_from_midi_text(ctx))<0) return err;
    int textlen=ctx->dst->c-textlenp-4;
    if (textlen<0) return eau_from_midi_error(ctx,"Impossible length %d for text block.",chdrlen);
    ((uint8_t*)ctx->dst->v)[textlenp+0]=textlen>>24;
    ((uint8_t*)ctx->dst->v)[textlenp+1]=textlen>>16;
    ((uint8_t*)ctx->dst->v)[textlenp+2]=textlen>>8;
    ((uint8_t*)ctx->dst->v)[textlenp+3]=textlen;
  }
  
  return sr_encoder_assert(ctx->dst);
}

/* EAU from MIDI, main entry point.
 */
 
int eau_cvt_eau_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg) {
  struct eau_from_midi ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .path=path,
    .get_chdr=get_chdr,
    .strip_names=strip_names,
    .errmsg=errmsg,
  };
  int err=eau_from_midi_inner(&ctx);
  eau_from_midi_cleanup(&ctx);
  return err;
}
