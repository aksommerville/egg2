#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include "synth_pcm.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#define SYNTH_RATE_MIN 200
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8

struct synth {
  int rate,chanc;
  
  struct synth_res {
    int qid; // rid + 0x10000 for song, 0 for sound
    const void *v;
    int c;
    struct synth_pcm *pcm; // Lazy, and only for sound.
  } *resv;
  int resc,resa;
};

#endif
