#include "../demo.h"
#include "gui.h"

/* Cells must be square, but for most purposes we'll pretend they can be oblong.
 * This cell size is intrinsic to image:fonttiles.
 */
#define COLW 8
#define ROWH 8

struct gui_term {
  int x,y,w,h;
  int colc,rowc;
  int x0,y0; // Absolute center of top-left tile, including left and top padding.
  uint32_t fgtint,bg;
  uint8_t fgalpha;
  struct egg_render_tile *vtxv;
  int vtxc;
  char *text;
  int dirty;
};

/* Delete.
 */
 
void gui_term_del(struct gui_term *term) {
  if (!term) return;
  if (term->vtxv) free(term->vtxv);
  if (term->text) free(term->text);
  free(term);
}

/* New.
 */

struct gui_term *gui_term_new(int x,int y,int w,int h) {
  if ((w<COLW)||(h<ROWH)) return 0;
  if ((w>0xffff)||(h>0xffff)) return 0; // oh come on!
  struct gui_term *term=calloc(1,sizeof(struct gui_term));
  if (!term) return 0;
  term->x=x;
  term->y=y;
  term->w=w;
  term->h=h;
  term->colc=w/COLW;
  term->rowc=h/ROWH;
  term->fgtint=0xffffffff;
  term->fgalpha=0xff;
  term->bg=0x000000ff;
  term->x0=term->x+(((term->w%COLW)+1)>>1)+(COLW>>1);
  term->y0=term->y+(((term->h%ROWH)+1)>>1)+(ROWH>>1);
  int cellc=term->colc*term->rowc;
  if (!(term->vtxv=malloc(sizeof(struct egg_render_tile)*cellc))) return 0;
  if (!(term->text=calloc(1,cellc))) return 0;
  return term;
}

/* Trivial accessors.
 */

void gui_term_get_size(int *colc,int *rowc,const struct gui_term *term) {
  if (!term) { *colc=*rowc=0; return; }
  *colc=term->colc;
  *rowc=term->rowc;
}

void gui_term_set_background(struct gui_term *term,uint32_t rgba) {
  if (!term) return;
  if (rgba&0xff) term->bg=rgba;
  else term->bg=0;
}

void gui_term_set_foreground(struct gui_term *term,uint32_t rgba) {
  if (!term) return;
  term->fgtint=rgba|0xff;
  term->fgalpha=rgba;
}

void gui_term_get_bounds(int *x,int *y,int *w,int *h,const struct gui_term *term,int col,int row,int colc,int rowc) {
  if (!term) { *x=*y=*w=*h=0; return; }
  if (col<0) { colc+=col; col=0; }
  if (row<0) { rowc+=row; row=0; }
  if (col>term->colc-colc) colc=term->colc-col;
  if (row>term->rowc-rowc) rowc=term->rowc-row;
  if (colc<0) colc=0;
  if (rowc<0) rowc=0;
  *x=term->x0-(COLW>>1)+col*COLW;
  *y=term->y0-(ROWH>>1)+row*ROWH;
  *w=colc*COLW;
  *h=rowc*ROWH;
}

/* Clear.
 */

void gui_term_clear(struct gui_term *term) {
  if (!term) return;
  memset(term->text,0,term->colc*term->rowc);
  term->dirty=1;
}

/* Write string.
 */
 
void gui_term_write(struct gui_term *term,int x,int y,const char *src,int srcc) {
  if (!term) return;
  if ((y<0)||(y>=term->rowc)) return;
  if (!src) return;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (x<0) {
    src-=x;
    srcc+=x;
    x=0;
  }
  if (x>term->colc-srcc) srcc=term->colc-x;
  if (srcc<1) return;
  char *dst=term->text+y*term->colc+x;
  memcpy(dst,src,srcc);
  term->dirty=1;
}

/* Write formatted string.
 */
 
void gui_term_writef(struct gui_term *term,int x,int y,const char *fmt,...) {
  if (!term||!fmt) return;
  char tmp[256];
  va_list vargs;
  va_start(vargs,fmt);
  int tmpc=vsnprintf(tmp,sizeof(tmp),fmt,vargs);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return;
  gui_term_write(term,x,y,tmp,tmpc);
}

/* Update.
 */

void gui_term_update(struct gui_term *term,double elapsed) {
  // Currently noop, but maybe in the future we'll want animation?
}

/* Rebuild (vtxv) from (text).
 */
 
static void gui_term_rebuild_vtxv(struct gui_term *term) {
  term->vtxc=0;
  struct egg_render_tile *vtx=term->vtxv;
  const char *src=term->text;
  int yi=term->rowc;
  int y=term->y0;
  for (;yi-->0;y+=ROWH) {
    int xi=term->colc;
    int x=term->x0;
    for (;xi-->0;x+=COLW,src++) {
      if ((*src<=0x20)||(*src>=0x7f)) continue;
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=*src;
      vtx->xform=0;
      vtx++;
      term->vtxc++;
    }
  }
}

/* Render.
 */
 
void gui_term_render(struct gui_term *term) {
  if (!term) return;
  if (term->dirty) {
    term->dirty=0;
    gui_term_rebuild_vtxv(term);
  }
  if (term->bg) graf_fill_rect(&g.graf,term->x,term->y,term->w,term->h,term->bg);
  if (!term->vtxc) return;
  graf_flush(&g.graf);
  struct egg_render_uniform un={
    .mode=EGG_RENDER_TILE,
    .dsttexid=1,
    .srctexid=g.texid_fonttiles,
    .tint=term->fgtint,
    .alpha=term->fgalpha,
  };
  egg_render(&un,term->vtxv,sizeof(struct egg_render_tile)*term->vtxc);
}
