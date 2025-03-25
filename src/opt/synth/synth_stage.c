#include "synth_internal.h"

/* Delete.
 */
 
void synth_stage_del(struct synth_stage *stage) {
  if (!stage) return;
  if (stage->del) stage->del(stage);
  free(stage);
}

/* New.
 */
 
static void synth_stage_update_noop(float *v,int c,struct synth_stage *stage) {}

struct synth_stage *synth_stage_new(struct synth *synth,int chanc,int stageid,const void *src,int srcc) {
  if ((chanc<1)||(chanc>2)) return 0;
  if ((srcc<0)||(srcc&&!src)) return 0;
  struct synth_stage *stage=0;
  switch (stageid) {
    case EAU_STAGEID_GAIN: stage=calloc(1,sizeof(struct synth_stage_gain)); break;
    case EAU_STAGEID_DELAY: stage=calloc(1,sizeof(struct synth_stage_delay)); break;
    case EAU_STAGEID_LOPASS: stage=calloc(1,sizeof(struct synth_stage_iir)); break;
    case EAU_STAGEID_HIPASS: stage=calloc(1,sizeof(struct synth_stage_iir)); break;
    case EAU_STAGEID_BPASS: stage=calloc(1,sizeof(struct synth_stage_iir)); break;
    case EAU_STAGEID_NOTCH: stage=calloc(1,sizeof(struct synth_stage_iir)); break;
    case EAU_STAGEID_WAVESHAPER: stage=calloc(1,sizeof(struct synth_stage_waveshaper)); break;
  }
  if (!stage) return 0;
  stage->synth=synth;
  stage->chanc=chanc;
  stage->update=synth_stage_update_noop;
  int err=-1;
  switch (stageid) {
    case EAU_STAGEID_GAIN: err=synth_stage_gain_init(stage,src,srcc); break;
    case EAU_STAGEID_DELAY: err=synth_stage_delay_init(stage,src,srcc); break;
    case EAU_STAGEID_LOPASS: err=synth_stage_lopass_init(stage,src,srcc); break;
    case EAU_STAGEID_HIPASS: err=synth_stage_hipass_init(stage,src,srcc); break;
    case EAU_STAGEID_BPASS: err=synth_stage_bpass_init(stage,src,srcc); break;
    case EAU_STAGEID_NOTCH: err=synth_stage_notch_init(stage,src,srcc); break;
    case EAU_STAGEID_WAVESHAPER: err=synth_stage_waveshaper_init(stage,src,srcc); break;
  }
  if (err<0) {
    synth_stage_del(stage);
    return 0;
  }
  return stage;
}
