#include "../demo.h"
#include "gui.h"

/* Single line of text from 8x8 tiles.
 */

int gui_render_string(int x,int y,const char *src,int srcc,uint32_t rgba) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  int x0=x;
  x+=4;
  y+=4;
  graf_set_input(&g.graf,g.texid_fonttiles);
  graf_set_tint(&g.graf,rgba);
  for (;srcc-->0;src++,x+=8) {
    uint8_t ch=*src;
    if (ch<=0x20) continue;
    if (ch>=0x7f) ch='?';
    graf_tile(&g.graf,x,y,ch,0);
  }
  x-=4;
  return x-x0;
}
