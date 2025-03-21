/* eggdev_convert_audio.c
 * Various audio conversions.
 */

#include "eggdev/eggdev_internal.h"
#include "opt/midi/midi.h"
#include "opt/synth/synth.h"
#include "opt/synth/eau.h"

/* WAV from EAU: Stand a synthesizer and print it.
 */
 
int eggdev_wav_from_eau(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* EAU bin from text.
 */
 
int eggdev_eau_from_eaut(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* EAU text from bin.
 */
 
int eggdev_eaut_from_eau(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Copy a drum Channel Header, starting at (mode).
 * Don't just dump the whole payload -- check the provided list of notes actually used.
 */
 
static int eggdev_eau_channel_copy_drums(struct eggdev_convert_context *ctx,struct eau_channel_entry *channel,const uint8_t *notev) {
  if (sr_encode_u8(ctx->dst,EAU_CHANNEL_MODE_DRUM)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  const uint8_t *src=channel->payload;
  int srcc=channel->payloadc;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) break;
    int sublen=(src[srcp+4]<<8)|src[srcp+5];
    if (srcp>srcc-sublen-6) break;
    if (!(src[srcp]&0x80)&&notev[src[srcp]]) {
      if (sr_encode_raw(ctx->dst,src+srcp,6+sublen)<0) return -1;
    }
    srcp+=6+sublen;
  }
  int len=ctx->dst->c-lenp-2;
  if ((len<0)||(len>0xffff)) return -1;
  ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp+1]=len;
  if (sr_encode_intbelen(ctx->dst,channel->post,channel->postc,2)<0) return -1;
  return 0;
}

/* Make up an EAU Channel Header for some MIDI channel.
 * Emit just (mode,payload,post) -- (chid,trim,pan) are already emitted.
 */
 
static int eggdev_eau_default_channel_header(struct eggdev_convert_context *ctx,uint8_t chid,uint8_t bank,uint8_t pid,const uint8_t *notev) {

  /* The SDK's instrument set should contain GM in 0..127, then a GM drum kit at 128.
   * We might install other instruments in 129..254, they're available.
   * The low bit of Bank selects those upper programs.
   */
  pid|=bank<<7;
  
  /* If (bank,pid) both zero and (chid) 9, assume it's a drum kit.
   */
  if ((chid==9)&&!bank&&!pid) pid=128;
  fprintf(stderr,"%s: Looking up pid %d for channel %d...\n",ctx->refname,pid,chid);
  
  /* Best case scenario: The instrument is defined by SDK.
   */
  const void *src=0;
  int srcc=eggdev_config_get_instruments(&src);
  struct eau_channel_entry program0={0};
  if (srcc>0) {
    struct eau_file file={0};
    if (eau_file_decode(&file,src,srcc)>=0) {
      struct eau_channel_reader reader={.v=file.chhdrv,.c=file.chhdrc};
      struct eau_channel_entry channel;
      while (eau_channel_reader_next(&channel,&reader)>0) {
        if (channel.chid==pid) {
          if (channel.mode==EAU_CHANNEL_MODE_DRUM) {
            return eggdev_eau_channel_copy_drums(ctx,&channel,notev);
          } else {
            if (sr_encode_u8(ctx->dst,channel.mode)<0) return -1;
            if (sr_encode_intbelen(ctx->dst,channel.payload,channel.payloadc,2)<0) return -1;
            if (sr_encode_intbelen(ctx->dst,channel.post,channel.postc,2)<0) return -1;
            return 0;
          }
        } else if (!channel.chid) {
          program0=channel;
        }
      }
    }
  }
  
  /* If the SDK defines instrument zero, which it really should, use that.
   */
  if (program0.mode&&(program0.mode!=EAU_CHANNEL_MODE_DRUM)) {
    if (sr_encode_u8(ctx->dst,program0.mode)<0) return -1;
    if (sr_encode_intbelen(ctx->dst,program0.payload,program0.payloadc,2)<0) return -1;
    if (sr_encode_intbelen(ctx->dst,program0.post,program0.postc,2)<0) return -1;
    return 0;
  }
  
  /* And finally a single canned bare-minimum instrument.
   * This is never the instrument you want.
   */
  if (sr_encode_u8(ctx->dst,EAU_CHANNEL_MODE_FM)<0) return -1;
  const uint8_t payload[]={
    0x04,1,3, // Level env. Sustain, no velocity.
      0x00,0x10, 0xf0,0x00,
      0x00,0x20, 0x40,0x00,
      0x01,0x00, 0x00,0x00,
    0x00,0x00,0, // Wave: Sine.
    0,0, // Pitch env: noop
    0x01,0x00, // Wheel range: 256 cents.
  };
  if (sr_encode_intbelen(ctx->dst,payload,sizeof(payload),2)<0) return -1;
  if (sr_encode_raw(ctx->dst,"\0\0",2)<0) return -1;
  return 0;
}

/* Validate EAU header chunk, from a MIDI Meta 0x77 (our private thing).
 */
 
static int eggdev_eau_chhdrv_validate(const void *src,int srcc) {
  struct eau_channel_reader reader={.v=src,.c=srcc};
  struct eau_channel_entry entry;
  int err;
  while ((err=eau_channel_reader_next(&entry,&reader))>0) ;
  return err;
}

/* EAU from MIDI, header.
 */
 
static int eggdev_eau_from_mid_header(struct eggdev_convert_context *ctx,struct midi_file *midi) {
  int dstc0=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0EAU\x01\xf4\0\0",8)<0) return -1; // 500 ms/qnote until we hear otherwise.
  
  struct midi_controls {
    uint8_t bank;
    uint8_t pid;
    uint8_t trim;
    uint8_t pan;
    int notec;
    uint8_t notev[128];
  } midi_controls[16]={0};
  struct midi_controls *controls=midi_controls;
  int i=16; for (;i-->0;controls++) {
    controls->trim=0x40;
    controls->pan=0x40;
  }
  const void *chhdrv=0;
  int chhdrc=0;
  
  for (;;) {
    struct midi_event event;
    int err=midi_file_next(&event,midi);
    if (err<0) break;
    if (err>0) {
      if (midi_file_advance(midi,err)<0) break;
    }
    switch (event.opcode) {
      case MIDI_OPCODE_META: switch (event.a) {
          case MIDI_META_SET_TEMPO: if (event.c>=3) {
              const uint8_t *src=event.v;
              int usperqnote=(src[0]<<16)|(src[1]<<8)|src[2];
              int msperqnote=usperqnote/1000;
              if (msperqnote<1) msperqnote=1;
              else if (msperqnote>0xffff) msperqnote=0xffff;
              ((uint8_t*)ctx->dst->v)[dstc0+4]=msperqnote>>8;
              ((uint8_t*)ctx->dst->v)[dstc0+5]=msperqnote;
            } break;
          case 0x77: if (eggdev_eau_chhdrv_validate(event.v,event.c)>=0) {
              chhdrv=event.v;
              chhdrc=event.c;
            } break;
        } break;
      case MIDI_OPCODE_PROGRAM: midi_controls[event.chid].pid=event.a; break;
      case MIDI_OPCODE_CONTROL: switch (event.a) {
          case MIDI_CONTROL_BANK_LSB: midi_controls[event.chid].bank=event.b; break;
          case MIDI_CONTROL_VOLUME_MSB: midi_controls[event.chid].trim=event.b; break;
          case MIDI_CONTROL_PAN_MSB: midi_controls[event.chid].pan=event.b; break;
        } break;
      case MIDI_OPCODE_NOTE_ON: midi_controls[event.chid].notec++; midi_controls[event.chid].notev[event.a]=1; break;
    }
  }
  
  if (chhdrv) {
    if (sr_encode_raw(ctx->dst,chhdrv,chhdrc)<0) return -1;
  } else {
    int chid=0,err;
    for (controls=midi_controls;chid<16;chid++,controls++) {
      // No header for a channel without any notes, even if it had some header events:
      if (!controls->notec) continue;
      // Likewise, if the trim was explicitly zero, no header. We'll still convert its events, but the synthesizer should ignore them:
      if (!controls->trim) continue;
      // Generic 3-byte introducer:
      if (sr_encode_u8(ctx->dst,chid)<0) return -1;
      if (sr_encode_u8(ctx->dst,(controls->trim<<1)|(controls->trim&1))<0) return -1;
      if (sr_encode_u8(ctx->dst,(controls->pan<<1)|(controls->pan&1))<0) return -1;
      // Then mode, payload, and post come from the SDK's instrument store:
      if ((err=eggdev_eau_default_channel_header(ctx,chid,controls->bank,controls->pid,controls->notev))<0) return -1;
    }
  }
  
  if (sr_encode_u8(ctx->dst,0xff)<0) return -1; // Channel Headers Terminator.
  return 0;
}

/* EAU from MIDI, events.
 */
 
static int eggdev_eau_from_mid_events(struct eggdev_convert_context *ctx,struct midi_file *midi) {

  #define HOLD_LIMIT 64
  struct midi_hold {
    uint8_t chid,noteid;
    int dstp;
    int starttime;
  } holdv[HOLD_LIMIT];
  int holdc=0;
  
  // EAU's wheel resolution is lower than MIDI's, so it's normal to get ignorable redundant wheel events.
  uint8_t wheel_by_chid[16];
  memset(wheel_by_chid,0x80,16);

  // Call SYNC_TIME to emit delay events to bring (outtime) up to (now).
  int now=0; // Current readhead time.
  int outtime=0; // Current writehead time.
  #define SYNC_TIME { \
    int delay=now-outtime; \
    while (delay>=2048) { \
      if (sr_encode_u8(ctx->dst,0x8f)<0) return -1; \
      delay-=2048; \
    } \
    if (delay>=128) { \
      if (sr_encode_u8(ctx->dst,0x80|((delay>>7)-1))<0) return -1; \
      delay&=0x7f; \
    } \
    if (delay>0) { \
      if (sr_encode_u8(ctx->dst,delay)<0) return -1; \
    } \
    outtime=now; \
  }
  
  for (;;) {
    struct midi_event event;
    int err=midi_file_next(&event,midi);
    if (err<0) {
      if (midi_file_is_finished(midi)) break;
      return eggdev_convert_error(ctx,"Malformed MIDI events.");
    }
    if (err) {
      now+=err;
      if (midi_file_advance(midi,err)<0) return eggdev_convert_error(ctx,"Malformed MIDI events.");
      continue;
    }
    switch (event.opcode) {
    
      case MIDI_OPCODE_NOTE_ON: {
          SYNC_TIME
          if (holdc>=HOLD_LIMIT) {
            if (ctx->refname&&!ctx->errmsg) fprintf(stderr,
              "%s:WARNING: Emitting note 0x%02x on channel %d at %d ms with no duration due to too many held notes.\n",
              ctx->refname,event.a,event.chid,now
            );
          } else {
            struct midi_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.a;
            hold->dstp=ctx->dst->c;
            hold->starttime=now;
          }
          if (sr_encode_u8(ctx->dst,0x90|event.chid)<0) return -1;
          if (sr_encode_u8(ctx->dst,(event.a<<1)|(event.b>>6))<0) return -1;
          if (sr_encode_u8(ctx->dst,(event.b<<2)&0xe0)<0) return -1;
        } break;
        
      case MIDI_OPCODE_NOTE_OFF: {
          int holdp=holdc;
          while (holdp-->0) {
            struct midi_hold *hold=holdv+holdp;
            if (hold->chid!=event.chid) continue;
            if (hold->noteid!=event.a) continue;
            if ((hold->dstp<0)||(hold->dstp>ctx->dst->c-3)) return -1;
            int durms=now-hold->starttime;
            if (durms<32) {
              ((uint8_t*)ctx->dst->v)[hold->dstp+2]|=durms;
            } else if (durms<=1024) {
              ((uint8_t*)ctx->dst->v)[hold->dstp]=0xa0|hold->chid;
              ((uint8_t*)ctx->dst->v)[hold->dstp+2]|=(durms>>5)-1;
            } else {
              if (durms>32768) durms=32768;
              ((uint8_t*)ctx->dst->v)[hold->dstp]=0xb0|hold->chid;
              ((uint8_t*)ctx->dst->v)[hold->dstp+2]|=(durms>>10)-1;
            }
            holdc--;
            memmove(hold,hold+1,sizeof(struct midi_hold)*(holdc-holdp));
            break;
          }
        } break;
        
      case MIDI_OPCODE_WHEEL: {
          int v=event.a|(event.b<<7);
          v>>=6;
          if (v==wheel_by_chid[event.chid]) break;
          wheel_by_chid[event.chid]=v;
          SYNC_TIME
          if (sr_encode_u8(ctx->dst,0xc0|event.chid)<0) return -1;
          if (sr_encode_u8(ctx->dst,v)<0) return -1;
        } break;
        
      //TODO Some kind of marker event for the loop position.
    }
  }
  SYNC_TIME
  #undef SYNC_TIME
  #undef HOLD_LIMIT
  if (holdc&&ctx->refname&&!ctx->errmsg) fprintf(stderr,
    "%s:WARNING: Emitting %d notes with duration zero due to Note Offs not present.\n",
    ctx->refname,holdc
  );
  return 0;
}

