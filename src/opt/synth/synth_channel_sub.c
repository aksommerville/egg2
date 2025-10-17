/* synth_channel_sub.c
 */
 
#include "synth_internal.h"

#define SUB_VOICE_LIMIT 16
#define SUB_STAGE_LIMIT 8
#define WIDTH_MIN 0.000001
#define WIDTH_MAX 0.499999

struct iir {
  float v[5];
  float cv[5];
};

struct synth_sub_extra {
  struct synth_env levelenv;
  int stagec; // 0..SUB_STAGE_LIMIT
  float widthlo,widthhi; // WIDTH_MIN..WIDTH_MAX, normalized
  float gain;
  
  struct sub_voice {
    int holdid;
    struct synth_env levelenv;
    int noisep;
    struct iir iirv[SUB_STAGE_LIMIT];
  } voicev[SUB_VOICE_LIMIT];
  int voicec;
};

#define EXTRA ((struct synth_sub_extra*)channel->extra)

/* Generic 3-point IIR bandpass.
 */
 
static void iir_init_bpass(struct iir *iir,float center,float width) {
  /* 3-point IIR bandpass.
   * I have only a vague idea of how this works, and the formula is taken entirely on faith.
   * Reference:
   *   Steven W Smith: The Scientist and Engineer's Guide to Digital Signal Processing
   *   Ch 19, p 326, Equation 19-7
   */
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*center);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  iir->cv[0]=1.0f-k;
  iir->cv[1]=2.0f*(k-r)*cosfreq;
  iir->cv[2]=r*r-k;
  iir->cv[3]=2.0f*r*cosfreq;
  iir->cv[4]=-r*r;
  
  iir->v[0]=iir->v[1]=iir->v[2]=iir->v[3]=iir->v[4]=0.0f;
}
 
static float iir_update(struct iir *iir,float src) {
  iir->v[2]=iir->v[1];
  iir->v[1]=iir->v[0];
  iir->v[0]=src;
  float wet=
    iir->v[0]*iir->cv[0]+
    iir->v[1]*iir->cv[1]+
    iir->v[2]*iir->cv[2]+
    iir->v[3]*iir->cv[3]+
    iir->v[4]*iir->cv[4];
  iir->v[4]=iir->v[3];
  iir->v[3]=wet;
  return wet;
}

/* Cleanup.
 */
 
static void synth_channel_cleanup_sub(struct synth_channel *channel) {
}

/* Update.
 */
 
static void sub_voice_update(float *v,int c,struct synth_channel *channel,struct sub_voice *voice) {
  if (voice->levelenv.finished) return;
  const float *noisev=channel->synth->noisev;
  int noisec=channel->synth->noisec;
  for (;c-->0;v++) {
  
    float sample=noisev[voice->noisep++];
    if (voice->noisep>=noisec) voice->noisep=0;
    
    struct iir *iir=voice->iirv;
    int i=EXTRA->stagec;
    for (;i-->0;iir++) sample=iir_update(iir,sample);
    
    sample*=EXTRA->gain;
    if (sample<-1.0f) sample=-1.0f;
    else if (sample>1.0f) sample=1.0f;
    
    float level=synth_env_update(&voice->levelenv);
    //fprintf(stderr,"sample=%f level=%f\n",sample,level);
    (*v)+=sample*level;
  }
}
 
static void synth_channel_update_sub(float *v,int c,struct synth_channel *channel) {
  struct sub_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) sub_voice_update(v,c,channel,voice);
  while (EXTRA->voicec&&EXTRA->voicev[EXTRA->voicec-1].levelenv.finished) EXTRA->voicec--;
}

/* Init.
 */
 
int synth_channel_init_SUB(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (!(channel->extra=calloc(1,sizeof(struct synth_sub_extra)))) return -1;
  channel->del=synth_channel_cleanup_sub;
  
  if (synth_require_noise(channel->synth)<0) return -1;
  
  EXTRA->stagec=1;
  int srcp=0,err;
  int widthlohz=200,widthhihz=200;
  int gain88=0x0100;
  if (srcp>srcc-2) {
    synth_env_default_level(&EXTRA->levelenv,channel->rate);
    srcp=srcc;
  } else if (!src[srcp]&&!src[srcp+1]) {
    synth_env_default_level(&EXTRA->levelenv,channel->rate);
    srcp+=2;
  } else {
    if ((err=synth_env_decode(&EXTRA->levelenv,src+srcp,srcc-srcp,channel->rate))<0) return err;
    srcp+=err;
  }
  synth_env_scale(&EXTRA->levelenv,1.0f/65536.0f);
  if (srcp>srcc-2) srcp=srcc; else { widthlohz=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
  if (srcp>srcc-2) srcp=srcc; else { widthhihz=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
  if (srcp<srcc) EXTRA->stagec=src[srcp++];
  if (srcp>srcc-2) srcp=srcc; else { gain88=(src[srcp]<<8)|src[srcp+1]; srcp+=2; }
  
  if (EXTRA->stagec>SUB_STAGE_LIMIT) EXTRA->stagec=SUB_STAGE_LIMIT;
  EXTRA->widthlo=(float)widthlohz/(float)channel->synth->rate;
       if (EXTRA->widthlo<WIDTH_MIN) EXTRA->widthlo=WIDTH_MIN;
  else if (EXTRA->widthlo>WIDTH_MAX) EXTRA->widthlo=WIDTH_MAX;
  EXTRA->widthhi=(float)widthhihz/(float)channel->synth->rate;
       if (EXTRA->widthhi<WIDTH_MIN) EXTRA->widthhi=WIDTH_MIN;
  else if (EXTRA->widthhi>WIDTH_MAX) EXTRA->widthhi=WIDTH_MAX;
  EXTRA->gain=(float)gain88/256.0f;
  
  channel->update_mono=synth_channel_update_sub;
  return 0;
}

/* Begin note.
 */
 
int synth_channel_note_sub(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {

  // Find an available voice.
  struct sub_voice *voice=0;
  if (EXTRA->voicec<SUB_VOICE_LIMIT) {
    voice=EXTRA->voicev+EXTRA->voicec++;
  } else {
    voice=EXTRA->voicev;
    struct sub_voice *q=EXTRA->voicev;
    int i=SUB_VOICE_LIMIT;
    for (;i-->0;q++) {
      if (q->levelenv.finished) {
        voice=q;
        break;
      } else if (q->holdid<voice->holdid) { // Assume that lower holdid are older, and more amenable to eviction.
        voice=q;
      }
    }
  }
  
  // Prepare IIRs.
  if (EXTRA->stagec>0) {
    float rate=channel->synth->ratefv[noteid&0x7f];
    float width=EXTRA->widthlo*(1.0f-velocity)+EXTRA->widthhi*velocity;
    iir_init_bpass(voice->iirv,rate,width);
    int i=1;
    for (;i<EXTRA->stagec;i++) voice->iirv[i]=voice->iirv[0];
  }
  
  // Other uncontroversial prep.
  synth_env_apply(&voice->levelenv,&EXTRA->levelenv,velocity,durframes);
  voice->noisep=0;
  voice->holdid=synth_holdid_next(channel->synth);
  
  return voice->holdid;
}

/* Release note.
 */
 
void synth_channel_release_sub(struct synth_channel *channel) {
  struct sub_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) synth_env_release(&voice->levelenv);
}

void synth_channel_release_one_sub(struct synth_channel *channel,int holdid) {
  struct sub_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) {
    if (voice->holdid==holdid) {
      synth_env_release(&voice->levelenv);
      return;
    }
  }
}
