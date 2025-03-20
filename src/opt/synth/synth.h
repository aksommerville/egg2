/* synth.h
 * Pure software audio synthesizer for Egg.
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;

void synth_del(struct synth *synth);
struct synth *synth_new(int rate,int chanc);

/* Resources are borrowed forever. We assume they are constant and immortal.
 */
int synth_load_song(struct synth *synth,int id,const void *src,int srcc);
int synth_load_sound(struct synth *synth,int id,const void *src,int srcc);

void synth_updatef(float *v,int c,struct synth *synth);
void synth_updatei(int16_t *v,int c,struct synth *synth);

void synth_play_song(struct synth *synth,int songid,int force,int repeat);
void synth_play_sound(struct synth *synth,int soundid,double trim,double pan);

int synth_get_song_id(const struct synth *synth);
double synth_get_playhead(const struct synth *synth);
void synth_set_playhead(struct synth *synth,double s);

#endif
