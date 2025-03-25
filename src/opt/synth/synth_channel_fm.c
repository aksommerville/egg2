#include "synth_internal.h"

#define CHANNEL ((struct synth_channel_fm*)channel)

// Arbitrary limit. Ensure it's low enough to avoid overflow.
// But also, keep it much lower than that, in case a misbehavey song tries to exhaust the system's memory or something.
// We don't evict voices when exhausted, we just reject new notes.
#define SYNTH_FM_VOICE_LIMIT 32

/* Cleanup.
 */
 
static void synth_channel_fm_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) free(CHANNEL->voicev);
  synth_wave_del(CHANNEL->sine);
  synth_wave_del(CHANNEL->carrier);
  synth_wave_del(CHANNEL->lfo);
  if (CHANNEL->lfobuf) free(CHANNEL->lfobuf);
}

/* Update.
 */

// Cheapest option: Just level and wave. No detune or FM.
static void synth_voice_fm_update_waveenv(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  for (;c-->0;v++) {
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    voice->p+=voice->dp;
    float level=synth_env_update(&voice->level);
    sample*=level;
    (*v)+=sample;
  }
}

// No FM or LFO, but do allow sample-cadence pitch adjustment.
static void synth_voice_fm_update_pitchenv(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  float fdp=(float)voice->dp;
  for (;c-->0;v++) {
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    float adjust=synth_env_update(&voice->pitchenv);
    uint32_t dp=(uint32_t)(fdp*powf(2.0f,adjust));
    voice->p+=dp;
    float level=synth_env_update(&voice->level);
    sample*=level;
    (*v)+=sample;
  }
}

// Fixed-rate FM with no pitch envelope.
static void synth_voice_fm_update_basicfm(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  float fdp=(float)voice->dp;
  for (;c-->0;v++) {
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    float level=synth_env_update(&voice->level);
    sample*=level;
    (*v)+=sample;
    float mod=CHANNEL->sine->v[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=voice->moddp;
    float dp=fdp+fdp*mod*CHANNEL->modrange;
    voice->p+=(uint32_t)dp;
  }
}

// FM with both envelopes but no LFO.
static void synth_voice_fm_update_fullenv(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  float fdp=(float)voice->dp;
  for (;c-->0;v++) {
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    float level=synth_env_update(&voice->level);
    sample*=level;
    (*v)+=sample;
    float adjust=synth_env_update(&voice->pitchenv);
    adjust=powf(2.0f,adjust);
    float mod=CHANNEL->sine->v[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=voice->moddp;//TODO adjust this too?
    mod*=synth_env_update(&voice->rangeenv);
    float dp=fdp+fdp*mod;
    dp*=adjust;
    voice->p+=(uint32_t)dp;
  }
}

// All features enabled. In particular, this is the only mode that uses the shared LFO.
static void synth_voice_fm_update_bells_whistles(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  float fdp=(float)voice->dp;
  const float *lfo=CHANNEL->lfobuf;
  for (;c-->0;v++,lfo++) {
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    float level=synth_env_update(&voice->level);
    sample*=level;
    (*v)+=sample;
    float adjust=synth_env_update(&voice->pitchenv);
    adjust=powf(2.0f,adjust);
    float mod=CHANNEL->sine->v[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=voice->moddp;//TODO adjust this too?
    mod*=synth_env_update(&voice->rangeenv);
    mod*=*lfo;
    float dp=fdp+fdp*mod;
    dp*=adjust;
    voice->p+=(uint32_t)dp;
  }
}
 
static void synth_channel_fm_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c);
  if (!CHANNEL->voice_update) {
    if (channel->ttl<1) channel->ttl=1;
    return;
  }
  if (!CHANNEL->voicec) {
    if (CHANNEL->use_lfo) CHANNEL->lfop+=CHANNEL->lfodp*c;
    return;
  }
  if (CHANNEL->use_lfo) {
    float *dst=CHANNEL->lfobuf;
    int i=c;
    for (;i-->0;dst++) {
      *dst=CHANNEL->lfo->v[CHANNEL->lfop>>SYNTH_WAVE_SHIFT];
      CHANNEL->lfop+=CHANNEL->lfodp;
    }
  }
  struct synth_fm_voice *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) CHANNEL->voice_update(v,c,voice,channel);
  while (CHANNEL->voicec&&synth_env_is_complete(&CHANNEL->voicev[CHANNEL->voicec-1].level)) CHANNEL->voicec--;
}

/* Finish decode.
 * Channel fields are populated straight off the serial config but not adjusted yet.
 * (update) should not be populated yet -- we do that, and if we decline to, the channel gets dropped.
 */
 
