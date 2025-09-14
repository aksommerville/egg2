#include "font_internal.h"

/* Fetch an image resource and convert to y8.
 * Caller frees.
 * This is a rather complex operation.
 * There are no image decoders client-side, so we have to ask Egg to render the image, then read back its pixels RGBA.
 */
 
static uint8_t *font_acquire_image(int *w,int *h,int imageid) {

  // Have Egg load this image into a texture.
  int texid=egg_texture_new();
  if (texid<1) return 0;
  if (egg_texture_load_image(texid,imageid)<0) {
    egg_texture_del(texid);
    return 0;
  }
  egg_texture_get_size(w,h,texid);
  if ((*w<1)||(*h<1)) {
    egg_texture_del(texid);
    return 0;
  }
  
  // Read RGBA pixels from the texture. ("ABGR", we're going to read them wordwise)
  int len8=(*w)*(*h);
  int len32=len8*4;
  uint32_t *abgr=malloc(len32);
  if (!abgr) {
    egg_texture_del(texid);
    return 0;
  }
  if (egg_texture_get_pixels(abgr,len32,texid)<0) {
    egg_texture_del(texid);
    free(abgr);
    return 0;
  }
  egg_texture_del(texid);
  
  // Convert to y8. Anything with at least one of the high chroma bits set is 0xff, anything else is 0x00.
  uint8_t *bits=malloc(len8);
  if (!bits) {
    free(abgr);
    return 0;
  }
  const uint32_t *src=abgr;
  uint8_t *dst=bits;
  int i=len8;
  for (;i-->0;dst++,src++) {
    if ((*src)&0x00808080) *dst=0xff;
    else *dst=0x00;
  }
  
  free(abgr);
  return bits;
}

/* Add glyph to page.
 */
 
static int font_page_add_glyph(struct font_page *page,int x,int y,int w) {
  if (w>255) return -1;
  if (page->glyphc>=page->glypha) {
    int na=page->glypha+32;
    if (na>INT_MAX/sizeof(struct font_glyph)) return -1;
    void *nv=realloc(page->glyphv,sizeof(struct font_glyph)*na);
    if (!nv) return -1;
    page->glyphv=nv;
    page->glypha=na;
  }
  struct font_glyph *glyph=page->glyphv+page->glyphc++;
  glyph->x=x;
  glyph->y=y;
  glyph->w=w;
  return 0;
}

/* Test for blank glyphs.
 */
 
static int font_glyph_is_empty(const struct font_page *page,int x,int y,int w,int h) {
  const uint8_t *row=page->bits+y*page->w+x;
  int yi=h;
  for (;yi-->0;row+=page->w) {
    const uint8_t *p=row;
    int xi=w;
    for (;xi-->0;p++) if (*p) return 0;
  }
  return 1;
}

/* Given a page with no glyphs and a ready image, locate and record all the glyphs.
 * Caller must validate the left control bar first, and tell us line height.
 * We won't assert the left control bar again.
 * Fails if no glyphs are detected.
 */
 
static int font_page_acquire_glyphs(struct font_page *page,int lineh) {
  int y=0;
  int ystride=lineh+1;
  const uint8_t *ctlrow=page->bits;
  int ctlrowstride=page->w*ystride;
  for (;y<page->h;y+=ystride,ctlrow+=ctlrowstride) {
    int x=1;
    while (x<page->w) {
      int w=1;
      while ((x+w<page->w)&&(ctlrow[x]==ctlrow[x+w])) w++;
      if ((x+w>=page->w)&&font_glyph_is_empty(page,x,y+1,w,lineh)) {
        // Skip blank glyphs at the end of the line.
      } else {
        if (font_page_add_glyph(page,x,y+1,w)<0) return -1;
      }
      x+=w;
    }
  }
  return 0;
}

/* Add image.
 */

const char *font_add_image(struct font *font,int imageid,int codepoint) {
  if (!font||(imageid<1)||(codepoint<0)) return "Invalid request.";
  
  uint8_t *bits=0;
  int w=0,h=0;
  if (!(bits=font_acquire_image(&w,&h,imageid))) return "Failed to acquire image.";
  if ((w>256)||(h>256)) { // Enforced here instead of font_acquire_image(), in order to provide better messaging.
    free(bits);
    return "Font page size exceeds 256x256.";
  }
  
  // Validate the left control bar. This yields count and height of lines, and must agree with (font) if we're not the first page.
  int lineh=0,linec=0,y=0;
  const uint8_t *p=bits;
  while (y<h) {
    if (*p) {
      free(bits);
      return "First pixel must be black.";
    }
    y++;
    p+=w;
    int lineh1=0;
    while ((y<h)&&*p) {
      y++;
      p+=w;
      lineh1++;
    }
    if (!linec) lineh=lineh1;
    else if (lineh!=lineh1) {
      free(bits);
      return "Mismatched line heights within page.";
    }
    linec++;
  }
  if (linec<1) { free(bits); return "Empty image."; }
  if ((lineh<1)||(lineh>255)) { free(bits); return "Invalid line height."; }
  if (font->lineh&&(font->lineh!=lineh)) { free(bits); return "Mismatched line heights between pages."; }
  
  // Insert the page. Beyond this, we'll have to remove it on errors.
  int pagep=font_pagev_search(font,codepoint);
  if (pagep>=0) { free(bits); return "Multiple glyphs for initial codepoint."; }
  pagep=-pagep-1;
  struct font_page *page=font_pagev_insert(font,pagep,codepoint);
  if (!page) {
    free(bits);
    return "Failed to allocate page.";
  }
  page->bits=bits; // HANDOFF
  page->w=w;
  page->h=h;
  
  // Acquire glyphs.
  if (font_page_acquire_glyphs(page,lineh)<0) {
    font_page_cleanup(page);
    font->pagec--;
    memmove(page,page+1,sizeof(struct font_page)*(font->pagec-pagep));
    return "Error breaking image into glyphs.";
  }
  
  // Confirm we didn't overflow into the next page.
  int neighborp=pagep+1;
  if (neighborp<font->pagec) {
    const struct font_page *neighbor=font->pagev+neighborp;
    if (page->codepoint+page->glyphc>neighbor->codepoint) {
      font_page_cleanup(page);
      font->pagec--;
      memmove(page,page+1,sizeof(struct font_page)*(font->pagec-pagep));
      return "Overlapping pages.";
    }
  }
  
  font->lineh=lineh;
  return 0;
}
