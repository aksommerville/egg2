#include "synth_internal.h"

#define SYNTH_SUB_VOICE_LIMIT 32

#define CHANNEL ((struct synth_channel_sub*)channel)

/* Cleanup.
 */
 
static void _sub_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) {
    free(CHANNEL->voicev);
  }
}

/* Update.
 */
 
static void synth_sub_voice_update(float *v,int c,struct synth_sub_voice *voice,struct synth_channel *channel) {
  for (;c-->0;v++) {
    float sample=((rand()&0xffff)-32768)/32768.0f;
    struct synth_iir3 *iir=voice->iirv;
    int i=CHANNEL->stagec;
    for (;i-->0;iir++) {
      sample=synth_iir3_update(iir,sample);
    }
    sample*=synth_env_update(&voice->level);
    (*v)+=sample;
  }
}
 
static void _sub_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c);
  struct synth_sub_voice *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_sub_voice_update(v,c,voice,channel);
  while (CHANNEL->voicec&&synth_env_is_complete(&CHANNEL->voicev[CHANNEL->voicec-1].level)) CHANNEL->voicec--;
}

/* Init.
 */
 
int synth_channel_sub_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  float gain=1.0f;
  CHANNEL->widthlo=CHANNEL->widthhi=100.0f/(float)channel->synth->rate;

  int srcp=synth_env_decode(&CHANNEL->level,src,srcc,channel->synth->rate);
  if (srcp<0) return srcp;
  
  if (srcp>=srcc) goto _ready_;
  if (srcp>srcc-2) return -1;
  CHANNEL->widthlo=(float)((src[srcp]<<8)|src[srcp+1])/(float)channel->synth->rate;
  srcp+=2;
  
  if (srcp>=srcc) { CHANNEL->widthhi=CHANNEL->widthlo; goto _ready_; }
  if (srcp>srcc-2) return -1;
  CHANNEL->widthhi=(float)((src[srcp]<<8)|src[srcp+1])/(float)channel->synth->rate;
  srcp+=2;
  
  if (srcp>=srcc) goto _ready_;
  CHANNEL->stagec=src[srcp++];
  
  if (srcp>=srcc) goto _ready_;
  gain=((src[srcp]<<8)|src[srcp+1])/256.0f;
  srcp+=2;
  
 _ready_:;
  synth_env_scale(&CHANNEL->level,gain);
  if (CHANNEL->widthlo<0.000001f) CHANNEL->widthlo=0.000001f;
  else if (CHANNEL->widthlo>0.5f) CHANNEL->widthlo=0.5f;
  if (CHANNEL->widthhi<0.000001f) CHANNEL->widthhi=0.000001f;
  else if (CHANNEL->widthhi>0.5f) CHANNEL->widthhi=0.5f;
  if (CHANNEL->stagec<1) CHANNEL->stagec=1;
  else if (CHANNEL->stagec>SYNTH_SUB_STAGE_LIMIT) CHANNEL->stagec=SYNTH_SUB_STAGE_LIMIT;
  channel->chanc=1;
  channel->update=_sub_update;
  channel->del=_sub_del;
  return 0;
}

/* Terminate.
 */
 
void synth_channel_sub_terminate(struct synth_channel *channel) {
  const struct synth_sub_voice *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    int framec=synth_env_remaining(&voice->level);
    if (framec>channel->ttl) channel->ttl=framec;
  }
  if (channel->ttl<1) channel->ttl=1;
}

/* New voice.
 */
 
static struct synth_sub_voice *synth_sub_voice_new(struct synth_channel *channel) {
  struct synth_sub_voice *voice;
  
  // Easiest and very likely, grab a voice at the top of the stack.
  if (CHANNEL->voicec<CHANNEL->voicea) {
    voice=CHANNEL->voicev+CHANNEL->voicec++;
    memset(voice,0,sizeof(struct synth_sub_voice));
    return voice;
  }
  
  // Check for defunct voices. We only remove them from the top, so defunct voices can linger in the middle for a long time.
  int i=CHANNEL->voicec;
  for (voice=CHANNEL->voicev;i-->0;voice++) {
    if (synth_env_is_complete(&voice->level)) {
      memset(voice,0,sizeof(struct synth_sub_voice));
      return voice;
    }
  }
  
  // Grow the voice list.
  if (CHANNEL->voicea>SYNTH_SUB_VOICE_LIMIT) return 0;
  int na=CHANNEL->voicea+8;
  void *nv=realloc(CHANNEL->voicev,sizeof(struct synth_sub_voice)*na);
  if (!nv) return 0;
  CHANNEL->voicev=nv;
  CHANNEL->voicea=na;
  voice=CHANNEL->voicev+CHANNEL->voicec++;
  memset(voice,0,sizeof(struct synth_sub_voice));
  return voice;
}

/* Note.
 */
 
void synth_channel_sub_note(struct synth_channel *channel,uint8_t noteid,float velocity,int dur) {
  if (noteid>=0x80) return;
  
  struct synth_sub_voice *voice=synth_sub_voice_new(channel);
  if (!voice) return;
  
  synth_env_reset(&voice->level,&CHANNEL->level,velocity,dur);
  
  float mid=channel->synth->ratefv[noteid];
  float wid=CHANNEL->widthlo*(1.0f-velocity)+CHANNEL->widthhi*velocity;
  synth_iir3_init_bpass(voice->iirv,mid,wid);
  int i=1; for (;i<CHANNEL->stagec;i++) voice->iirv[i]=voice->iirv[0];
}