static int synth_channel_fm_ready(struct synth_channel *channel) {
  channel->chanc=1;
  channel->update=synth_channel_fm_update;

  /* FM is in play if both rate and range are nonzero.
   * In theory it still might noop, if the range envelope emits straight zeroes.
   */
  if ((CHANNEL->modrate>0.0f)&&(CHANNEL->modrange>0.0f)) {
    if (!(CHANNEL->sine=synth_wave_new(channel->synth,"\0\0\0",3))) return -1;
    
    /* LFO is only relevant when FM in play.
     * If we have an LFO, we're automatically bells-and-whistliest case.
     */
    if ((CHANNEL->lforate>0.0f)&&(CHANNEL->lfodepth>0.0f)&&(channel->synth->tempo_frames>0)) {
      if (!CHANNEL->pitchenv.pointc&&!CHANNEL->pitchenv.flags) {
        CHANNEL->pitchenv.initlo=CHANNEL->pitchenv.inithi=0.0f;
      } else {
        synth_env_bias(&CHANNEL->pitchenv,-0.5f);
        synth_env_scale(&CHANNEL->pitchenv,65535.0f/1200.0f);
      }
      CHANNEL->use_pitchenv=1;
      CHANNEL->use_moddp=1;
      if (!CHANNEL->rangeenv.pointc&&!CHANNEL->rangeenv.flags) {
        CHANNEL->rangeenv.initlo=CHANNEL->rangeenv.inithi=1.0f;
      } else {
        synth_env_scale(&CHANNEL->rangeenv,CHANNEL->modrange);
      }
      CHANNEL->use_rangeenv=1;
      if (!(CHANNEL->lfo=synth_wave_new(0,"\0\0\0",3))) return -1; // sine wave, but a fresh one
      float *v=CHANNEL->lfo->v;
      int i=SYNTH_WAVE_SIZE_SAMPLES;
      for (;i-->0;v++) *v=(*v)*CHANNEL->lfodepth+(1.0f-CHANNEL->lfodepth);
      int period=(int)(CHANNEL->lforate*channel->synth->tempo_frames);
      if (period<0) period=1;
      CHANNEL->lfodp=UINT32_MAX/period;
      if (!(CHANNEL->lfobuf=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT_FRAMES))) return -1;
      CHANNEL->use_lfo=1;
      CHANNEL->voice_update=synth_voice_fm_update_bells_whistles;
      return 0;
    }
    
    /* If either pitchenv or rangeenv in play, use them both.
     */
    if (
      (CHANNEL->pitchenv.pointc||CHANNEL->pitchenv.flags)||
      (CHANNEL->rangeenv.pointc||CHANNEL->rangeenv.flags)
    ) {
      if (!CHANNEL->pitchenv.pointc&&!CHANNEL->pitchenv.flags) {
        CHANNEL->pitchenv.initlo=CHANNEL->pitchenv.inithi=0.0f;
      } else {
        synth_env_bias(&CHANNEL->pitchenv,-0.5f);
        synth_env_scale(&CHANNEL->pitchenv,65535.0f/1200.0f);
      }
      CHANNEL->use_pitchenv=1;
      CHANNEL->use_moddp=1;
      if (!CHANNEL->rangeenv.pointc&&!CHANNEL->rangeenv.flags) {
        CHANNEL->rangeenv.initlo=CHANNEL->rangeenv.inithi=1.0f;
      } else {
        synth_env_scale(&CHANNEL->rangeenv,CHANNEL->modrange);
      }
      CHANNEL->use_rangeenv=1;
      CHANNEL->voice_update=synth_voice_fm_update_fullenv;
      return 0;
    }
    
    CHANNEL->use_moddp=1;
    CHANNEL->voice_update=synth_voice_fm_update_basicfm;
    return 0;
  }
  
  /* Pitch envelope is an extra expense.
   */
  if (CHANNEL->pitchenv.pointc||CHANNEL->pitchenv.flags) {
    synth_env_bias(&CHANNEL->pitchenv,-0.5f);
    synth_env_scale(&CHANNEL->pitchenv,65535.0f/1200.0f);
    CHANNEL->use_pitchenv=1;
    CHANNEL->voice_update=synth_voice_fm_update_pitchenv;
    return 0;
  }
  
  /* Finally, we have just wave, envelope, and wheel, the simplest case.
   * Our wave construction is pretty expressive, you'd be surprised how nice these can be.
   */
  CHANNEL->voice_update=synth_voice_fm_update_waveenv;
  return 0;
}

/* Init.
 */
 
