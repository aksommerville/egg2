#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include "synth_pcm.h"
#include "eau.h"
#include "synth_channel.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

#define SYNTH_RATE_MIN 200
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8
#define SYNTH_QBUF_SIZE_FRAMES 1024
#define SYNTH_UPDATE_LIMIT_FRAMES 1024
#define SYNTH_PCMPLAY_LIMIT 64 /* arbitrary */
#define SYNTH_CHANNEL_LIMIT 32 /* Must accomodate at least 2 songs of 16 each. */

struct synth {
  int rate,chanc;
  int64_t framec_total; // How long we've been running.
  float *qbuf;
  int qbufa;
  struct synth_wave *sine; // lazy
  int framec_in_progress;
  
  struct synth_res {
    int qid; // rid + 0x10000 for song, 0 for sound
    const void *v;
    int c;
    struct synth_pcm *pcm; // Lazy, and only for sound.
  } *resv;
  int resc,resa;
  
  int songid;
  int songrepeat;
  const uint8_t *song; // Events only.
  int songc;
  int songloopp;
  int songp;
  int songdelay; // Frames.
  
  struct synth_channel *channelv[SYNTH_CHANNEL_LIMIT];
  int channelc;
  
  struct synth_pcmplay pcmplayv[SYNTH_PCMPLAY_LIMIT];
  int pcmplayc;
  
  struct synth_printer **printerv;
  int printerc,printera;
};

//TODO
static inline void synth_end_song(struct synth *synth) {}
static inline int synth_prepare_song_channels(struct synth *synth,const struct eau_file *file) { return -1; }
static inline void synth_update_internal(float *v,int framec,struct synth *synth) {}

#endif
