#ifndef EGGDEV_INTERNAL_H
#define EGGDEV_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "egg/egg.h"
#include "convert/eggdev_convert.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

extern struct g {
  const char *exename;
} g;

//TODO This is a tricky one. It needs to access the project's resource TOC, if there is one.
static inline int eggdev_tid_eval(const char *src,int srcc) { return -1; }
static inline int eggdev_tid_repr(char *dst,int dsta,int tid) { return -1; }

#endif
