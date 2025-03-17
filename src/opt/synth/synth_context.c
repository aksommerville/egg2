#include "synth_internal.h"

/* Delete.
 */
 
void synth_del(struct synth *synth) {
  if (!synth) return;
  free(synth);
}

/* New.
 */
 
struct synth *synth_new(int rate,int chanc) {
  if ((rate<SYNTH_RATE_MIN)||(rate>SYNTH_RATE_MAX)) return 0;
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  synth->rate=rate;
  synth->chanc=chanc;
  return synth;
}

/* Load resources.
 */

int synth_load_song(struct synth *synth,int id,const void *src,int srcc) {
  return -1;
}

int synth_load_sound(struct synth *synth,int id,const void *src,int srcc) {
  return -1;
}

/* Update, public entry points.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  memset(v,0,sizeof(float)*c);
}

void synth_updatei(int16_t *v,int c,struct synth *synth) {
  memset(v,0,c<<1);
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int songid,int force,int repeat) {
}

/* Play sound.
 */
 
void synth_play_sound(struct synth *synth,int soundid,double trim,double pan) {
}

/* Trivial accessors.
 */

int synth_get_song_id(const struct synth *synth) {
  return 0;
}

double synth_get_playhead(const struct synth *synth) {
  return 0.0;
}

void synth_set_playhead(struct synth *synth,double s) {
}
