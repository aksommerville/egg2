#ifndef DEMO_H
#define DEMO_H

#include "egg/egg.h"
#include "util/stdlib/egg-stdlib.h"
#include "util/graf/graf.h"
#include "util/font/font.h"
#include "util/res/res.h"
#include "shared_symbols.h"
#include "egg_res_toc.h"
#include <limits.h>
#include "modal.h"

#define FBW 320
#define FBH 180

extern struct g {
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  struct graf graf;
  struct font *font;
  int pvinput;
  int texid_fonttiles; // 8x8 tiles
  
  // Only the top modal renders or receives input. The stack is never empty.
  struct modal **modalv;
  int modalc,modala;
} g;

int demo_get_res(void *dstpp/*BORROW*/,int tid,int rid);
int demo_resv_search(int tid,int rid);

#endif
