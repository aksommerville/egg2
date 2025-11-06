/* synth_channel_type_trivial.c
 * The dumbest voicing mode I can imagine, mostly using for validation purposes during development.
 * Voices are square wave only, with no envelope except a brief release stage.
 */
 
#include "synth_internal.h"

// We'll artificially refuse to create more than so many voices.
#define TRIVIAL_VOICE_LIMIT 32

struct synth_voice_trivial {
  uint8_t noteid; // 0xff if unaddressable. Not necessarily unique.
  uint32_t p;
  uint32_t dp;
  uint32_t predp; // (dp) before accounting for wheel.
  int ttl; // Defunct if zero. Initially at least (holdtime+releasetime).
  int ttl0; // TTL at start, so we can validate holdtime on manual releases.
  float level;
  float dlevel; // Per frame during release; always negative.
};

struct synth_channel_trivial {
  struct synth_channel hdr;
  struct synth_voice_trivial *voicev;
  int voicec,voicea;
  int wheelrange; // cents
  float minlevel,maxlevel;
  int holdtime,releasetime; // frames, >0
  float wheelbend; // Multiplier, final answer for wheel, ready to apply.
  int longdurframes; // TTL of a note with unspecified release time, not including our global releasetime.
};

#define CHANNEL ((struct synth_channel_trivial*)channel)

/* Cleanup.
 */
 
static void _trivial_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) synth_free(CHANNEL->voicev);
}

/* Update.
 */
 
static void synth_voice_trivial_update(float *dst,struct synth_voice_trivial *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
  
    // TTL and fadeout.
    if (voice->ttl<=0) break;
    voice->ttl--;
    if (voice->ttl<CHANNEL->releasetime) {
      if ((voice->level+=voice->dlevel)<=0.0f) {
        voice->ttl=0;
        break;
      }
    }
    
    voice->p+=voice->dp;
    if (voice->p&0x80000000) {
      (*dst)+=voice->level;
    } else {
      (*dst)-=voice->level;
    }
  }
}
 
static void _trivial_update_mono(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_trivial *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_trivial_update(dst,voice,channel,framec);
  while (CHANNEL->voicec&&(CHANNEL->voicev[CHANNEL->voicec-1].ttl<=0)) CHANNEL->voicec--;
}

/* Initialize.
 */
 
static int _trivial_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  
  // Set defaults. (holdtime,releasetime) in ms initially.
  CHANNEL->wheelrange=200;
  int minleveli=0x2000;
  int maxleveli=0xffff;
  CHANNEL->holdtime=100;
  CHANNEL->releasetime=100;
  
  // Read modecfg. Our modecfg is pretty simple, it's all u16.
  int srcp=0;
  #define RD16(dst) if (srcp<srcc) { \
    if (srcp>srcc-2) return -1; \
    dst=(src[srcp]<<8)|src[srcp+1]; \
    srcp+=2; \
  }
  RD16(CHANNEL->wheelrange)
  RD16(minleveli)
  RD16(maxleveli)
  RD16(CHANNEL->holdtime)
  RD16(CHANNEL->releasetime)
  #undef RD16
  
  CHANNEL->minlevel=(float)minleveli/65535.0f;
  CHANNEL->maxlevel=(float)maxleveli/65535.0f;
  if ((CHANNEL->holdtime=synth_frames_from_ms(CHANNEL->holdtime))<1) CHANNEL->holdtime=1;
  if ((CHANNEL->releasetime=synth_frames_from_ms(CHANNEL->releasetime))<1) CHANNEL->releasetime=1;
  
  CHANNEL->wheelbend=1.0f;
  CHANNEL->longdurframes=SYNTH_DEFAULT_HOLD_TIME_S*synth.rate;
  
  channel->update_mono=_trivial_update_mono;
  
  return 0;
}

/* Release all.
 */
 
