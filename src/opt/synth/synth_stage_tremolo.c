#include "synth_internal.h"

#define STAGE ((struct synth_stage_tremolo*)stage)

/* Cleanup.
 */
 
static void _tremolo_del(struct synth_stage *stage) {
  synth_wave_del(STAGE->wave);
}

/* Update.
 */
 
static void _tremolo_update_mono(float *v,int c,struct synth_stage *stage) {
  for (;c-->0;v++) {
    float adjust=STAGE->wave->v[STAGE->p>>SYNTH_WAVE_SHIFT];
    STAGE->p+=STAGE->dp;
    v[0]*=adjust;
  }
}

static void _tremolo_update_stereo(float *v,int c,struct synth_stage *stage) {
  for (;c-->0;v+=2) {
    float adjust=STAGE->wave->v[STAGE->p>>SYNTH_WAVE_SHIFT];
    STAGE->p+=STAGE->dp;
    v[0]*=adjust;
    v[1]*=adjust;
  }
}

/* Init.
 */
 
int synth_stage_tremolo_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=4) return -1;
  double qnote256=(src[0]<<8)|src[1];
  float depth=src[2]/255.0f;
  STAGE->p=(src[3]<<24)|(src[3]<<16)|(src[3]<<8)|src[3];
  int period=(int)((qnote256*stage->synth->tempo_frames)/256.0);
  if (period<1) period=1;
  STAGE->dp=0xffffffff/period;
  if (!(STAGE->wave=synth_wave_new(0,"\0\0\0",3))) return -1;
  depth*=0.5f;
  float bias=1.0f-depth;
  float *v=STAGE->wave->v;
  int c=SYNTH_WAVE_SIZE_SAMPLES;
  for (;c-->0;v++) {
    *v=(*v)*depth+bias;
  }
  stage->del=_tremolo_del;
  if (stage->chanc==1) stage->update=_tremolo_update_mono;
  else if (stage->chanc==2) stage->update=_tremolo_update_stereo;
  return 0;
}
