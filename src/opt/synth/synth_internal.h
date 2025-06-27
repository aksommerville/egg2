#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define SYNTH_RATE_MIN 2000
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8
#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)
#define SYNTH_ID_SONG 0x10000
#define SYNTH_ID_SOUND 0x20000
#define SYNTH_UPDATE_LIMIT 1024 /* Longest update of the signal graph, in frames. */
#define SYNTH_PCM_LIMIT_SAMPLES (1<<20) /* Sanity limit. Keep it way too long, since it doesn't account for rate. */

struct synth_pcm;
struct synth_wave;
struct synth_printer;
struct synth_pcmplay;
struct synth_song;

/* PCM dump etc.
 *****************************************************************************/
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
int synth_wave_ref(struct synth_wave *wave);
struct synth_wave *synth_wave_new();

struct synth_printer {
  struct synth_song *song;
  struct synth_pcm *pcm;
  int p;
};

void synth_printer_del(struct synth_printer *printer);
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);
int synth_printer_update(struct synth_printer *printer,int c); // >0 if still running

struct synth_pcmplay {
  struct synth_pcm *pcm;
  int p;
  int chanc;
  float triml,trimr;
};

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay);
int synth_pcmplay_init(struct synth_pcmplay *pcmplay,struct synth_pcm *pcm,int chanc,float trim,float pan);
int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay); // adds; >0 if still running

/* Song player.
 *******************************************************************************/
 
struct synth_song {
  struct synth *synth; // WEAK, REQUIRED
  int songid; // 0..0xffff
  int repeat;
  int chanc; // Matches (synth) for the main song, but will always be 1 for pcm printers.
  int delay; // frames
  const uint8_t *v; // Events only.
  int c;
  int p; // Position in (v), past any delay events already added to (delay).
  int phframes; // Playhead, in frames. Resets at loop.
  int tempo; // frames/qnote, established at construction.
  int loopp;
  int terminated;
  //TODO voicing etc
};

void synth_song_del(struct synth_song *song);
struct synth_song *synth_song_new(struct synth *synth,const void *v,int c,int repeat,int chanc);

/* Release all held notes and poison the player so as not to accept any new events.
 * A terminated song must report completion soon.
 */
void synth_song_terminate(struct synth_song *song);

float synth_song_get_playhead(const struct synth_song *song);
void synth_song_set_playhead(struct synth_song *song,float s);
int synth_song_frames_for_bytes(const struct synth_song *song,int p);
int synth_song_measure_frames(const struct synth_song *song); // => sum of delays only

/* Update signal for the given frame count -- (song->chanc) must be respected.
 * Adds to (v).
 * Advances time. Do not call with more than (song->delay).
 * Returns 0 if terminated.
 */
int synth_song_update(float *v,int framec,struct synth_song *song);

void synth_song_note(struct synth_song *song,uint8_t chid,uint8_t noteid,float velocity,int durframes);
void synth_song_wheel(struct synth_song *song,uint8_t chid,int v); // -512..0..511

/* Synth context.
 ******************************************************************************/
 
struct synth {
  int rate,chanc;
  float *qbuf;
  int qbufa;
  
  struct synth_res {
    int id;
    const void *v;
    int c;
    struct synth_pcm *pcm; // For sound only, and null until the first use. Printing may be in progress.
  } *resv;
  int resc,resa;
  
  struct synth_printer **printerv;
  int printerc,printera;
  int framec_in_progress; // New printers must print so much immediately. Also serves as a reentry detector.
  
  struct synth_pcmplay *pcmplayv;
  int pcmplayc,pcmplaya;
  
  struct synth_song *song;
  struct synth_song *pvsong;
};

int synth_frames_from_ms(const struct synth *synth,int ms);
void synth_apply_pan(float *triml,float *trimr,float trim,float pan);

#endif
