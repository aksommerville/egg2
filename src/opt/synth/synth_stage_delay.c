#include "synth_internal.h"

#define STAGE ((struct synth_stage_delay*)stage)

/* Cleanup.
 */
 
static void _delay_del(struct synth_stage *stage) {
  synth_ring_cleanup(&STAGE->ring);
}

/* Update.
 */
 
static void _delay_update_mono(float *v,int c,struct synth_stage *stage) {
  for (;c-->0;v++) {
    float dry=*v;
    float prv=synth_ring_read(&STAGE->ring);
    *v=dry*STAGE->dry+prv*STAGE->wet;
    synth_ring_write(&STAGE->ring,dry*STAGE->sto+prv*STAGE->fbk);
    synth_ring_advance(&STAGE->ring);
  }
}

static void _delay_update_stereo(float *v,int c,struct synth_stage *stage) {
  _delay_update_mono(v,c<<1,stage);
}

/* Init.
 */
 
int synth_stage_delay_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  if (srcc!=6) return -1;
  double qnote256=(src[0]<<8)|src[1];
  STAGE->dry=src[2]/255.0f;
  STAGE->wet=src[3]/255.0f;
  STAGE->sto=src[4]/255.0f;
  STAGE->fbk=src[5]/255.0f;
  int period=(int)((qnote256*stage->synth->tempo_frames)/256.0);
  if (period<1) period=1;
  if (stage->chanc==2) {
    period<<=1;
    stage->update=_delay_update_stereo;
  } else if (stage->chanc==1) {
    stage->update=_delay_update_mono;
  }
  if (synth_ring_init(&STAGE->ring,period)<0) return -1;
  return 0;
}