int synth_channel_fm_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  channel->del=synth_channel_fm_del;
  
  int srcp=0,err,len;
  
  // level: It's ok to be empty; synth_env_decode will produce a default.
  if ((err=synth_env_decode(&CHANNEL->level,src+srcp,srcc-srcp,channel->synth->rate))<0) return err;
  srcp+=err;
  
  // wave: Plain sine if unspecified.
  if (srcp>=srcc) {
    if (!(CHANNEL->carrier=synth_wave_new(channel->synth,"\0\0\0",3))) return -1;
  } else {
    if ((len=synth_wave_measure(src+srcp,srcc-srcp))<1) return -1;
    if (!(CHANNEL->carrier=synth_wave_new(channel->synth,src+srcp,len))) return -1;
    srcp+=len;
  }
  
  // Everything else is truly optional...
  
  // pitchenv
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if ((err=synth_env_decode(&CHANNEL->pitchenv,src+srcp,srcc-srcp,channel->synth->rate))<0) return err;
  srcp+=err;
  
  // wheel range
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if (srcp>srcc-2) return -1;
  CHANNEL->wheelrange=(src[srcp]<<8)|src[srcp+1];
  srcp+=2;
  
  // mod rate
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if (srcp>srcc-2) return -1;
  CHANNEL->modrate=((src[srcp]<<8)|src[srcp+1])/256.0f;
  srcp+=2;
  
  // mod range
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if (srcp>srcc-2) return -1;
  CHANNEL->modrange=((src[srcp]<<8)|src[srcp+1])/256.0f;
  srcp+=2;
  
  // rangeenv
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if ((err=synth_env_decode(&CHANNEL->rangeenv,src+srcp,srcc-srcp,channel->synth->rate))<0) return err;
  srcp+=err;
  
  // lfo rate
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  if (srcp>srcc-2) return -1;
  CHANNEL->lforate=((src[srcp]<<8)|src[srcp+1])/256.0f;
  srcp+=2;
  
  // lfo depth
  if (srcp>=srcc) return synth_channel_fm_ready(channel);
  CHANNEL->lfodepth=src[srcp]/255.0f;
  srcp++;
  
  // Ignore extra content.
  return synth_channel_fm_ready(channel);
}

/* Terminate.
 */
 
void synth_channel_fm_terminate(struct synth_channel *channel) {
  const struct synth_fm_voice *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    int framec=synth_env_remaining(&voice->level);
    if (framec>channel->ttl) channel->ttl=framec;
  }
  if (channel->ttl<1) channel->ttl=1;
}

/* New voice.
 */
 
static struct synth_fm_voice *synth_fm_voice_new(struct synth_channel *channel) {
  struct synth_fm_voice *voice;
  
  // Easiest and very likely, grab a voice at the top of the stack.
  if (CHANNEL->voicec<CHANNEL->voicea) {
    voice=CHANNEL->voicev+CHANNEL->voicec++;
    memset(voice,0,sizeof(struct synth_fm_voice));
    return voice;
  }
  
  // Check for defunct voices. We only remove them from the top, so defunct voices can linger in the middle for a long time.
  int i=CHANNEL->voicec;
  for (voice=CHANNEL->voicev;i-->0;voice++) {
    if (synth_env_is_complete(&voice->level)) {
      memset(voice,0,sizeof(struct synth_fm_voice));
      return voice;
    }
  }
  
  // Grow the voice list.
  if (CHANNEL->voicea>SYNTH_FM_VOICE_LIMIT) return 0;
  int na=CHANNEL->voicea+8;
  void *nv=realloc(CHANNEL->voicev,sizeof(struct synth_fm_voice)*na);
  if (!nv) return 0;
  CHANNEL->voicev=nv;
  CHANNEL->voicea=na;
  voice=CHANNEL->voicev+CHANNEL->voicec++;
  memset(voice,0,sizeof(struct synth_fm_voice));
  return voice;
}

/* Note.
 */
 
void synth_channel_fm_note(struct synth_channel *channel,uint8_t noteid,float velocity,int dur) {
  if (noteid&0x80) return;
  struct synth_fm_voice *voice=synth_fm_voice_new(channel);
  if (!voice) return;
  
  voice->dp=channel->synth->rateiv[noteid];
  synth_env_reset(&voice->level,&CHANNEL->level,velocity,dur);
  
  if (CHANNEL->use_pitchenv) {
    synth_env_reset(&voice->pitchenv,&CHANNEL->pitchenv,velocity,dur);
  }
  
  if (CHANNEL->use_moddp) {
    voice->moddp=(uint32_t)((float)voice->dp*CHANNEL->modrate);
  }
  
  if (CHANNEL->use_rangeenv) {
    synth_env_reset(&voice->rangeenv,&CHANNEL->rangeenv,velocity,dur);
  }
}

/* Wheel.
 */
 
void synth_channel_fm_wheel(struct synth_channel *channel,uint8_t v) {
  //TODO wheel
}
