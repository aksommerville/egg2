/* eggdev_convert.h
 * General data conversion.
 */
 
#ifndef EGGDEV_CONVERT_H
#define EGGDEV_CONVERT_H

#include "opt/serial/serial.h"

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
  _(png) _(gif) _(jpeg) _(wav) _(mid) _(eau) \
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
int eggdev_convert_noop(struct sr_convert_context *ctx); // Just copy (src) to (dst), no conversion.
int eggdev_egg_from_exe(struct sr_convert_context *ctx); // ie extract ROM.
int eggdev_egg_from_zip(struct sr_convert_context *ctx); // Zip must contain "game.bin" at its root.
int eggdev_zip_from_egg(struct sr_convert_context *ctx);
int eggdev_png_from_png(struct sr_convert_context *ctx); // Same format, but some optimizations.
int eggdev_wav_from_eau(struct sr_convert_context *ctx); // Stands a synthesizer and records it.
int eggdev_wav_from_mid(struct sr_convert_context *ctx);
int eau_cvt_midi_eau(struct sr_convert_context *ctx); // Owned by the "eau" unit, not us.
int eau_cvt_eau_midi(struct sr_convert_context *ctx); // ''
int eggdev_metadata_from_metatxt(struct sr_convert_context *ctx);
int eggdev_metatxt_from_metadata(struct sr_convert_context *ctx);
int eggdev_strings_from_strtxt(struct sr_convert_context *ctx);
int eggdev_strtxt_from_strings(struct sr_convert_context *ctx);
int eggdev_tilesheet_from_tstxt(struct sr_convert_context *ctx);
int eggdev_tstxt_from_tilesheet(struct sr_convert_context *ctx);
int eggdev_decalsheet_from_dstxt(struct sr_convert_context *ctx);
int eggdev_dstxt_from_decalsheet(struct sr_convert_context *ctx);
int eggdev_map_from_maptxt(struct sr_convert_context *ctx);
int eggdev_maptxt_from_map(struct sr_convert_context *ctx);
int eggdev_sprite_from_sprtxt(struct sr_convert_context *ctx);
int eggdev_sprtxt_from_sprite(struct sr_convert_context *ctx);
int eggdev_cmdlist_from_cmdltxt(struct sr_convert_context *ctx);
int eggdev_cmdltxt_from_cmdlist(struct sr_convert_context *ctx);

/* Generic access to converters and conveniences for common use cases.
 * These may use extra parameters in (eggdev): strip,rate,chanc
 */
sr_convert_fn eggdev_get_converter(int dstfmt,int srcfmt);
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

#endif
