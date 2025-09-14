#ifndef DEMO_H
#define DEMO_H

#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/font/font.h"
#include "opt/res/res.h"
#include "shared_symbols.h"
#include "egg_res_toc.h"
#include <limits.h>

#define FBW 320
#define FBH 180

extern struct g {
  void *rom;
  int romc;
  struct graf graf;
  struct font *font;
  int label_texid;
  int label_w,label_h;
} g;

#endif
