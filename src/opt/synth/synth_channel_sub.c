#include "synth_internal.h"

/* Update.
 */
 
static void synth_channel_sub_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c);
}

/* Init.
 */
 
int synth_channel_sub_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  channel->chanc=1;
  channel->update=synth_channel_sub_update;
  return 0;
}

/* Terminate.
 */
 
void synth_channel_sub_terminate(struct synth_channel *channel) {
}

/* Note.
 */
 
void synth_channel_sub_note(struct synth_channel *channel,uint8_t noteid,float velocity,int dur) {
}
