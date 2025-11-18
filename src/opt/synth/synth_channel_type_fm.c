/* synth_channel_type_fm.c
 * This is a kitchen-sink channel mode that should cover the vast majority of tuned voices.
 * We call it "fm", but FM is just one of several features, and it collapses neatly into a simple wave+envelope.
 */
 
#include "synth_internal.h"

// We'll artificially refuse to create more than so many voices.
#define FM_VOICE_LIMIT 32

struct synth_voice_fm {
  uint8_t noteid;
  uint32_t carp;
  uint32_t cardp;
  uint32_t carpredp;
  struct synth_env levelenv;
  struct synth_env mixenv;
  struct synth_env rangeenv;
  struct synth_env pitchenv;
  uint32_t modp;
};

struct synth_channel_fm {
  struct synth_channel hdr;
  struct synth_voice_fm *voicev;
  int voicec,voicea;
  
  // Per modecfg:
  struct synth_env levelenv;
  int wheelrange;
  struct synth_wave *wavea;
  struct synth_wave *waveb;
  struct synth_env mixenv;
  uint32_t modabs; // absolute modulator step, also serves as the flag
  float modrate;
  float modrange;
  struct synth_env rangeenv;
  struct synth_env pitchenv;
  struct synth_wave *modulator;
  float rangelforate;
  float rangelfodepth;
  struct synth_wave *rangelfowave;
  float mixlforate;
  float mixlfodepth;
  struct synth_wave *mixlfowave;
  
  float wheelbend;
  int longdurframes;
  float *rangelfo;
  uint32_t rangelfop,rangelfodp;
  float *mixlfo;
  uint32_t mixlfop,mixlfodp;
};

#define CHANNEL ((struct synth_channel_fm*)channel)

/* Cleanup.
 */
 
static void _fm_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) synth_free(CHANNEL->voicev);
  synth_wave_del(CHANNEL->wavea);
  synth_wave_del(CHANNEL->waveb);
  synth_wave_del(CHANNEL->modulator);
  synth_wave_del(CHANNEL->rangelfowave);
  synth_wave_del(CHANNEL->mixlfowave);
  if (CHANNEL->rangelfo) synth_free(CHANNEL->rangelfo);
  if (CHANNEL->mixlfo) synth_free(CHANNEL->mixlfo);
}

/* "waveaonly": No FM, pitch bend, or LFOs, and we only use wave A. This is as simple as we get.
 */
 