/* EAU from MIDI, top level.
 */
 
int eggdev_eau_from_mid(struct eggdev_convert_context *ctx) {
  int err;
  struct midi_file *midi=midi_file_new(ctx->src,ctx->srcc,1000);
  if (!midi) return eggdev_convert_error(ctx,"Failed to decode MIDI file.");
  if ((err=eggdev_eau_from_mid_header(ctx,midi))<0) {
    midi_file_del(midi);
    return err;
  }
  midi_file_reset(midi);
  if ((err=eggdev_eau_from_mid_events(ctx,midi))<0) {
    midi_file_del(midi);
    return err;
  }
  midi_file_del(midi);
  return 0;
}

/* MIDI from EAU.
 */
 
int eggdev_mid_from_eau(struct eggdev_convert_context *ctx) {
  struct eau_file eau;
  if (eau_file_decode(&eau,ctx->src,ctx->srcc)<0) {
    return eggdev_convert_error(ctx,"Failed to decode EAU.");
  }
  
  // MThd, and begin the MTrk.
  if (sr_encode_raw(ctx->dst,"MThd\0\0\0\6",8)<0) return -1;
  if (sr_encode_raw(ctx->dst,"\0\1\0\1",4)<0) return -1; // Format 1, and single track.
  if (sr_encode_intbe(ctx->dst,eau.tempo,2)<0) return -1; // 1 tick = 1 ms.
  if (sr_encode_raw(ctx->dst,"MTrk",4)<0) return -1;
  int lenp=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0\0\0\0",4)<0) return -1;
  
  // Meta 0x51 Set Tempo. us/qnote in 3 bytes.
  if (sr_encode_raw(ctx->dst,"\0\xff\x51\3",4)<0) return -1;
  if (sr_encode_intbe(ctx->dst,eau.tempo*1000,3)<0) return -1;
  
  // Meta 0x77 if we have channel headers, which we really should.
  if (eau.chhdrc>0) {
    if (sr_encode_raw(ctx->dst,"\0\xff\x77",3)<0) return -1;
    if (sr_encode_vlq(ctx->dst,eau.chhdrc)<0) return -1;
    if (sr_encode_raw(ctx->dst,eau.chhdrv,eau.chhdrc)<0) return -1;
  }
  
  // Add milliseconds to (delay) arbitrarily, then call FLUSH_DELAY to emit Note Offs if needed, and the leading delay of an event.
  int delay=0;
  int now=0;
  #define HOLD_LIMIT 64
  struct eau_hold {
    uint8_t chid,noteid;
    int offtime;
  } holdv[HOLD_LIMIT];
  int holdc=0;
  #define FLUSH_DELAY { \
    for (;;) { \
      int nexttime=now+delay; \
      int holdp=-1; \
      int i=holdc; while (i-->0) { \
        if (holdv[i].offtime>nexttime) continue; \
        if ((holdp>=0)&&(holdv[i].offtime>=holdv[holdp].offtime)) continue; \
        holdp=i; \
      } \
      if (holdp>=1) { /* Releasing a hold. */ \
        int subdelay=holdv[holdp].offtime-now; \
        if (subdelay<0) subdelay=0; \
        if (sr_encode_vlq(ctx->dst,subdelay)<0) return -1; \
        if (sr_encode_u8(ctx->dst,0x80|holdv[holdp].chid)<0) return -1; \
        if (sr_encode_u8(ctx->dst,holdv[holdp].noteid)<0) return -1; \
        if (sr_encode_u8(ctx->dst,0x40)<0) return -1; \
        holdc--; \
        memmove(holdv+holdp,holdv+holdp+1,sizeof(struct eau_hold)*(holdc-holdp)); \
        now+=subdelay; \
        delay-=subdelay; \
        continue; \
      } \
      if (sr_encode_vlq(ctx->dst,delay)<0) return -1; \
      now+=delay; \
      delay=0; \
      break; \
    } \
  }
  
  // Emit events in order.
  //TODO Insert some MIDI event for the loop position if nonzero.
  int evtp=0;
  while (evtp<eau.evtc) {
    struct eau_event event;
    int err=eau_event_decode(&event,eau.evtv+evtp,eau.evtc-evtp);
    if (err<0) return eggdev_convert_error(ctx,"Malformed EAU events.");
    if (!err) break;
    evtp+=err;
    switch (event.type) {
    
      case 'd': {
          delay+=event.delay;
        } break;
        
      case 'n': {
          FLUSH_DELAY
          if (sr_encode_u8(ctx->dst,0x90|event.chid)<0) return -1;
          if (sr_encode_u8(ctx->dst,event.noteid)<0) return -1;
          if (sr_encode_u8(ctx->dst,(event.velocity<<3)|(event.velocity>>1))<0) return -1;
          if (!event.delay) {
            // No duration, we can emit the note off right away.
            // Also might as well use Running Status here, even though in general we don't.
            if (sr_encode_u8(ctx->dst,0)<0) return -1;
            if (sr_encode_u8(ctx->dst,event.noteid)<0) return -1;
            if (sr_encode_u8(ctx->dst,0)<0) return -1;
          } else if (holdc>=HOLD_LIMIT) {
            if (ctx->refname&&!ctx->errmsg) fprintf(stderr,
              "%s:WARNING: Dropping duration %d for note 0x%02x on channel %d at %d ms, too many notes held.\n",
              ctx->refname,event.delay,event.noteid,event.chid,now
            );
            if (sr_encode_u8(ctx->dst,0)<0) return -1;
            if (sr_encode_u8(ctx->dst,event.noteid)<0) return -1;
            if (sr_encode_u8(ctx->dst,0)<0) return -1;
          } else {
            struct eau_hold *hold=holdv+holdc++;
            hold->chid=event.chid;
            hold->noteid=event.noteid;
            hold->offtime=now+event.delay;
          }
        } break;
        
      case 'w': {
          FLUSH_DELAY
          if (sr_encode_u8(ctx->dst,0xe0|event.chid)<0) return -1;
          int v=(event.velocity<<6)||(event.velocity&0x3f); // 14 bits from 8, and ensure that (0x00,0x80,0xff) all expand sensibly.
          if (sr_encode_u8(ctx->dst,v&0x7f)<0) return -1;
          if (sr_encode_u8(ctx->dst,(v>>7)&0x7f)<0) return -1;
        } break;
        
      default: return eggdev_convert_error(ctx,"Unexpected EAU event type '%c'",event.type);
    }
  }
  
  // Finally, flush any remaining delay, and apply that to a Meta 0x2f EOT.
  // Oh also, if any holds remain after the flush, emit a Note Off for each.
  FLUSH_DELAY
  while (holdc>0) {
    const struct eau_hold *hold=holdv+(--(holdc));
    // A delay is already emitted, so we emit this Note Off a bit out of phase:
    if (sr_encode_u8(ctx->dst,0x80|hold->chid)<0) return -1;
    if (sr_encode_u8(ctx->dst,hold->noteid)<0) return -1;
    if (sr_encode_u8(ctx->dst,0x40)<0) return -1;
    if (sr_encode_u8(ctx->dst,0)<0) return -1; // next delay
  }
  if (sr_encode_raw(ctx->dst,"\xff\x2f\0",3)<0) return -1;
  #undef FLUSH_DELAY
  #undef HOLD_LIMIT
  
  // Fill in MTrk length.
  int len=ctx->dst->c-lenp-4;
  if (len<0) return -1;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>24;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>16;
  ((uint8_t*)ctx->dst->v)[lenp++]=len>>8;
  ((uint8_t*)ctx->dst->v)[lenp++]=len;
  
  return 0;
}
