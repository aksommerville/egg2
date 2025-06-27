/* synth.h
 * Egg synthesizer, native implementation.
 * This is meant to be as portable as possible. Only dependencies are libc and libm.
 * (note that we do not use Egg's "eau" unit; that's only for tooling).
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

struct synth;

void synth_del(struct synth *synth);

struct synth *synth_new(int rate,int chanc);

/* Install all resources soon after construction.
 * All (v) must remain constant and valid throughout synth's life. Presumably from a ROM.
 * IDs must be greater than zero.
 */
int synth_install_song(struct synth *synth,int songid,const void *v,int c);
int synth_install_sound(struct synth *synth,int soundid,const void *v,int c);

/* These ensure that the object is not in use (possibly stopping something cold), then unlist it from our registry.
 * The original serial buffer is returned on success; you may free it after.
 * Editor might need this when it's playing temporary songs.
 */
void *synth_uninstall_song(struct synth *synth,int songid);
void *synth_uninstall_sound(struct synth *synth,int soundid);

/* Stop the current song and begin a new one.
 * (force) to restart even if already playing.
 * Only one song is officially playing at a time and that changes immediately during this call.
 * We may or may not do a brief crossfade or something. You shouldn't need to worry about it.
 */
void synth_play_song(struct synth *synth,int songid,int force,int repeat);

/* Play a fire-and-forget sound effect.
 */
void synth_play_sound(struct synth *synth,int soundid,float trim,float pan);

/* Advance time and generate (c) samples (not frames, not bytes).
 * (c) must be a multiple of (chanc).
 * Floating-point is more natural to us, but we can quantize to s16 for you.
 */
void synth_updatef(float *v,int c,struct synth *synth);
void synth_updatei(int16_t *v,int c,struct synth *synth);

/* Access to current song.
 * (songid) is zero if no song is playing, even if you asked for a nonzero but nonexistant song.
 * Same, zero, if you played a song without repeat and it has finished.
 * (playhead) is in seconds from the start of the song, wrapping back on repeats, and zero if no song.
 */
int synth_get_songid(const struct synth *synth);
float synth_get_playhead(const struct synth *synth);
void synth_set_playhead(struct synth *synth,float s);

#endif