static void synth_voice_fm_update_waveaonly(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
    voice->carp+=voice->cardp;
    float sample=CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT];
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_waveaonly(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_waveaonly(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* "waveonly": Wave mixer. No FM, pitch bend, or LFOs.
 */
 
static void synth_voice_fm_update_waveonly(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
    voice->carp+=voice->cardp;
    float mix=synth_env_update(voice->mixenv);
    float sample=
      CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT]*(1.0f-mix)+
      CHANNEL->waveb->v[voice->carp>>SYNTH_WAVE_SHIFT]*mix;
      
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_waveonly(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_waveonly(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* "fmonly": Relative modulation. No pitchenv, LFOs, or wave mixer.
 */
 
static void synth_voice_fm_update_fmonly(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
  
    float fdp=(float)voice->cardp;
    
    voice->modp+=(int32_t)(fdp*CHANNEL->modrate);
    
    float mod=CHANNEL->modulator->v[voice->modp>>SYNTH_WAVE_SHIFT];
    float range=synth_env_update(voice->rangeenv);
    mod*=range;
    int32_t dp=voice->cardp+(fdp*mod);
    voice->carp+=dp;
    
    float sample=CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT];
      
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_fmonly(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_fmonly(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* "nobend": FM+mix but no pitchenv.
 * Note that bending with the pitch wheel is always permitted; that's not part of update.
 */
 
static void synth_voice_fm_update_nobend(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
  
    float fdp=(float)voice->cardp;
    
    voice->modp+=(int32_t)(fdp*CHANNEL->modrate);
    
    float mod=CHANNEL->modulator->v[voice->modp>>SYNTH_WAVE_SHIFT];
    float range=synth_env_update(voice->rangeenv);
    mod*=range;
    int32_t dp=voice->cardp+(fdp*mod);
    voice->carp+=dp;
    
    float mix=synth_env_update(voice->mixenv);
    float sample=
      CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT]*(1.0f-mix)+
      CHANNEL->waveb->v[voice->carp>>SYNTH_WAVE_SHIFT]*mix;
      
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_nobend(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_nobend(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* "nolfo": All options except LFOs and absolute modulators.
 */
 
static void synth_voice_fm_update_nolfo(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  for (;framec-->0;dst++) {
  
    float fdp=(float)voice->cardp;
    int cents=(int)synth_env_update(voice->pitchenv);
    fdp*=synth_bend_from_cents(cents);
    
    voice->modp+=(int32_t)(fdp*CHANNEL->modrate);
    
    float mod=CHANNEL->modulator->v[voice->modp>>SYNTH_WAVE_SHIFT];
    float range=synth_env_update(voice->rangeenv);
    mod*=range;
    int32_t dp=(int32_t)(fdp+(fdp*mod));
    voice->carp+=dp;
    
    float mix=synth_env_update(voice->mixenv);
    float sample=
      CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT]*(1.0f-mix)+
      CHANNEL->waveb->v[voice->carp>>SYNTH_WAVE_SHIFT]*mix;
      
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_nolfo(float *dst,struct synth_channel *channel,int framec) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_nolfo(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* "full": All options enabled, and modulator may be absolute or relative.
 */
 
static void synth_voice_fm_update_full(float *dst,struct synth_voice_fm *voice,struct synth_channel *channel,int framec) {
  const float *rangelfo=CHANNEL->rangelfo;
  const float *mixlfo=CHANNEL->mixlfo;
  for (;framec-->0;dst++) {
  
    float fdp=(float)voice->cardp;
    int cents=(int)synth_env_update(voice->pitchenv);
    fdp*=synth_bend_from_cents(cents);
    
    if (CHANNEL->modabs) voice->modp+=CHANNEL->modabs;
    else voice->modp+=(int32_t)(fdp*CHANNEL->modrate);
    
    float mod=CHANNEL->modulator->v[voice->modp>>SYNTH_WAVE_SHIFT];
    float range=synth_env_update(voice->rangeenv);
    if (rangelfo) {
      range*=*rangelfo;
      rangelfo++;
    }
    mod*=range;
    int32_t dp=voice->cardp+(fdp*mod);
    voice->carp+=dp;
    
    float mix=synth_env_update(voice->mixenv);
    if (mixlfo) {
      mix+=*mixlfo;
      if (mix<0.0f) mix=0.0f; else if (mix>1.0f) mix=1.0f;
      mixlfo++;
    }
    float sample=
      CHANNEL->wavea->v[voice->carp>>SYNTH_WAVE_SHIFT]*(1.0f-mix)+
      CHANNEL->waveb->v[voice->carp>>SYNTH_WAVE_SHIFT]*mix;
      
    float level=synth_env_update(voice->levelenv);
    (*dst)+=sample*level;
  }
}
 
static void _fm_update_mono_full(float *dst,struct synth_channel *channel,int framec) {
  if (CHANNEL->rangelfo) { // rangelfo is a multiplier, scale from -1..1 to 0..1 and bias to the top
    const float *lfosrc=CHANNEL->rangelfowave->v;
    float *lfodst=CHANNEL->rangelfo;
    int i=framec;
    float mlt=0.5f*CHANNEL->rangelfodepth;
    float add=1.0f-CHANNEL->rangelfodepth;
    for (;i-->0;lfodst++) {
      CHANNEL->rangelfop+=CHANNEL->rangelfodp;
      *lfodst=(lfosrc[CHANNEL->rangelfop>>SYNTH_WAVE_SHIFT]+1.0f)*mlt+add;
    }
  }
  if (CHANNEL->mixlfo) { // mixlfo is additive; keep it in -1..1
    const float *lfosrc=CHANNEL->mixlfowave->v;
    float *lfodst=CHANNEL->mixlfo;
    int i=framec;
    for (;i-->0;lfodst++) {
      CHANNEL->mixlfop+=CHANNEL->mixlfodp;
      *lfodst=lfosrc[CHANNEL->mixlfop>>SYNTH_WAVE_SHIFT]*CHANNEL->mixlfodepth;
    }
  }
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update_full(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* Decode modecfg.
 */
 
static int synth_channel_fm_decode(struct synth_channel *channel,const uint8_t *src,int srcc) {
  int srcp=0,err;
  // (synth.sine) is immortal, it's safe to refer to it directly.
  #define RDWAVE(fldname) { \
    if (srcp>=srcc) CHANNEL->fldname=&synth.sine; \
    else if (!src[srcp]) { srcp++; CHANNEL->fldname=&synth.sine; } \
    else { \
      if (!(CHANNEL->fldname=synth_wave_new())) return -1; \
      if ((err=synth_wave_decode(CHANNEL->fldname,src+srcp,srcc-srcp))<0) return -1; \
      srcp+=err; \
    } \
  }
  if ((err=synth_env_decode(&CHANNEL->levelenv,src+srcp,srcc-srcp,SYNTH_ENV_FALLBACK_LEVEL))<0) return -1; srcp+=err;
  if (srcp<=srcc-2) { CHANNEL->wheelrange=(src[srcp]<<8)|src[srcp+1]; srcp+=2; } else CHANNEL->wheelrange=200;
  RDWAVE(wavea)
  RDWAVE(waveb)
  if ((err=synth_env_decode(&CHANNEL->mixenv,src+srcp,srcc-srcp,SYNTH_ENV_FALLBACK_ZERO))<0) return -1; srcp+=err;
  if (srcp<=srcc-2) {
    int pre=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (pre&0x8000) CHANNEL->modabs=1; else CHANNEL->modabs=0;
    CHANNEL->modrate=(float)(pre&0x7fff)/256.0f;
  }
  if (srcp<=srcc-2) { CHANNEL->modrange=((src[srcp]<<8)|src[srcp+1])/256.0f; srcp+=2; }
  if ((err=synth_env_decode(&CHANNEL->rangeenv,src+srcp,srcc-srcp,SYNTH_ENV_FALLBACK_ONE))<0) return -1; srcp+=err;
  if ((err=synth_env_decode(&CHANNEL->pitchenv,src+srcp,srcc-srcp,SYNTH_ENV_FALLBACK_HALF))<0) return -1; srcp+=err;
  RDWAVE(modulator)
  if (srcp<=srcc-2) { CHANNEL->rangelforate=((src[srcp]<<8)|src[srcp+1])/256.0f; srcp+=2; }
  if (srcp<srcc) CHANNEL->rangelfodepth=src[srcp++]/255.0f; else CHANNEL->rangelfodepth=1.0f;
  RDWAVE(rangelfowave)
  if (srcp<=srcc-2) { CHANNEL->mixlforate=((src[srcp]<<8)|src[srcp+1])/256.0f; srcp+=2; }
  if (srcp<srcc) CHANNEL->mixlfodepth=src[srcp++]/255.0f; else CHANNEL->mixlfodepth=1.0f;
  RDWAVE(mixlfowave)
  #undef RDWAVE
  return 0;
}

/* Initialize.
 */
 
static int _fm_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  
  if (synth_channel_fm_decode(channel,src,srcc)<0) return -1;
  
  // Don't use (CHANNEL->modrange) once running; just bake it into (CHANNEL->rangeenv).
  synth_env_mlt(&CHANNEL->rangeenv,CHANNEL->modrange);
  
  // Scale (CHANNEL->pitchenv) to express itself in cents.
  synth_env_add(&CHANNEL->pitchenv,-0.5f);
  synth_env_mlt(&CHANNEL->pitchenv,32767.0f);
  
  // If the modulator's rate is absolute, precalculate as a step. And don't let it be zero.
  if (CHANNEL->modabs) {
    if (!(CHANNEL->modabs=synth_song_get_tempo_step(channel->song,CHANNEL->modrate))) CHANNEL->modabs=1;
  }
  
  // Prepare (rangelfo) and (mixlfo).
  if ((CHANNEL->rangelforate>0.0f)&&(CHANNEL->rangelfodepth>0.0f)) {
    if ((CHANNEL->rangelfodp=synth_song_get_tempo_step(channel->song,CHANNEL->rangelforate))) {
      if (!(CHANNEL->rangelfo=synth_malloc(sizeof(float)*synth.buffer_frames))) return -1;
    }
  }
  if ((CHANNEL->mixlforate>0.0f)&&(CHANNEL->mixlfodepth>0.0f)) {
    if ((CHANNEL->mixlfodp=synth_song_get_tempo_step(channel->song,CHANNEL->mixlforate))) {
      if (!(CHANNEL->mixlfo=synth_malloc(sizeof(float)*synth.buffer_frames))) return -1;
    }
  }
  
  CHANNEL->wheelbend=1.0f;
  CHANNEL->longdurframes=SYNTH_DEFAULT_HOLD_TIME_S*synth.rate;
  
  /* Select the lightest update regime that performs all the operations we're configured for.
   */
  // Only "full" does LFOs and absolute modulator rates. Those are all kind of unusual, so we don't bother splitting them out with more specific optimization.
  if (CHANNEL->rangelfo||CHANNEL->mixlfo||CHANNEL->modabs) channel->update_mono=_fm_update_mono_full;
  // If we have a pitch envelope, we need "nolfo".
  else if (CHANNEL->pitchenv.flags&SYNTH_ENV_PRESENT) channel->update_mono=_fm_update_mono_nolfo;
  // If the wave mixer is defaulted, we can use "fmonly" or "waveaonly".
  else if (!(CHANNEL->mixenv.flags&SYNTH_ENV_PRESENT)) {
    if ((CHANNEL->modrate>0.0f)&&(CHANNEL->modrange>0.0f)) channel->update_mono=_fm_update_mono_fmonly;
    else channel->update_mono=_fm_update_mono_waveaonly;
  }
  // If FM rate or range is zero, we can use "waveonly".
  else if ((CHANNEL->modrate<=0.0f)&&(CHANNEL->modrange<=0.0f)) channel->update_mono=_fm_update_mono_waveonly;
  // No LFO, absmod, or pitchenv: We can use "nobend".
  else channel->update_mono=_fm_update_mono_nobend;
  
  return 0;
}

/* Release all.
 */
 
static void _fm_release_all(struct synth_channel *channel) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    synth_env_release(&voice->levelenv);
    synth_env_release(&voice->mixenv);
    synth_env_release(&voice->rangeenv);
    synth_env_release(&voice->pitchenv);
    voice->noteid=0xff;
  }
}

/* Wheel changed.
 */
 
static void _fm_wheel_changed(struct synth_channel *channel) {
  int cents=(int)(CHANNEL->wheelrange*channel->wheelf);
  CHANNEL->wheelbend=synth_bend_from_cents(cents);
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid&0x80) continue; // Debatable: Wheel only applies to addressable voices.
    voice->cardp=(int32_t)((float)voice->carpredp*CHANNEL->wheelbend);
  }
}

/* Begin note.
 */
 
static void _fm_note_once(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  if (noteid&0x80) return;
  
  // Find a voice. If we're at the limit and nothing is available, discard the event.
  struct synth_voice_fm *voice=0;
  if (CHANNEL->voicec<CHANNEL->voicea) {
    voice=CHANNEL->voicev+CHANNEL->voicec++;
  } else {
    struct synth_voice_fm *q=CHANNEL->voicev;
    int i=CHANNEL->voicec;
    for (;i-->0;q++) {
      if (q->levelenv.finished) {
        voice=q;
        break;
      }
    }
    if (!voice) {
      if (CHANNEL->voicea>=FM_VOICE_LIMIT) return;
      int na=CHANNEL->voicea+8;
      void *nv=synth_realloc(CHANNEL->voicev,sizeof(struct synth_voice_fm)*na);
      if (!nv) return;
      CHANNEL->voicev=nv;
      CHANNEL->voicea=na;
      voice=CHANNEL->voicev+CHANNEL->voicec++;
    }
  }
  
  voice->noteid=noteid;
  voice->carp=0;
  voice->carpredp=synth.iratev[noteid];
  voice->cardp=(int32_t)((float)voice->carpredp*CHANNEL->wheelbend);
  synth_env_apply(&voice->levelenv,&CHANNEL->levelenv,velocity,durframes);
  synth_env_apply(&voice->mixenv,&CHANNEL->mixenv,velocity,durframes);
  synth_env_apply(&voice->rangeenv,&CHANNEL->rangeenv,velocity,durframes);
  synth_env_apply(&voice->pitchenv,&CHANNEL->pitchenv,velocity,durframes);
  voice->modp=0;
}

static void _fm_note_on(struct synth_channel *channel,uint8_t noteid,float velocity) {
  _fm_note_once(channel,noteid,velocity,CHANNEL->longdurframes);
}

/* Release note.
 */
 
static void _fm_note_off(struct synth_channel *channel,uint8_t noteid) {
  struct synth_voice_fm *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid!=noteid) continue;
    synth_env_release(&voice->levelenv);
    synth_env_release(&voice->mixenv);
    synth_env_release(&voice->rangeenv);
    synth_env_release(&voice->pitchenv);
    voice->noteid=0xff;
  }
}

/* Type definition.
 */
 
const struct synth_channel_type synth_channel_type_fm={
  .name="fm",
  .objlen=sizeof(struct synth_channel_fm),
  .del=_fm_del,
  .init=_fm_init,
  .release_all=_fm_release_all,
  .wheel_changed=_fm_wheel_changed,
  .note_once=_fm_note_once,
  .note_on=_fm_note_on,
  .note_off=_fm_note_off,
};
