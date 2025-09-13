#include "egg/egg.h"
#include "graf.h"

/* Texture cache.
 */
 
int graf_tex(struct graf *graf,int imageid) {
  if (imageid<1) return 0;
  struct graf_tex *evictable=graf->texv;
  struct graf_tex *q=graf->texv;
  int i=GRAF_TEX_LIMIT;
  for (;i-->0;q++) {
    if (q->imageid==imageid) return q->texid;
    if (q->seq<evictable->seq) evictable=q;
  }
  struct graf_tex *tex;
  if (graf->texc<GRAF_TEX_LIMIT) {
    tex=graf->texv+graf->texc++;
    tex->texid=egg_texture_new();
  } else {
    tex=evictable;
    graf->texevictc++;
    if (graf->un.srctexid==tex->texid) graf_flush(graf); // <-- The reason tex cache is built into graf.
  }
  tex->imageid=imageid;
  egg_texture_load_image(tex->texid,imageid);
  tex->seq=graf->texseqnext++;
  return tex->texid;
}

/* Reset.
 */
 
void graf_reset(struct graf *graf) {
  graf->vtxc=0;
  graf->vtxsize=0;
  graf->un.mode=0;
  graf->un.dsttexid=1;
  graf->un.srctexid=0;
  graf->un.tint=0;
  graf->un.alpha=0xff;
  graf->un.filter=0;
}

/* Flush.
 */

void graf_flush(struct graf *graf) {
  if (graf->vtxc) {
    egg_render(&graf->un,graf->vtxv,graf->vtxc);
    graf->vtxc=0;
  }
}

/* Uniforms.
 */

void graf_set_output(struct graf *graf,int texid) {
  if (graf->un.dsttexid==texid) return;
  graf_flush(graf);
  graf->un.dsttexid=texid;
}

void graf_set_input(struct graf *graf,int texid) {
  if (graf->un.srctexid==texid) return;
  graf_flush(graf);
  graf->un.srctexid=texid;
}

void graf_set_image(struct graf *graf,int imageid) {
  graf_set_input(graf,graf_tex(graf,imageid));
}

void graf_set_tint(struct graf *graf,uint32_t rgba) {
  if (graf->un.tint==rgba) return;
  graf_flush(graf);
  graf->un.tint=rgba;
}

void graf_set_alpha(struct graf *graf,uint8_t alpha) {
  if (graf->un.alpha==alpha) return;
  graf_flush(graf);
  graf->un.alpha=alpha;
}

void graf_set_filter(struct graf *graf,uint8_t filter) {
  if (graf->un.filter==filter) return;
  graf_flush(graf);
  graf->un.filter=filter;
}

/* Add a vertex, and flush first if we're out of room.
 */
 
static void *graf_add_vertex(struct graf *graf) {
  if (graf->vtxc>sizeof(graf->vtxv)-graf->vtxsize) {
    graf_flush(graf);
  }
  void *vtx=graf->vtxv+graf->vtxc;
  graf->vtxc+=graf->vtxsize;
  return vtx;
}

/* No-texture primitives: point, line, line strip.
 */

void graf_point(struct graf *graf,int16_t x,int16_t y,uint32_t rgba) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_POINTS)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_POINTS;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_line(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba
) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_LINES)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_LINES;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=ax;
  vtx->y=ay;
  vtx->r=argba>>24;
  vtx->g=argba>>16;
  vtx->b=argba>>8;
  vtx->a=argba;
  vtx=graf_add_vertex(graf);
  vtx->x=bx;
  vtx->y=by;
  vtx->r=brgba>>24;
  vtx->g=brgba>>16;
  vtx->b=brgba>>8;
  vtx->a=brgba;
}

void graf_line_strip_begin(struct graf *graf,int16_t x,int16_t y,uint32_t rgba) {
  graf_flush(graf);
  graf->un.mode=EGG_RENDER_LINE_STRIP;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_line_strip_more(struct graf *graf,int16_t x,int16_t y,uint32_t rgba) {
  if (graf->un.mode!=EGG_RENDER_LINE_STRIP) return;
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

/* Triangles.
 */

void graf_triangle(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba,
  int16_t cx,int16_t cy,uint32_t crgba
) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_TRIANGLES)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_TRIANGLES;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=ax;
  vtx->y=ay;
  vtx->r=argba>>24;
  vtx->g=argba>>16;
  vtx->b=argba>>8;
  vtx->a=argba;
  vtx=graf_add_vertex(graf);
  vtx->x=bx;
  vtx->y=by;
  vtx->r=brgba>>24;
  vtx->g=brgba>>16;
  vtx->b=brgba>>8;
  vtx->a=brgba;
  vtx=graf_add_vertex(graf);
  vtx->x=cx;
  vtx->y=cy;
  vtx->r=crgba>>24;
  vtx->g=crgba>>16;
  vtx->b=crgba>>8;
  vtx->a=crgba;
}

