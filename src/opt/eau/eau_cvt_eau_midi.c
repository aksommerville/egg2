#include "eau.h"
#include "opt/serial/serial.h"
#include "opt/midi/midi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

eau_get_chdr_fn eau_get_chdr=0;

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

struct eau_from_midi_event {
  int time;
  uint8_t chid;
  uint8_t opcode; // 0x80,0x90,0xa0,0xe0,0xfx
  uint8_t a,b; // (noteid,velocity) for Note Off, Note On, Note Once. Verbatim params for Wheel.
  int durms; // For Note Once. Also for Note On, set nonzero to indicate an explicit Note Off is provided.
};
 
struct eau_from_midi {
  struct sr_convert_context *cvtctx;
  struct sr_encoder *dst; // Same as (cvtctx->dst), just repeating here because it was already written that way.
  eau_get_chdr_fn get_chdr;
  int status;
  int strip_names;
  struct midi_file *midi;
  int tempop; // Postition in (dst), points to 2 bytes.
  struct eau_from_midi_channel channelv[16];
  const void *chdr,*text; // WEAK; point into (src) for explicit chunks.
  int chdrc,textc;
  struct eau_from_midi_name {
    uint8_t chid,noteid;
    const char *name; // WEAK; points into (src).
    int namec;
  } *namev;
  int namec,namea;
  struct eau_from_midi_event *eventv; // Used transiently while encoding events; stored in context mostly as a convenience.
  int eventc,eventa;
  const void *chdr80; // Default drum kit, loaded once if we need it.
  int chdr80c; // <0 if we tried and failed.
};

static void eau_from_midi_cleanup(struct eau_from_midi *ctx) {
  midi_file_del(ctx->midi);
  if (ctx->namev) free(ctx->namev);
  if (ctx->eventv) free(ctx->eventv);
}

/* Names list primitives.
 */
 
