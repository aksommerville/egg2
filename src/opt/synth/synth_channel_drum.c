#include "synth_internal.h"

/* Update.
 */
 
static void synth_channel_drum_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c);
  //TODO
}

/* Init.
 */
 
int synth_channel_drum_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  channel->chanc=1;//XXX We must support stereo eventually.
  channel->update=synth_channel_drum_update;
  return 0;
}

/* Terminate.
 */
 
void synth_channel_drum_terminate(struct synth_channel *channel) {
}

/* Note.
 */
 
void synth_channel_drum_note(struct synth_channel *channel,uint8_t noteid,float velocity) {
}
