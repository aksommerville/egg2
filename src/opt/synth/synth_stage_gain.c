#include "synth_internal.h"

#define STAGE ((struct synth_stage_gain*)stage)

/* Update.
 */
 
static void _gain_update_mono(float *v,int framec,struct synth_stage *stage) {
  float nclip=-STAGE->clip;
  for (;framec-->0;v++) {
    (*v)*=STAGE->gain;
    if (*v<nclip) *v=nclip;
    else if (*v>STAGE->clip) *v=STAGE->clip;
  }
}
 
static void _gain_update_stereo(float *v,int framec,struct synth_stage *stage) {
  float nclip=-STAGE->clip;
  framec<<=1;
  for (;framec-->0;v++) {
    (*v)*=STAGE->gain;
    if (*v<nclip) *v=nclip;
    else if (*v>STAGE->clip) *v=STAGE->clip;
  }
}

/* Init.
 */
 
int synth_stage_gain_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=3) return -1;
  STAGE->gain=((src[0]<<8)|src[1])/256.0f;
  STAGE->clip=src[2]/255.0f;
  if (stage->chanc==1) stage->update=_gain_update_mono;
  else if (stage->chanc==2) stage->update=_gain_update_stereo;
  return 0;
}