static int eau_from_midi_search_name(const struct eau_from_midi *ctx,int chid,int noteid) {
  int lo=0,hi=ctx->namec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eau_from_midi_name *name=ctx->namev+ck;
         if (chid<name->chid) hi=ck;
    else if (chid>name->chid) lo=ck+1;
    else if (noteid<name->noteid) hi=ck;
    else if (noteid>name->noteid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct eau_from_midi_name *eau_from_midi_insert_name(struct eau_from_midi *ctx,int p,int chid,int noteid) {
  if ((p<0)||(p>ctx->namec)) return 0;
  if (ctx->namec>=ctx->namea) {
    int na=ctx->namea+32;
    if (na>INT_MAX/sizeof(struct eau_from_midi_name)) return 0;
    void *nv=realloc(ctx->namev,sizeof(struct eau_from_midi_name)*na);
    if (!nv) return 0;
    ctx->namev=nv;
    ctx->namea=na;
  }
  struct eau_from_midi_name *name=ctx->namev+p;
  memmove(name+1,name,sizeof(struct eau_from_midi_name)*(ctx->namec-p));
  ctx->namec++;
  name->chid=chid;
  name->noteid=noteid;
  name->name=0;
  name->namec=0;
  return name;
}

/* Names storage.
 * (src) is borrowed; it must be held constant.
 */
 
static int eau_from_midi_add_name(struct eau_from_midi *ctx,int chid,int noteid,const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  if ((chid<0)||(chid>0xff)||(noteid<0)||(noteid>0xff)) return 0;
  if (srcc>0xff) srcc=0xff;
  int p=eau_from_midi_search_name(ctx,chid,noteid);
  if (p>=0) {
    struct eau_from_midi_name *name=ctx->namev+p;
    name->name=src;
    name->namec=srcc;
    return 0;
  }
  p=-p-1;
  struct eau_from_midi_name *name=eau_from_midi_insert_name(ctx,p,chid,noteid);
  if (!name) return -1;
  name->name=src;
  name->namec=srcc;
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

/* Locate drum in a drum channel header.
 */
 
static int eau_from_midi_find_drum(void *dstpp,const uint8_t *src,int srcc,int noteid) {
  // (src) is the entire channel header. Trim to its modecfg.
  if (srcc<6) return 0;
  int modecfgc=(src[4]<<8)|src[5];
  if (modecfgc>srcc-6) return 0;
  src=src+6;
  srcc=modecfgc;
  // OK, now read it:
  int srcp=0;
  for (;;) {
    if (srcp>srcc-6) return 0;
    int len=(src[srcp+4]<<8)|src[srcp+5];
    if (srcp>srcc-len-6) return 0;
    if (src[srcp]==noteid) {
      *(const void**)dstpp=src+srcp;
      return 6+len;
    }
    srcp+=6+len;
  }
  return 0;
}

/* Generate one entry in the default drum modecfg.
 */
 
static int eau_from_midi_generate_default_drum(struct eau_from_midi *ctx,int noteid) {

  /* First look in fqpid 0x80, the default drum kit.
   */
  if (!ctx->chdr80c) {
    if (!ctx->get_chdr) ctx->chdr80c=-1;
    else {
      const void *src=0;
      int srcc=ctx->get_chdr(&src,0x80);
      if (srcc>0) {
        ctx->chdr80=src;
        ctx->chdr80c=srcc;
      } else {
        ctx->chdr80c=-1;
      }
    }
  }
  if (ctx->chdr80c>0) {
    const void *src=0;
    int srcc=eau_from_midi_find_drum(&src,ctx->chdr80,ctx->chdr80c,noteid);
    if (srcc>0) {
      return sr_encode_raw(ctx->dst,src,srcc);
    }
  }

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
  if ((len<0)||(len>0xffff)) return sr_convert_error(ctx->cvtctx,"Invalid length %d for drum %d.",len,noteid);
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
  sr_encode_u8(ctx->dst,4); // DRUM
  
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
  if ((modecfglen<0)||(modecfglen>0xffff)) return sr_convert_error(ctx->cvtctx,"Invalid length %d for drums modecfg.",modecfglen);
  ((uint8_t*)ctx->dst->v)[modecfglenp]=modecfglen>>8;
  ((uint8_t*)ctx->dst->v)[modecfglenp+1]=modecfglen;
  
  sr_encode_intbe(ctx->dst,0,2); // post len
  return 0;
}

/* Generate one CHDR entry for a channel with explicit program.
 */
 
static int eau_from_midi_generate_chdr_from_fqpid(struct eau_from_midi *ctx,int chid,struct eau_from_midi_channel *channel) {

  // Look up (channel->fqpid) in the SDK's standard instruments.
  if (ctx->get_chdr) {
    const uint8_t *src=0;
    int srcc=ctx->get_chdr(&src,channel->fqpid);
    if (srcc>=8) {
      // Ignore the SDK's first three bytes.
      sr_encode_u8(ctx->dst,chid);
      sr_encode_u8(ctx->dst,channel->trim);
      sr_encode_u8(ctx->dst,channel->pan);
      // Everything else, framing included, take it on faith.
      sr_encode_raw(ctx->dst,src+3,srcc-3);
      return 0;
    } else if (srcc>0) {
      return sr_convert_error(ctx->cvtctx,"Invalid length %d for fqpid 0x%08x returned from SDK.",srcc,channel->fqpid);
    } else {
      // SDK lookup failed. No big deal, proceed with defaults.
    }
  }
  
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
              if (ctx->chdr) return sr_convert_error(ctx->cvtctx,"Multiple Meta 0x77 Egg Channel Headers.");
              ctx->chdr=event.v;
              ctx->chdrc=event.c;
            } break;
          case 0x78: { // Egg Text.
              if (ctx->text) return sr_convert_error(ctx->cvtctx,"Multiple Meta 0x78 Egg Text.");
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
    if (channel->pgmset) { // They asked for a specific program, so it's either that or the default.
      if ((err=eau_from_midi_generate_chdr_from_fqpid(ctx,chid,channel))<0) return err;
    } else if (chid==9) { // On channel 9, attempt a drum kit.
      if ((err=eau_from_midi_generate_default_drums(ctx,chid,channel))<0) return err;
    } else { // Any other channel with no program change, pretend they asked for program zero.
      if ((err=eau_from_midi_generate_chdr_from_fqpid(ctx,chid,channel))<0) return err;
    }
  }
  
  return 0;
}

/* Append a blank event to our list.
 */
 
static struct eau_from_midi_event *eau_from_midi_add_event(struct eau_from_midi *ctx) {
  if (ctx->eventc>=ctx->eventa) {
    int na=ctx->eventa+256;
    if (na>INT_MAX/sizeof(struct eau_from_midi_event)) return 0;
    void *nv=realloc(ctx->eventv,sizeof(struct eau_from_midi_event)*na);
    if (!nv) return 0;
    ctx->eventv=nv;
    ctx->eventa=na;
  }
  struct eau_from_midi_event *event=ctx->eventv+ctx->eventc++;
  memset(event,0,sizeof(struct eau_from_midi_event));
  return event;
}

/* Find a Note On for the described Note Off.
 */
 
static struct eau_from_midi_event *eau_from_midi_find_note_on(struct eau_from_midi *ctx,uint8_t chid,uint8_t noteid) {
  struct eau_from_midi_event *event=ctx->eventv;
  int i=ctx->eventc;
  for (;i-->0;event++) {
    if (event->opcode!=0x90) continue;
    if (event->chid!=chid) continue;
    if (event->a!=noteid) continue;
    if (event->durms) continue;
    return event;
  }
  return 0;
}

/* Stream events and translate into output.
 * Caller is responsible for the length field.
 */
 
static int eau_from_midi_events(struct eau_from_midi *ctx) {

  /* Build up (ctx->eventv) by streaming the entire MIDI file.
   * Unnecessary events are dropped, and we turn On/Off pairs into Once wherever possible.
   * If we finish with any Note On with (durms==0), it was not Offed.
   */
  midi_file_reset(ctx->midi);
  ctx->eventc=0;
  int now=0;
  for (;;) {
    struct midi_event event={0};
    int err=midi_file_next(&event,ctx->midi);
    if (err<0) break;
    if (err>0) {
      now+=err;
      if (midi_file_advance(ctx->midi,err)<0) return sr_convert_error(ctx->cvtctx,"Unknown error reading MIDI file.");
      continue;
    }
    switch (event.opcode) {
    
      case 0x80: { // Note Off. Search for the corresponding Note On, and maybe (usually) turn it into Once.
          struct eau_from_midi_event *onevent=eau_from_midi_find_note_on(ctx,event.chid,event.a);
          if (!onevent) break; // No Note On. Drop it.
          int durms=now-onevent->time;
          if (durms<=16368) { // Change to Note Once -- the usual case.
            onevent->opcode=0xa0;
            onevent->durms=durms;
          } else { // >16s long hold. Emit as an On/Off pair. We already have the On.
            onevent->durms=1; // Mark the On as permanent.
            struct eau_from_midi_event *offevent=eau_from_midi_add_event(ctx);
            if (!offevent) return -1;
            offevent->time=now;
            offevent->opcode=0x80;
            offevent->chid=event.chid;
            offevent->a=event.a;
            offevent->b=event.b;
          }
        } break;
        
      case 0x90: case 0xe0: { // Note On, Wheel. Record as is.
          struct eau_from_midi_event *dst=eau_from_midi_add_event(ctx);
          if (!dst) return -1;
          dst->time=now;
          dst->opcode=event.opcode;
          dst->chid=event.chid;
          dst->a=event.a;
          dst->b=event.b;
          dst->durms=0;
        } break;
        
      case MIDI_OPCODE_META: switch (event.a) {
          case 0x07: { // Cue Point
              if ((event.c==4)&&!memcmp(event.v,"LOOP",4)) {
                struct eau_from_midi_event *dst=eau_from_midi_add_event(ctx);
                if (!dst) return -1;
                dst->time=now;
                dst->opcode=0xf0;
                dst->chid=0xff;
              }
            } break;
        } break;
        
      // Other events are perfectly legal, and ignored.
    }
  }
  if (!midi_file_is_finished(ctx->midi)) return sr_convert_error(ctx->cvtctx,"Unknown error reading MIDI file.");
  
  /* Encode, inserting delays and forcing unpaired Note On to duration zero.
   */
  struct eau_from_midi_event *event=ctx->eventv;
  int i=ctx->eventc;
  int wtime=0;
  for (;i-->0;event++) {
  
    // Unpaired Note On.
    if ((event->opcode==0x90)&&!event->durms) {
      event->opcode=0xa0;
    }
    
    // Delay.
    int delay=event->time-wtime;
    while (delay>4096) {
      sr_encode_u8(ctx->dst,0x7f);
      delay-=4096;
    }
    if (delay>0x3f) {
      sr_encode_u8(ctx->dst,0x40|((delay>>6)-1));
      delay&=0x3f;
    }
    if (delay>0) {
      sr_encode_u8(ctx->dst,delay);
    }
    wtime=event->time;
    
    switch (event->opcode) {
      case 0x80: case 0x90: case 0xe0: { // Note Off, Note On, Wheel: Verbatim, with 2 param bytes.
          sr_encode_u8(ctx->dst,event->opcode|event->chid);
          sr_encode_u8(ctx->dst,event->a);
          sr_encode_u8(ctx->dst,event->b);
        } break;
      case 0xa0: { // Note Once.
          int dur=event->durms>>4;
          if (dur<0) dur=0;
          else if (dur>0x3ff) dur=0x3ff;
          sr_encode_u8(ctx->dst,event->opcode|event->chid);
          sr_encode_u8(ctx->dst,(event->a<<1)|(event->b>>6));
          sr_encode_u8(ctx->dst,(event->b<<2)|(dur>>8));
          sr_encode_u8(ctx->dst,dur);
        } break;
      default: { // Must be 0xfx Marker. Verbatim.
          sr_encode_u8(ctx->dst,event->opcode);
        }
    }
  }
  
  /* If any delay remains unwritten, write it.
   * Unlike MIDI, EAU has discrete Delay events, so it's no big deal here.
   * (now) remains the total song time.
   */
  int delay=now-wtime;
  while (delay>4096) {
    sr_encode_u8(ctx->dst,0x7f);
    delay-=4096;
  }
  if (delay>0x3f) {
    sr_encode_u8(ctx->dst,0x40|((delay>>6)-1));
    delay&=0x3f;
  }
  if (delay>0) {
    sr_encode_u8(ctx->dst,delay);
  }
  
  return 0;
}

/* Encode the entire Text block, from content captured during eau_from_midi_chdr().
 * Caller is responsible for the length field.
 */
 
static int eau_from_midi_text(struct eau_from_midi *ctx) {
  
  // If Meta 0x78 Egg Text exists, use it verbatim and nothing else.
  if (ctx->text) return sr_encode_raw(ctx->dst,ctx->text,ctx->textc);
  
  // Synthesize from (namev).
  const struct eau_from_midi_name *name=ctx->namev;
  int i=ctx->namec;
  for (;i-->0;name++) {
    sr_encode_u8(ctx->dst,name->chid);
    sr_encode_u8(ctx->dst,name->noteid);
    sr_encode_u8(ctx->dst,name->namec);
    sr_encode_raw(ctx->dst,name->name,name->namec);
  }
    
  return 0;
}

/* EAU from MIDI, in context.
 */
 
static int eau_from_midi_inner(struct eau_from_midi *ctx) {
  int err;

  // Create the MIDI stream. "1000" means it will report timing in milliseconds, convenient for EAU.
  if (!(ctx->midi=midi_file_new(ctx->cvtctx->src,ctx->cvtctx->srcc,1000))) {
    return sr_convert_error(ctx->cvtctx,"Failed to decode MIDI file.");
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
  if (chdrlen<0) return sr_convert_error(ctx->cvtctx,"Impossible length %d for channel headers.",chdrlen);
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
  if (evtlen<0) return sr_convert_error(ctx->cvtctx,"Impossible length %d for events block.",chdrlen);
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
    if (textlen<0) return sr_convert_error(ctx->cvtctx,"Impossible length %d for text block.",chdrlen);
    ((uint8_t*)ctx->dst->v)[textlenp+0]=textlen>>24;
    ((uint8_t*)ctx->dst->v)[textlenp+1]=textlen>>16;
    ((uint8_t*)ctx->dst->v)[textlenp+2]=textlen>>8;
    ((uint8_t*)ctx->dst->v)[textlenp+3]=textlen;
  }
  
  return sr_encoder_assert(ctx->dst);
}

/* EAU from MIDI, main entry point.
 */
 
int eau_cvt_eau_midi(struct sr_convert_context *cvtctx) {
  struct eau_from_midi ctx={
    .cvtctx=cvtctx,
    .dst=cvtctx->dst,
    .get_chdr=eau_get_chdr,
  };
  sr_convert_arg_int(&ctx.strip_names,cvtctx,"strip",5);
  int err=eau_from_midi_inner(&ctx);
  eau_from_midi_cleanup(&ctx);
  return err;
}
