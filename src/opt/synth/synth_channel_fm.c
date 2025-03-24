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
  synth_wave_del(CHANNEL->carrier);
}

/* Update.
 */
 
static void synth_voice_fm_update(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel) {
  const float *src=CHANNEL->carrier->v;
  for (;c-->0;v++) {
    //TODO fm, wheel, lfo, all the things
    float sample=src[voice->p>>SYNTH_WAVE_SHIFT];
    //if (sample<slo) slo=sample; else if (sample>shi) shi=sample;
    voice->p+=voice->dp;
    float level=synth_env_update(&voice->level);
    //if (level<llo) llo=sample; else if (level>lhi) lhi=sample;
    sample*=level;
    (*v)+=sample;
  }
}
 
static void synth_channel_fm_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c);
  if (!CHANNEL->voicec) return;
  struct synth_fm_voice *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_fm_update(v,c,voice,channel);
  while (CHANNEL->voicec&&synth_env_is_complete(&CHANNEL->voicev[CHANNEL->voicec-1].level)) CHANNEL->voicec--;
}

/* Init.
 */
 
int synth_channel_fm_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  channel->del=synth_channel_fm_del;
  
  int srcp=0,err,len;
  if ((err=synth_env_decode(&CHANNEL->level,src+srcp,srcc-srcp,channel->synth->rate))<0) return err;
  srcp+=err;
  
  if ((len=synth_wave_measure(src+srcp,srcc-srcp))<1) return -1;
  if (!(CHANNEL->carrier=synth_wave_new(channel->synth,src+srcp,len))) return -1;
  srcp+=len;
  
  //TODO pitchenv
  //TODO u16 wheel range
  //TODO u8.8 modrate
  //TODO u8.8 modrange
  //TODO rangeenv
  //TODO u8.8 rangelforate, qnotes
  //TODO u0.8 rangelfodepth
  
  channel->chanc=1;
  channel->update=synth_channel_fm_update;
  return 0;
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
  //TODO this is very temporary, just testing the works
  //fprintf(stderr,"%s ok noteid=0x%02x dur=%d remaining=%d\n",__func__,noteid,dur,synth_env_remaining(&voice->level));
}

/* Wheel.
 */
 
void synth_channel_fm_wheel(struct synth_channel *channel,uint8_t v) {
  //TODO wheel
}
