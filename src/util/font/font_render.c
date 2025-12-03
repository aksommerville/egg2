#include "font_internal.h"

/* Lookup and render one glyph.
 * Returns horizontal advancement in pixels.
 */
 
static int font_render_glyph_internal(
  void *dstp,int dstw,int dsth,int dststride_words,
  const struct font *font,
  int codepoint,
  uint32_t pixel
) {
  int pagep=font_pagev_search(font,codepoint);
  if (pagep<0) return 0;
  const struct font_page *page=font->pagev+pagep;
  codepoint-=page->codepoint;
  if ((codepoint<0)||(codepoint>=page->glyphc)) return 0; // Shouldn't be possible.
  const struct font_glyph *glyph=page->glyphv+codepoint;
  if (!glyph->w) return 0;
  int cpw=glyph->w;
  if (cpw>dstw) cpw=dstw;
  uint32_t *dstrow=dstp;
  const uint8_t *srcrow=page->bits+glyph->y*page->w+glyph->x;
  int yi=font->lineh;
  if (yi>dsth) yi=dsth;
  for (;yi-->0;dstrow+=dststride_words,srcrow+=page->w) {
    uint32_t *dstpx=dstrow;
    const uint8_t *srcpx=srcrow;
    int xi=cpw;
    for (;xi-->0;dstpx++,srcpx++) {
      if (*srcpx) *dstpx=pixel;
    }
  }
  return glyph->w;
}

/* Render single line to software framebuffer.
 */

int font_render(
  void *dstp,int dstw,int dsth,int dststride,
  const struct font *font,
  const char *src,int srcc,
  uint32_t rgba
) {
  if (!dstp||!font) return 0;
  if (dststride&3) return 0;
  dststride>>=2; // We want the stride in 32-bit words, not bytes.
  int result=0;
  uint32_t pixel=(rgba>>24)|((rgba&0xff0000)>>8)|((rgba&0xff00)<<8)|(rgba<<24);
  struct font_string_reader reader;
  font_string_reader_init(&reader,src,srcc);
  int codepoint;
  while (font_string_reader_next(&codepoint,&reader)>0) {
    int advance=font_render_glyph_internal(dstp,dstw,dsth,dststride,font,codepoint,pixel);
    result+=advance;
    if ((dstw-=advance)<=0) break;
    dstp=((uint32_t*)dstp)+advance;
  }
  return result;
}

/* Any nonzero pixels in this column?
 */
 
static int font_column_is_blank(const uint32_t *p,int stride_words,int h) {
  for (;h-->0;p+=stride_words) if (*p) return 0;
  return 1;
}

/* Render to new texture.
 */

int font_render_to_texture(
  int texid,
  const struct font *font,
  const char *src,int srcc,
  int wlimit,int hlimit,
  uint32_t color
) {

  // Some crude validation first.
  if (!font) return -1;
  if (font->lineh<1) return -1; // No pages yet? Obviously we can't produce anything.
  if ((wlimit<1)||(hlimit<1)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  // Break lines and decide how many are going to render.
  #define LINE_LIMIT 64 /* I don't imagine one would ever need more than this (or even close to it). */
  int startv[LINE_LIMIT];
  int linec=font_break_lines(startv,LINE_LIMIT,font,src,srcc,wlimit);
  #undef LINE_LIMIT
  int vislinec=(hlimit+font->lineh-1)/font->lineh;
  if (linec>vislinec) linec=vislinec;
  if (linec<1) return -1;
  
  // Allocate the RGBA buffer generously; we will trim its right edge after rendering.
  int w=wlimit;
  int h=linec*font->lineh;
  if (h>hlimit) h=hlimit;
  int stride=w<<2;
  uint32_t *rgba=calloc(stride,h);
  if (!rgba) return -1;
  
  // Render each line.
  int startp=0,y=0;
  for (;startp<linec;startp++,y+=font->lineh) {
    int len=srcc-startv[startp];
    if (startp<linec-1) len=startv[startp+1]-startv[startp];
    font_render(rgba+y*w,w,h-y,stride,font,src+startv[startp],len,color);
  }
  
  // Trim empty columns from the right edge.
  // Do not trim the bottom, top, or left.
  while (w&&font_column_is_blank(rgba+w-1,stride>>2,h)) w--;
  if (!w) {
    free(rgba);
    return -1;
  }
  
  // Upload to a new texture.
  int mytexture=0;
  if (texid<1) {
    if ((texid=egg_texture_new())<1) {
      free(rgba);
      return -1;
    }
    mytexture=1;
  }
  if (egg_texture_load_raw(texid,w,h,stride,rgba,stride*h)<0) {
    free(rgba);
    if (mytexture) egg_texture_del(texid);
    return -1;
  }
  free(rgba);
  return texid;
}
