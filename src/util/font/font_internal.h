#ifndef FONT_INTERNAL_H
#define FONT_INTERNAL_H

#include "egg/egg.h"
#include "util/stdlib/egg-stdlib.h"
#include "font.h"

struct font {
  int lineh; // Zero only if no pages have been installed.
  struct font_page {
    // We store images 8-bit instead of 1-bit to make slicing and traversal more convenient.
    uint8_t *bits; // y8; bytes should only be 0x00 or 0xff. Stride is (w).
    int w,h; // Size of (bits), the whole image with control bars and all.
    int codepoint; // Of first glyph; the rest proceed monotonically.
    struct font_glyph {
      uint8_t x,y,w; // No (h); that's (font->lineh).
    } *glyphv;
    int glyphc,glypha;
  } *pagev;
  int pagec,pagea;
};

void font_page_cleanup(struct font_page *page);

int font_pagev_search(const struct font *font,int codepoint);
struct font_page *font_pagev_insert(struct font *font,int p,int codepoint);

#endif
