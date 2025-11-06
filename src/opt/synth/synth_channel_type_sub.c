/* synth_channel_type_sub.c
 * Simple and highly specific voicing mode, producing a scrapey effect that really can't be achieved any other way.
 * Generate white noise and pass it through a bandpass filter.
 * Pitch wheel not supported.
 */
 
#include "synth_internal.h"

// We'll artificially refuse to create more than so many voices.
#define SUB_VOICE_LIMIT 32

#define SUB_STAGE_LIMIT 10

struct synth_voice_sub {
  struct synth_iir3 iir3v[SUB_STAGE_LIMIT];
  struct synth_env levelenv;
  uint8_t noteid;
  int seq;
};

struct synth_channel_sub {
  struct synth_channel hdr;
  struct synth_voice_sub *voicev;
  int voicec,voicea;
  float *noise; // synth.buffer_limit. Generated at the start of each update and shared across voices.
  struct synth_env levelenv;
  int stagec; // 0..SUB_STAGE_LIMIT
  float widthlo,widthhi; // Normalized.
  float gain;
  int seq_next;
  uint32_t randstate;
  int longdurframes;
};

#define CHANNEL ((struct synth_channel_sub*)channel)

/* Cleanup.
 */
 
static void _sub_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) synth_free(CHANNEL->voicev);
  if (CHANNEL->noise) synth_free(CHANNEL->noise);
}

/* Update.
 */
 
static void synth_voice_sub_update(float *dst,struct synth_voice_sub *voice,struct synth_channel *channel,int framec) {
  const float *src=CHANNEL->noise;
  for (;framec-->0;dst++,src++) {
    float sample=*src;
    struct synth_iir3 *iir3=voice->iir3v;
    int i=CHANNEL->stagec;
    for (;i-->0;iir3++) sample=synth_iir3_update(iir3,sample);
    sample*=CHANNEL->gain;
    sample*=synth_env_update(voice->levelenv);
    (*dst)+=sample;
  }
}
 
static void _sub_update_mono(float *dst,struct synth_channel *channel,int framec) {
  if (CHANNEL->voicec<1) return;
  CHANNEL->randstate=synth_rand(CHANNEL->noise,framec,CHANNEL->randstate);
  struct synth_voice_sub *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) synth_voice_sub_update(dst,voice,channel,framec);
  while (CHANNEL->voicec&&CHANNEL->voicev[CHANNEL->voicec-1].levelenv.finished) CHANNEL->voicec--;
}

/* Initialize.
 */
 
static int _sub_init(struct synth_channel *channel,const uint8_t *src,int srcc) {

  // Decode config.
  int srcp=0,err;
  int wlohz=200,whihz=200;
  if ((err=synth_env_decode(&CHANNEL->levelenv,src+srcp,srcc-srcp,SYNTH_ENV_FALLBACK_LEVEL))<0) return err; srcp+=err;
  if (srcp<=srcc-2) { wlohz=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
  if (srcp<=srcc-2) { whihz=(src[srcp]<<8)|src[srcp+1]; srcp+=2; } else whihz=wlohz;
  if (srcp<srcc) CHANNEL->stagec=src[srcp++]; else CHANNEL->stagec=1;
  if (srcp<=srcc-2) { CHANNEL->gain=src[srcp]+src[srcp+1]/256.0f; srcp+=2; } else CHANNEL->gain=1.0f;
  
  // Sanitize config.
  if (CHANNEL->stagec>SUB_STAGE_LIMIT) CHANNEL->stagec=SUB_STAGE_LIMIT;
  CHANNEL->widthlo=(float)wlohz/(float)synth.rate;
  CHANNEL->widthhi=(float)whihz/(float)synth.rate;
  if (CHANNEL->widthlo<0.000001f) CHANNEL->widthlo=0.000001f; else if (CHANNEL->widthlo>0.499f) CHANNEL->widthlo=0.499f;
  if (CHANNEL->widthhi<0.000001f) CHANNEL->widthhi=0.000001f; else if (CHANNEL->widthhi>0.499f) CHANNEL->widthhi=0.499f;
  
  // Allocate noise buffer.
  if (!(CHANNEL->noise=synth_malloc(sizeof(float)*synth.buffer_frames))) return -1;
  CHANNEL->randstate=0xaaaaaaaa;
  
  CHANNEL->longdurframes=SYNTH_DEFAULT_HOLD_TIME_S*synth.rate;
  
  channel->update_mono=_sub_update_mono;

  return 0;
}

/* Release all.
 */
 
static void _sub_release_all(struct synth_channel *channel) {
  struct synth_voice_sub *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    synth_env_release(&voice->levelenv);
    voice->noteid=0xff;
  }
}

/* Begin note.
 */
 
static void _sub_note_once(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  if (noteid&0x80) return;
  
  struct synth_voice_sub *voice=0;
  if (CHANNEL->voicec<CHANNEL->voicea) {
    voice=CHANNEL->voicev+CHANNEL->voicec++;
  } else {
    struct synth_voice_sub *q=CHANNEL->voicev;
    int i=CHANNEL->voicec;
    for (;i-->0;q++) {
      if (q->levelenv.finished) {
        voice=q;
        break;
      }
    }
    if (!voice) {
      if (CHANNEL->voicea>=SUB_VOICE_LIMIT) return;
      int na=CHANNEL->voicea+8;
      if (na>INT_MAX/sizeof(struct synth_voice_sub)) return;
      void *nv=synth_realloc(CHANNEL->voicev,sizeof(struct synth_voice_sub)*na);
      if (!nv) return;
      CHANNEL->voicev=nv;
      CHANNEL->voicea=na;
      voice=CHANNEL->voicev+CHANNEL->voicec++;
    }
  }
  
  voice->noteid=noteid;
  if (CHANNEL->seq_next<1) CHANNEL->seq_next=1;
  voice->seq=CHANNEL->seq_next++;
  synth_env_apply(&voice->levelenv,&CHANNEL->levelenv,velocity,durframes);
  
  if (CHANNEL->stagec) {
    float mid=synth.fratev[noteid];
    float wid=CHANNEL->widthlo*(1.0f-velocity)+CHANNEL->widthhi*velocity;
    synth_iir3_init_bpass(voice->iir3v,mid,wid);
    struct synth_iir3 *iir3=voice->iir3v+1;
    int i=CHANNEL->stagec;
    for (;i-->1;iir3++) *iir3=voice->iir3v[0];
  }
}

static void _sub_note_on(struct synth_channel *channel,uint8_t noteid,float velocity) {
  _sub_note_once(channel,noteid,velocity,CHANNEL->longdurframes);
}

/* Release note.
 */
 
static void _sub_note_off(struct synth_channel *channel,uint8_t noteid) {
  struct synth_voice_sub *voice=CHANNEL->voicev;
  int i=CHANNEL->voicec;
  for (;i-->0;voice++) {
    if (voice->noteid!=noteid) continue;
    voice->noteid=0xff;
    synth_env_release(&voice->levelenv);
  }
}

/* Type definition.
 */
 
const struct synth_channel_type synth_channel_type_sub={
  .name="sub",
  .objlen=sizeof(struct synth_channel_sub),
  .del=_sub_del,
  .init=_sub_init,
  .release_all=_sub_release_all,
  .note_once=_sub_note_once,
  .note_on=_sub_note_on,
  .note_off=_sub_note_off,
};
