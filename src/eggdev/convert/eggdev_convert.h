/* eggdev_convert.h
 * General data conversion.
 */
 
#ifndef EGGDEV_CONVERT_H
#define EGGDEV_CONVERT_H

#define EGGDEV_FMT_egg         1
#define EGGDEV_FMT_exe         2
#define EGGDEV_FMT_zip         3
#define EGGDEV_FMT_html        4
#define EGGDEV_FMT_css         5
#define EGGDEV_FMT_js          6
#define EGGDEV_FMT_png         7
#define EGGDEV_FMT_gif         8
#define EGGDEV_FMT_jpeg        9
#define EGGDEV_FMT_wav        10
#define EGGDEV_FMT_mid        11
#define EGGDEV_FMT_eau        12
#define EGGDEV_FMT_eaut       13
#define EGGDEV_FMT_wasm       14
#define EGGDEV_FMT_metadata   15
#define EGGDEV_FMT_metatxt    16
#define EGGDEV_FMT_strings    17
#define EGGDEV_FMT_strtxt     18
#define EGGDEV_FMT_tilesheet  19
#define EGGDEV_FMT_tstxt      20
#define EGGDEV_FMT_decalsheet 21
#define EGGDEV_FMT_dstxt      22
#define EGGDEV_FMT_map        23
#define EGGDEV_FMT_maptxt     24
#define EGGDEV_FMT_sprite     25
#define EGGDEV_FMT_sprtxt     26
#define EGGDEV_FMT_cmdlist    27
#define EGGDEV_FMT_cmdltxt    28
#define EGGDEV_FMT_ico        29
#define EGGDEV_FMT_FOR_EACH \
  _(egg) _(exe) _(zip) _(html) _(css) _(js) \
  _(png) _(gif) _(jpeg) _(wav) _(mid) _(eau) _(eaut) \
  _(wasm) _(metadata) _(metatxt) _(strings) _(strtxt) \
  _(tilesheet) _(tstxt) _(decalsheet) _(dstxt) \
  _(map) _(maptxt) _(sprite) _(sprtxt) _(cmdlist) _(cmdltxt) \
  _(ico)

/* Format properties and detection.
 */
int eggdev_fmt_repr(char *dst,int dsta,int fmt);
int eggdev_fmt_eval(const char *src,int srcc); // The symbols above, normalized, plus aliases, or integers.
int eggdev_fmt_by_path(const char *path,int pathc); // Usually from the extension. Also groks resource directories.
int eggdev_fmt_by_tid(int tid); // Preferred format for resources of a given standard type.
int eggdev_fmt_portable(int fmt); // Preferred format to convert to when extracting from ROM.
int eggdev_fmt_by_signature(const void *src,int srcc);
int eggdev_tid_by_path_or_fmt(const char *path,int pathc,int fmt);
const char *eggdev_mime_type_by_fmt(int fmt);
const char *eggdev_guess_mime_type(const void *src,int srcc,const char *path,int fmt);

/* Primitive converters.
 */
struct eggdev_convert_context {
  struct sr_encoder *dst; // REQUIRED
  const void *src; int srcc; // REQUIRED
  const char *ns; int nsc; // "CMD" namespace identifier.
  const char *refname;
  int lineno0;
  struct sr_encoder *errmsg; // OPTIONAL. If present, errors log here instead of stderr, and even if no (refname).
};
int eggdev_convert_noop(struct eggdev_convert_context *ctx); // Just copy (src) to (dst), no conversion.
int eggdev_egg_from_exe(struct eggdev_convert_context *ctx); // ie extract ROM.
int eggdev_egg_from_zip(struct eggdev_convert_context *ctx); // Zip must contain "game.bin" at its root.
int eggdev_egg_from_html(struct eggdev_convert_context *ctx); // Standalone HTML only.
int eggdev_zip_from_egg(struct eggdev_convert_context *ctx);
int eggdev_zip_from_html(struct eggdev_convert_context *ctx); // From Standalone HTML. Uses SDK's Separate HTML template.
int eggdev_zip_from_exe(struct eggdev_convert_context *ctx); // Fails if code:1 absent, which will usually be the case.
int eggdev_html_from_egg(struct eggdev_convert_context *ctx);
int eggdev_html_from_zip(struct eggdev_convert_context *ctx); // Uses SDK's Standalone HTML template.
int eggdev_html_from_exe(struct eggdev_convert_context *ctx); // Fails if code:1 absent, which will usually be the case.
int eggdev_wav_from_eau(struct eggdev_convert_context *ctx); // Stands a synthesizer and records it.
int eggdev_wav_from_eaut(struct eggdev_convert_context *ctx);
int eggdev_wav_from_mid(struct eggdev_convert_context *ctx);
int eggdev_eau_from_eaut(struct eggdev_convert_context *ctx);
int eggdev_eau_from_mid(struct eggdev_convert_context *ctx);
int eggdev_eaut_from_eau(struct eggdev_convert_context *ctx);
int eggdev_eaut_from_mid(struct eggdev_convert_context *ctx);
int eggdev_mid_from_eau(struct eggdev_convert_context *ctx);
int eggdev_mid_from_eaut(struct eggdev_convert_context *ctx);
int eggdev_metadata_from_metatxt(struct eggdev_convert_context *ctx);
int eggdev_metatxt_from_metadata(struct eggdev_convert_context *ctx);
int eggdev_strings_from_strtxt(struct eggdev_convert_context *ctx);
int eggdev_strtxt_from_strings(struct eggdev_convert_context *ctx);
int eggdev_tilesheet_from_tstxt(struct eggdev_convert_context *ctx);
int eggdev_tstxt_from_tilesheet(struct eggdev_convert_context *ctx);
int eggdev_decalsheet_from_dstxt(struct eggdev_convert_context *ctx);
int eggdev_dstxt_from_decalsheet(struct eggdev_convert_context *ctx);
int eggdev_map_from_maptxt(struct eggdev_convert_context *ctx);
int eggdev_maptxt_from_map(struct eggdev_convert_context *ctx);
int eggdev_sprite_from_sprtxt(struct eggdev_convert_context *ctx);
int eggdev_sprtxt_from_sprite(struct eggdev_convert_context *ctx);
int eggdev_cmdlist_from_cmdltxt(struct eggdev_convert_context *ctx);
int eggdev_cmdltxt_from_cmdlist(struct eggdev_convert_context *ctx);

/* Generic access to converters and conveniences for common use cases.
 */
typedef int (*eggdev_convert_fn)(struct eggdev_convert_context *ctx);
eggdev_convert_fn eggdev_get_converter(int dstfmt,int srcfmt);
int eggdev_convert_for_rom(struct sr_encoder *dst,const void *src,int srcc,int srcfmt,const char *path,struct sr_encoder *errmsg);
int eggdev_convert_for_extraction(struct sr_encoder *dst,const void *src,int srcc,int srcfmt,int tid,struct sr_encoder *errmsg);
int eggdev_convert_auto(
  struct sr_encoder *dst, // Required.
  const void *src,int srcc, // Required.
  int dstfmt,int srcfmt, // Provide as much as you know...
  const char *dstpath,
  const char *srcpath,
  int tid
);

int eggdev_convert_error(struct eggdev_convert_context *ctx,const char *fmt,...);
int eggdev_convert_error_at(struct eggdev_convert_context *ctx,int lineno,const char *fmt,...); // Give us the line number relative to (ctx->src), we add (lineno0).

#endif
