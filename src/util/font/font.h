/* font.h
 * Utilities for generating text labels in Egg, client-side.
 * Requires stdlib.
 * All entry points are null-safe.
 *
 * You provide source images containing the glyphs.
 *  - Same format as egg v1 (though this API is a brand new implementation).
 *  - Black and white.
 *  - Leftmost column is a control region: Black indicates a row start and white is the content of the row.
 *  - All rows across one font must be the same height.
 *  - - Note that the left control bar is largely redundant. We lean on that as a validation mechanism.
 *  - In the control row, alternate black and white to indicate the width of the glyph below. First glyph has a white control row.
 *  - Each of these glyphs can contain any count of glyphs (I typically cut at 16). If the final region in a glyph row has no set pixels, it's ignored.
 *  - Glyphs should contain all of their horizontal space. Typically one column on the right.
 *  - Limit 255 pixels per glyph on both axes.
 *  - Page dimensions limited to 256x256.
 *
 * All text is left-to-right. We can't do Arabic or Hebrew, sorry.
 * Also no ligatures, kerning, or other exceptional spacing arrangements: One codepoint is one glyph, and it always looks the same.
 * Glyphs not explicitly defined in your images are noop (in particular, no horizontal advancement).
 * Text is UTF-8. If misencoded, we treat it as 8859-1, one byte at a time.
 */
 
#ifndef FONT_H
#define FONT_H

#include <stdint.h>

struct font;

void font_del(struct font *font);

struct font *font_new();

/* See above for formatting of images.
 * If anything goes wrong, we return a canned error message in English.
 * Null on success.
 */
const char *font_add_image(struct font *font,int imageid,int codepoint);

int font_get_line_height(const struct font *font);
int font_get_glyph_width(const struct font *font,int codepoint);
int font_measure_string(const struct font *font,const char *src,int srcc);

/* Fills (startv) with the position in (src) of lines after breaking against (wlimit).
 * Returns the count of lines, never more than (starta).
 * Lines may breach (wlimit), especially the last one if there's inadequate space in (startv).
 */
int font_break_lines(int *startv,int starta,const struct font *font,const char *src,int srcc,int wlimit);

/* Draw one line of text on top of a software RGBA buffer.
 * Caller should move (dstp) on your own to effect the offset.
 * No line breaks. LF et al are treated like ordinary glyphs.
 * Returns total horizontal advancement.
 * This is used internally. From the application, you probably want font_render_to_texture().
 */
int font_render(
  void *dstp,int dstw,int dsth,int dststride,
  const struct font *font,
  const char *src,int srcc,
  uint32_t rgba
);

/* Equivalent to font_render, but you may supply a full RGBA image and we manage the cropping.
 * We can't crop left or top.
 * Provided only for compatibility with Egg v1's font unit.
 */
int font_render_string(
  void *dst,int dstw,int dsth,int dststride,
  int x,int y,
  const struct font *font,
  const char *src,int srcc,
  uint32_t rgba
);

/* Rewrite (texid) with the given text after breaking lines.
 * Height is always a multiple of the font's line height.
 * Width is trimmed such that the right column is never blank.
 * Text may be cropped at right or bottom edges. We will never produce something larger than the provided limits.
 * Never produces empty images. If the input is empty or all space, we clean up and fail instead.
 * You provide a foreground color; the background is transparent.
 * If (texid) zero, we allocate a new one.
 * Returns (texid).
 */
int font_render_to_texture(
  int texid,
  const struct font *font,
  const char *src,int srcc,
  int wlimit,int hlimit,
  uint32_t rgba
);

/* A little overkilly, but to ensure we manage defaulting of misencoding text uniformly,
 * we will always read text via this reader.
 */
struct font_string_reader {
  const uint8_t *v;
  int c,p;
};
void font_string_reader_init(struct font_string_reader *reader,const void *v,int c);
int font_string_reader_next(int *codepoint,struct font_string_reader *reader);

#endif