static void _trivial_release_all(struct synth_channel *channel) {
  struct synth_voice_trivial *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    voice->noteid=0xff; // Everything becomes unaddressable.
    if (voice->ttl<CHANNEL->releasetime) continue; // Defunct or already releasing.
    int elapsed=voice->ttl0-voice->ttl;
    if (elapsed<CHANNEL->holdtime) { // Must include the remainder of holdtime too.
      voice->ttl=CHANNEL->releasetime+(CHANNEL->holdtime-elapsed);
    } else { // Go directly to release, do not pass Go, do not collect $200.
      voice->ttl=CHANNEL->releasetime;
    }
  }
}

/* Wheel changed.
 */
 
static void _trivial_wheel_changed(struct synth_channel *channel) {
  if (!CHANNEL->wheelrange) return;
  int cents=(int)(CHANNEL->wheelrange*channel->wheelf);
  CHANNEL->wheelbend=synth_bend_from_cents(cents);
  struct synth_voice_trivial *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid&0x80) continue; // Debatable: Wheel only applies to addressable voices.
    voice->dp=(int32_t)((float)voice->predp*CHANNEL->wheelbend);
  }
}

/* Begin note.
 */
 
static void _trivial_note_once(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  if (noteid&0x80) return;

  // Clamp to (holdtime), then add (releasetime).
  if (durframes<CHANNEL->holdtime) durframes=CHANNEL->holdtime;
  durframes+=CHANNEL->releasetime;
  if (durframes<1) return;

  /* Find an available voice.
   * Do not evict living voices.
   * If we don't have a free one, and can't allocate more, discard the event.
   */
  struct synth_voice_trivial *voice=0;
  if (CHANNEL->voicec<CHANNEL->voicea) {
    voice=CHANNEL->voicev+CHANNEL->voicec++;
  } else {
    struct synth_voice_trivial *q=CHANNEL->voicev;
    int i=CHANNEL->voicec;
    for (;i-->0;q++) {
      if (q->ttl>0) continue;
      voice=q;
      break;
    }
    if (!voice) {
      if (CHANNEL->voicea>=TRIVIAL_VOICE_LIMIT) return; // Too many voices.
      int na=CHANNEL->voicea+8;
      void *nv=synth_realloc(CHANNEL->voicev,sizeof(struct synth_voice_trivial)*na);
      if (!nv) return;
      CHANNEL->voicev=nv;
      CHANNEL->voicea=na;
      voice=CHANNEL->voicev+CHANNEL->voicec++;
    }
  }
  
  // Initialize voice.
  voice->p=0;
  voice->noteid=noteid;
  voice->predp=synth.iratev[noteid];
  voice->dp=(int32_t)((float)voice->predp*CHANNEL->wheelbend);
  voice->ttl=voice->ttl0=durframes;
  voice->level=CHANNEL->minlevel*(1.0f-velocity)+CHANNEL->maxlevel*velocity;
  voice->dlevel=-voice->level/(float)CHANNEL->releasetime;
}

static void _trivial_note_on(struct synth_channel *channel,uint8_t noteid,float velocity) {
  _trivial_note_once(channel,noteid,velocity,CHANNEL->longdurframes);
}

/* Release note.
 */
 
static void _trivial_note_off(struct synth_channel *channel,uint8_t noteid) {
  struct synth_voice_trivial *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid!=noteid) continue;
    voice->noteid=0xff;
    if (voice->ttl<=CHANNEL->releasetime) continue;
    int elapsed=voice->ttl0-voice->ttl;
    if (elapsed<CHANNEL->holdtime) voice->ttl=CHANNEL->releasetime+(CHANNEL->holdtime-elapsed);
    else voice->ttl=CHANNEL->releasetime;
  }
}

/* Type definition.
 */
 
const struct synth_channel_type synth_channel_type_trivial={
  .name="trivial",
  .objlen=sizeof(struct synth_channel_trivial),
  .del=_trivial_del,
  .init=_trivial_init,
  .release_all=_trivial_release_all,
  .wheel_changed=_trivial_wheel_changed,
  .note_once=_trivial_note_once,
  .note_on=_trivial_note_on,
  .note_off=_trivial_note_off,
};
