/* eggdev_convert.h
 * General data conversion.
 */
 
#ifndef EGGDEV_CONVERT_H
#define EGGDEV_CONVERT_H

/* Named formats are all concrete.
 * (note that there is no "TEXT", "BINARY", "UNKNOWN"...)
 */
#define EGGDEV_FMT_ROM              1
#define EGGDEV_FMT_WEB_ZIP          2
#define EGGDEV_FMT_EXE              3
#define EGGDEV_FMT_HTML             4
#define EGGDEV_FMT_CSS              5
#define EGGDEV_FMT_JS               6
#define EGGDEV_FMT_PNG              7
#define EGGDEV_FMT_GIF              8
#define EGGDEV_FMT_JPEG             9
#define EGGDEV_FMT_ICO             10
#define EGGDEV_FMT_WAV             11
#define EGGDEV_FMT_MIDI            12
#define EGGDEV_FMT_EAU             13
#define EGGDEV_FMT_EAU_TEXT        14
#define EGGDEV_FMT_METADATA        15
#define EGGDEV_FMT_METADATA_TEXT   16
#define EGGDEV_FMT_WASM            17
#define EGGDEV_FMT_STRINGS         18
#define EGGDEV_FMT_STRINGS_TEXT    19
#define EGGDEV_FMT_TILESHEET       20
#define EGGDEV_FMT_TILESHEET_TEXT  21
#define EGGDEV_FMT_DECALSHEET      22
#define EGGDEV_FMT_DECALSHEET_TEXT 23
#define EGGDEV_FMT_MAP             24
#define EGGDEV_FMT_MAP_TEXT        25
#define EGGDEV_FMT_SPRITE          26
#define EGGDEV_FMT_SPRITE_TEXT     27
#define EGGDEV_FMT_CMDLIST         28
#define EGGDEV_FMT_CMDLIST_TEXT    29

#define EGGDEV_FMT_FLAG_TEXT       0x0001 /* Binary if unset. */
#define EGGDEV_FMT_FLAG_PORTABLE   0x0002 /* Defined by parties outside Egg. */
#define EGGDEV_FMT_FLAG_ROMMABLE   0x0004 /* Specified for inclusion in an Egg ROM. */

struct eggdev_fmt {
  int fmtid;
  const char *names; // Comma-delimited path suffix or loose name. First should be the canonical suffix. All lowercase, and they match insensitively.
  const void *magic; // Signature.
  int magicc;
  int flags;
};

struct eggdev_convert_context {
  struct sr_encoder *dst; // Required.
  const void *src; int srcc; // Required.
  const char *ns; int nsc; // Optional, for cmdlist formats.
  const char *refname; // Optional. No logging if null.
  int lineno0; // One before the first line number, if (src) is not the whole file.
  const char *comment; int commentc; // Directive from path, for resource compilers.
};

struct eggdev_converter {
  int (*fn)(struct eggdev_convert_context *ctx);
  int dstfmt; // (dstfmt,srcfmt) may be the same thing, for canonicalization converters.
  int srcfmt;
};

/* Indexed by fmtid, and there is also a zero record at the end.
 */
extern const struct eggdev_fmt eggdev_fmtv[];

/* Terminated by a zero record.
 */
extern const struct eggdev_converter eggdev_converterv[];

/* Format analysis.
 */
int eggdev_fmt_eval(const char *src,int srcc);
int eggdev_fmt_repr(char *dst,int dsta,int fmt);
int eggdev_fmt_guess_file(const void *src,int srcc,const char *path,int pathc);
int eggdev_fmt_guess_res(const void *src,int srcc,int tid);
int eggdev_fmt_for_rom(int srcfmt); // Given source file in (srcfmt), what's the best ROMMABLE format for it?
int eggdev_fmt_from_rom(int srcfmt); // Extracting a resource of this type from ROM, what's the best PORTABLE format for it?

const struct eggdev_converter *eggdev_converter_get(int dstfmt,int srcfmt);

/* Primitive converters.
 */
int eggdev_cvt_rom_webzip(struct eggdev_convert_context *ctx);
int eggdev_cvt_rom_exe(struct eggdev_convert_context *ctx);
int eggdev_cvt_rom_html(struct eggdev_convert_context *ctx);
int eggdev_cvt_webzip_rom(struct eggdev_convert_context *ctx);
int eggdev_cvt_html_rom(struct eggdev_convert_context *ctx);
int eggdev_cvt_png_gif(struct eggdev_convert_context *ctx);
int eggdev_cvt_png_jpeg(struct eggdev_convert_context *ctx);
int eggdev_cvt_png_ico(struct eggdev_convert_context *ctx);
int eggdev_cvt_gif_png(struct eggdev_convert_context *ctx);
int eggdev_cvt_jpeg_png(struct eggdev_convert_context *ctx);
int eggdev_cvt_ico_png(struct eggdev_convert_context *ctx);
int eggdev_cvt_wav_eau(struct eggdev_convert_context *ctx);
int eggdev_cvt_midi_eau(struct eggdev_convert_context *ctx);
int eggdev_cvt_eau_midi(struct eggdev_convert_context *ctx);
int eggdev_cvt_eau_eautext(struct eggdev_convert_context *ctx);
int eggdev_cvt_eautext_eau(struct eggdev_convert_context *ctx);
int eggdev_compile_metadata(struct eggdev_convert_context *ctx);
int eggdev_uncompile_metadata(struct eggdev_convert_context *ctx);
int eggdev_compile_strings(struct eggdev_convert_context *ctx);
int eggdev_uncompile_strings(struct eggdev_convert_context *ctx);
int eggdev_compile_tilesheet(struct eggdev_convert_context *ctx);
int eggdev_uncompile_tilesheet(struct eggdev_convert_context *ctx);
int eggdev_compile_decalsheet(struct eggdev_convert_context *ctx);
int eggdev_uncompile_decalsheet(struct eggdev_convert_context *ctx);
int eggdev_compile_map(struct eggdev_convert_context *ctx);
int eggdev_uncompile_map(struct eggdev_convert_context *ctx);
int eggdev_compile_sprite(struct eggdev_convert_context *ctx);
int eggdev_uncompile_sprite(struct eggdev_convert_context *ctx);
int eggdev_compile_cmdlist(struct eggdev_convert_context *ctx);
int eggdev_uncompile_cmdlist(struct eggdev_convert_context *ctx);

#endif
