#include "font_internal.h"

/* Measure line up to a provided limit.
 * Returns count of bytes.
 * Stops at LF.
 * Permits whitespace to breach (wlimit).
 * Also may breach (wlimit) if there just isn't enough room.
 */
 
static int font_measure_line(const struct font *font,const char *src,int srcc,int wlimit) {
  struct font_string_reader reader;
  font_string_reader_init(&reader,src,srcc);
  int w=0,codepoint;
  int breakp=0; // The most recent spot we've identified as breakable.
  while (font_string_reader_next(&codepoint,&reader)>0) {
    int glyphw=font_get_glyph_width(font,codepoint);
    w+=glyphw;
    
    // If it's whitespace, we add it no matter what.
    // And furthermore, if it's LF, add it and we're done.
    // Reader's current position becomes the future breaking point.
    if (codepoint<=0x20) {
      if (codepoint==0x0a) break;
      breakp=reader.p;
      continue;
    }
    
    // If we're over the limit, use our recorded break point.
    // If that break point is zero, use the reader's position, ie emit one glyph.
    if ((w>wlimit)&&breakp) {
      if (breakp) return breakp;
      return reader.p;
    }
    
    // Allow breaks after any punctuation (whitespace is already covered).
    // Assume that anything outside G0 is letters, despite a lot of it actually being punctuation.
         if (codepoint>=0x80) ;
    else if ((codepoint>='a')&&(codepoint<='z')) ;
    else if ((codepoint>='A')&&(codepoint<='Z')) ;
    else if ((codepoint>='0')&&(codepoint<='9')) ;
    else if (codepoint=='_') ;
    else if (codepoint=='\'') ;
    else breakp=reader.p;
  }
  return reader.p;
}

/* Break lines, public entry point.
 */
 
int font_break_lines(int *startv,int starta,const struct font *font,const char *src,int srcc,int wlimit) {
  if (!font||(starta<1)) return 0;
  startv[0]=0;
  if (starta==1) return 1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  int startc=1,srcp=0;
  while ((startc<starta)&&(srcp<srcc)) {
    int c=font_measure_line(font,src+srcp,srcc-srcp,wlimit);
    if (c<1) break;
    srcp+=c;
    if (srcp>=srcc) break;
    startv[startc++]=srcp;
  }
  return startc;
}
