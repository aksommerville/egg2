#include "synth_internal.h"

struct synth_drum_extra {
  int TODO;
};

#define EXTRA ((struct synth_drum_extra*)channel->extra)

/* Cleanup.
 */
 
static void synth_channel_cleanup_drum(struct synth_channel *channel) {
  if (!channel->extra) return;
  free(channel->extra);
}

/* Initialize drum channel.
 * Set (mode=0) if we determine it's noop.
 */
 
int synth_channel_init_DRUM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc) {
  fprintf(stderr,"TODO %s chid=%d modecfgc=%d\n",__func__,channel->chid,modecfgc);//TODO
  return 0;
}

/* Play note.
 */
 
void synth_channel_note_drum(struct synth_channel *channel,uint8_t noteid,float velocity) {
  //TODO
}
