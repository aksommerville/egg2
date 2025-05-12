#ifndef EGG_GAME_MAIN_H
#define EGG_GAME_MAIN_H

#include "egg/egg.h"
#include "opt/stdlib/egg-stdlib.h"
#include "opt/graf/graf.h"
#include "opt/res/res.h"
#include "shared_symbols.h"
#include "egg_res_toc.h"
#include <limits.h>

//XXX Temporarily ridiculously small to test client-side software framebuffer.
//#define FBW 320
//#define FBH 180
#define FBW 32
#define FBH 32

extern struct g {
  void *rom;
  int romc;
  struct graf graf;
  int texid_tiles;
} g;

#endif
