#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
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
};

#endif
