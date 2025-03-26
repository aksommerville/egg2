#include "synth_internal.h"

#define STAGE ((struct synth_stage_iir*)stage)

/* Update.
 */
 
static void _iir_update_mono(float *v,int framec,struct synth_stage *stage) {
  for (;framec-->0;v++) *v=synth_iir3_update(&STAGE->iir,*v);
}

static void _iir_update_stereo(float *v,int framec,struct synth_stage *stage) {
  while (framec-->0) {
    *v=synth_iir3_update(&STAGE->iir,*v); v++;
    *v=synth_iir3_update(&STAGE->r,*v); v++;
  }
}

/* Init.
 */
 
int synth_stage_lopass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=2) return -1;
  float freq=((src[0]<<8)|src[1])/(float)stage->synth->rate;
  if (freq<0.0f) freq=0.0f; else if (freq>0.5f) freq=0.5f;
  synth_iir3_init_lopass(&STAGE->iir,freq);
  if (stage->chanc==2) {
    STAGE->r=STAGE->iir;
    stage->update=_iir_update_stereo;
  } else {
    stage->update=_iir_update_mono;
  }
  return 0;
}

/* Init.
 */
 
int synth_stage_hipass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=2) return -1;
  float freq=((src[0]<<8)|src[1])/(float)stage->synth->rate;
  if (freq<0.0f) freq=0.0f; else if (freq>0.5f) freq=0.5f;
  synth_iir3_init_hipass(&STAGE->iir,freq);
  if (stage->chanc==2) {
    STAGE->r=STAGE->iir;
    stage->update=_iir_update_stereo;
  } else {
    stage->update=_iir_update_mono;
  }
  return 0;
}

/* Init.
 */
 
int synth_stage_bpass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=4) return -1;
  float mid=((src[0]<<8)|src[1])/(float)stage->synth->rate;
  float wid=((src[2]<<8)|src[3])/(float)stage->synth->rate;
  if (mid<0.0f) mid=0.0f; else if (mid>0.5f) mid=0.5f;
  if (wid<0.0f) wid=0.0f; else if (wid>0.5f) wid=0.5f;
  synth_iir3_init_bpass(&STAGE->iir,mid,wid);
  if (stage->chanc==2) {
    STAGE->r=STAGE->iir;
    stage->update=_iir_update_stereo;
  } else {
    stage->update=_iir_update_mono;
  }
  return 0;
}

/* Init.
 */
 
int synth_stage_notch_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=4) return -1;
  float mid=((src[0]<<8)|src[1])/(float)stage->synth->rate;
  float wid=((src[2]<<8)|src[3])/(float)stage->synth->rate;
  if (mid<0.0f) mid=0.0f; else if (mid>0.5f) mid=0.5f;
  if (wid<0.0f) wid=0.0f; else if (wid>0.5f) wid=0.5f;
  synth_iir3_init_notch(&STAGE->iir,mid,wid);
  if (stage->chanc==2) {
    STAGE->r=STAGE->iir;
    stage->update=_iir_update_stereo;
  } else {
    stage->update=_iir_update_mono;
  }
  return 0;
}
