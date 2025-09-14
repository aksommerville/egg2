/* graf.h
 * Client-side rendering helper for Egg.
 */
 
#ifndef GRAF_H
#define GRAF_H

#define GRAF_VERTEX_BUFFER_SIZE 4096 /* bytes */
#define GRAF_TEX_LIMIT 8 /* textures */

struct graf {
  struct egg_render_uniform un;
  uint8_t vtxv[GRAF_VERTEX_BUFFER_SIZE];
  int vtxc; // bytes, not vertices
  int vtxsize; // Size of one vertex in bytes, derived from (un.mode).
  
  // Texture cache.
  struct graf_tex {
    int texid,imageid,seq;
  } texv[GRAF_TEX_LIMIT];
  int texc;
  int texseqnext;
  
  /* Incremented every time we evict a texture from the cache.
   * It's wise to monitor this during development.
   * If you see evictions every frame, you should increase the cache size or reduce your scenes' complexity.
   */
  int texevictc;
};

/* Load an image resource into a texture and return that texture's ID.
 * This is mostly independent of graf's main responsibilities,
 * but there can be cases where graphics are using an image ready to evict, and need to be flushed first.
 */
int graf_tex(struct graf *graf,int imageid);

/* Drop any content we haven't drawn yet, and return to the default state.
 * You'll want to do this at the start of each frame, and probably nowhere else.
 */
void graf_reset(struct graf *graf);

/* Force out any content we haven't drawn yet.
 * Aside from pending vertices, all state is preserved.
 * You may need to insert these manually if you're uploading textures, or making egg_render() calls behind our back.
 * You must flush at the end of each frame.
 */
void graf_flush(struct graf *graf);

/* Modify uniforms.
 * These will automatically flush first if necessary.
 * Set all uniforms before calling any vertex ops.
 */
void graf_set_output(struct graf *graf,int texid); // Default 1 ie main output.
void graf_set_input(struct graf *graf,int texid); // Mandatory for TILE and FANCY, optional for other modes.
void graf_set_image(struct graf *graf,int imageid); // Convenience; use an image as input.
void graf_set_tint(struct graf *graf,uint32_t rgba); // Alpha is the amount of tinting, zero alpha is noop.
void graf_set_alpha(struct graf *graf,uint8_t alpha);
void graf_set_filter(struct graf *graf,uint8_t filter); // 0=nearest-neighbor, 1=linear.

/* Queue a 1-pixel point for render.
 * Egg does not have a settable point size, it's always 1.
 */
void graf_point(struct graf *graf,int16_t x,int16_t y,uint32_t rgba);

/* Queue a skinny line for render.
 * Width is always 1.
 */
void graf_line(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba
);

/* Queue a set of connected lines.
 * You must "more" at least once.
 * WARNING: If we run out of buffer during a line or triangle strip batch, some units will be lost.
 */
void graf_line_strip_begin(struct graf *graf,int16_t x,int16_t y,uint32_t rgba);
void graf_line_strip_more(struct graf *graf,int16_t x,int16_t y,uint32_t rgba);

/* Queue a single triangle for render, either with colors or tex coords.
 */
void graf_triangle(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba,
  int16_t cx,int16_t cy,uint32_t crgba
);
void graf_triangle_tex(struct graf *graf,
  int16_t ax,int16_t ay,int16_t atx,int16_t aty,
  int16_t bx,int16_t by,int16_t btx,int16_t bty,
  int16_t cx,int16_t cy,int16_t ctx,int16_t cty
);

/* Queue a set of connected triangles.
 * The "begin" call takes three points, so it starts with a valid set.
 * Each "more" adds one more triangle, using the last two points we saw, plus the new one.
 */
void graf_triangle_strip_begin(struct graf *graf,
  int16_t ax,int16_t ay,uint32_t argba,
  int16_t bx,int16_t by,uint32_t brgba,
  int16_t cx,int16_t cy,uint32_t crgba
);
void graf_triangle_strip_more(struct graf *graf,int16_t x,int16_t y,uint32_t rgba);

/* Connected triangles, with tex coords instead of colors.
 */
void graf_triangle_strip_tex_begin(struct graf *graf,
  int16_t ax,int16_t ay,int16_t atx,int16_t aty,
  int16_t bx,int16_t by,int16_t btx,int16_t bty,
  int16_t cx,int16_t cy,int16_t ctx,int16_t cty
);
void graf_triangle_strip_tex_more(struct graf *graf,int16_t x,int16_t y,int16_t tx,int16_t ty);

/* Convenience to do a complete triangle-strip batch with one textured quad.
 */
void graf_decal(struct graf *graf,int dstx,int dsty,int srcx,int srcy,int w,int h);

/* Queue a tile for render.
 * This is the performance workhorse of Egg Render.
 * Try to batch tiles by their source image so we can deliver large batches at once.
 * Internally, these are implemented as point sprites.
 */
void graf_tile(struct graf *graf,int16_t x,int16_t y,uint8_t tileid,uint8_t xform);

/* Queue a tile for render, with bells and whistles.
 * These are point sprites like "tiles" but with a much larger vertex size and lots of fun options.
 */
void graf_fancy(struct graf *graf,
  int16_t x,int16_t y,uint8_t tileid,uint8_t xform, // Same as tile.
  uint8_t rotation, // 1/256 of a turn clockwise.
  uint8_t size, // Output size in pixels.
  uint32_t tint, // RGBA. A is the tinting amount.
  uint32_t primary // RGBA. A is the master alpha, and RGB is substituted for all pure-gray pixels.
);

#endif
