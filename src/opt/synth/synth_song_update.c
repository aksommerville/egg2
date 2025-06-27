#include "synth_internal.h"

/* Update song's signal graph, main entry point.
 * Add to (v).
 * Return >0 to stay alive.
 */
  
int synth_song_update(float *v,int framec,struct synth_song *song) {
  //TODO
  if ((song->delay-=framec)<0) song->delay=0;
  return 1;
}
