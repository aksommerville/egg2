#include "synth_internal.h"

#define STAGE ((struct synth_stage_iir*)stage)

/* Init.
 */
 
int synth_stage_lopass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Init.
 */
 
int synth_stage_hipass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Init.
 */
 
int synth_stage_bpass_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}

/* Init.
 */
 
int synth_stage_notch_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s\n",__func__);//TODO
  return 0;
}