void graf_triangle_tex(struct graf *graf,
  int16_t ax,int16_t ay,int16_t atx,int16_t aty,
  int16_t bx,int16_t by,int16_t btx,int16_t bty,
  int16_t cx,int16_t cy,int16_t ctx,int16_t cty
) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_TRIANGLES)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_TRIANGLES;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=ax;
  vtx->y=ay;
  vtx->tx=atx;
  vtx->ty=aty;
  vtx=graf_add_vertex(graf);
  vtx->x=bx;
  vtx->y=by;
  vtx->tx=btx;
  vtx->ty=bty;
  vtx=graf_add_vertex(graf);
  vtx->x=cx;
  vtx->y=cy;
  vtx->tx=ctx;
  vtx->ty=cty;
}

void graf_triangle_strip_begin(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba,
  int16_t cx,int16_t cy,uint32_t crgba
) {
  graf_flush(graf);
  graf->un.mode=EGG_RENDER_TRIANGLE_STRIP;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=ax;
  vtx->y=ay;
  vtx->r=argba>>24;
  vtx->g=argba>>16;
  vtx->b=argba>>8;
  vtx->a=argba;
  vtx=graf_add_vertex(graf);
  vtx->x=bx;
  vtx->y=by;
  vtx->r=brgba>>24;
  vtx->g=brgba>>16;
  vtx->b=brgba>>8;
  vtx->a=brgba;
  vtx=graf_add_vertex(graf);
  vtx->x=cx;
  vtx->y=cy;
  vtx->r=crgba>>24;
  vtx->g=crgba>>16;
  vtx->b=crgba>>8;
  vtx->a=crgba;
}

void graf_triangle_strip_more(struct graf *graf,int16_t x,int16_t y,uint32_t rgba) {
  if (graf->un.mode!=EGG_RENDER_TRIANGLE_STRIP) return;
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->r=rgba>>24;
  vtx->g=rgba>>16;
  vtx->b=rgba>>8;
  vtx->a=rgba;
}

void graf_triangle_strip_tex_begin(struct graf *graf,
  int16_t ax,int16_t ay,int16_t atx,int16_t aty,
  int16_t bx,int16_t by,int16_t btx,int16_t bty,
  int16_t cx,int16_t cy,int16_t ctx,int16_t cty
) {
  graf_flush(graf);
  graf->un.mode=EGG_RENDER_TRIANGLE_STRIP;
  graf->vtxsize=sizeof(struct egg_render_raw);
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=ax;
  vtx->y=ay;
  vtx->tx=atx;
  vtx->ty=aty;
  vtx=graf_add_vertex(graf);
  vtx->x=bx;
  vtx->y=by;
  vtx->tx=btx;
  vtx->ty=bty;
  vtx=graf_add_vertex(graf);
  vtx->x=cx;
  vtx->y=cy;
  vtx->tx=ctx;
  vtx->ty=cty;
}

void graf_triangle_strip_tex_more(struct graf *graf,int16_t x,int16_t y,int16_t tx,int16_t ty) {
  if (graf->un.mode!=EGG_RENDER_TRIANGLE_STRIP) return;
  struct egg_render_raw *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->tx=tx;
  vtx->ty=ty;
}

/* Point sprites.
 */

void graf_tile(struct graf *graf,int16_t x,int16_t y,uint8_t tileid,uint8_t xform) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_TILE)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_TILE;
  graf->vtxsize=sizeof(struct egg_render_tile);
  struct egg_render_tile *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->tileid=tileid;
  vtx->xform=xform;
}

void graf_fancy(struct graf *graf,
  int16_t x,int16_t y,uint8_t tileid,uint8_t xform, // Same as tile.
  uint8_t rotation, // 1/256 of a turn clockwise.
  uint8_t size, // Output size in pixels.
  uint32_t tint, // RGBA. A is the tinting amount.
  uint32_t primary // RGBA. A is the master alpha, and RGB is substituted for all pure-gray pixels.
) {
  if (graf->vtxc&&(graf->un.mode!=EGG_RENDER_FANCY)) graf_flush(graf);
  graf->un.mode=EGG_RENDER_FANCY;
  graf->vtxsize=sizeof(struct egg_render_fancy);
  struct egg_render_fancy *vtx=graf_add_vertex(graf);
  vtx->x=x;
  vtx->y=y;
  vtx->tileid=tileid;
  vtx->xform=xform;
  vtx->rotation=rotation;
  vtx->size=size;
  vtx->tr=tint>>24;
  vtx->tg=tint>>16;
  vtx->tb=tint>>8;
  vtx->ta=tint;
  vtx->pr=primary>>24;
  vtx->pg=primary>>16;
  vtx->pb=primary>>8;
  vtx->a=primary;
}
