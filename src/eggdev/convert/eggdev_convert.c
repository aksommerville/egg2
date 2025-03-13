#include "eggdev/eggdev_internal.h"
#include "eggdev_convert.h"

/* Registry of formats.
 */
 
const struct eggdev_fmt eggdev_fmtv[]={
  [EGGDEV_FMT_ROM]={
    EGGDEV_FMT_ROM,
    "egg,rom",
    "\0ERM",4,
    0,
  },
  [EGGDEV_FMT_WEB_ZIP]={
    EGGDEV_FMT_WEB_ZIP,
    "zip",
    "PK",2,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_EXE]={
    EGGDEV_FMT_EXE,
    "exe",
    0,0,
    0,
  },
  [EGGDEV_FMT_HTML]={
    EGGDEV_FMT_HTML,
    "html,htm",
    "<!DOCTYPE html",14,
    EGGDEV_FMT_FLAG_TEXT|EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_CSS]={
    EGGDEV_FMT_CSS,
    "css",
    0,0,
    EGGDEV_FMT_FLAG_TEXT|EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_JS]={
    EGGDEV_FMT_JS,
    "js",
    0,0,
    EGGDEV_FMT_FLAG_TEXT|EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_PNG]={
    EGGDEV_FMT_PNG,
    "png",
    "\x89PNG\r\n\x1a\n",8,
    EGGDEV_FMT_FLAG_PORTABLE|EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_GIF]={
    EGGDEV_FMT_GIF,
    "gif",
    "GIF",3,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_JPEG]={
    EGGDEV_FMT_JPEG,
    "jpeg,jpg",
    0,0,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_ICO]={
    EGGDEV_FMT_ICO,
    "ico",
    0,0,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_WAV]={
    EGGDEV_FMT_WAV,
    "wav",
    "RIFF",4,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_MIDI]={
    EGGDEV_FMT_MIDI,
    "mid",
    "MThd\0\0\0\6",8,
    EGGDEV_FMT_FLAG_PORTABLE,
  },
  [EGGDEV_FMT_EAU]={
    EGGDEV_FMT_EAU,
    "eau",
    "\0EAU",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_EAU_TEXT]={
    EGGDEV_FMT_EAU_TEXT,
    "eaut",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_METADATA]={
    EGGDEV_FMT_METADATA,
    "metadata",
    "\0EMD",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_METADATA_TEXT]={
    EGGDEV_FMT_METADATA_TEXT,
    "metadata_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_WASM]={
    EGGDEV_FMT_WASM,
    "wasm",
    "\0asm",4,
    EGGDEV_FMT_FLAG_PORTABLE|EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_STRINGS]={
    EGGDEV_FMT_STRINGS,
    "strings",
    "\0EST",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_STRINGS_TEXT]={
    EGGDEV_FMT_STRINGS_TEXT,
    "strings_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_TILESHEET]={
    EGGDEV_FMT_TILESHEET,
    "tilesheet",
    "\0ETS",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_TILESHEET_TEXT]={
    EGGDEV_FMT_TILESHEET_TEXT,
    "tilesheet_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_DECALSHEET]={
    EGGDEV_FMT_DECALSHEET,
    "decalsheet",
    "\0EDS",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_DECALSHEET_TEXT]={
    EGGDEV_FMT_DECALSHEET_TEXT,
    "decalsheet_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_MAP]={
    EGGDEV_FMT_MAP,
    "map",
    "\0EMP",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_MAP_TEXT]={
    EGGDEV_FMT_MAP_TEXT,
    "map_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_SPRITE]={
    EGGDEV_FMT_SPRITE,
    "sprite",
    "\0ESP",4,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_SPRITE_TEXT]={
    EGGDEV_FMT_SPRITE_TEXT,
    "sprite_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
  [EGGDEV_FMT_CMDLIST]={
    EGGDEV_FMT_CMDLIST,
    "cmdlist",
    0,0,
    EGGDEV_FMT_FLAG_ROMMABLE,
  },
  [EGGDEV_FMT_CMDLIST_TEXT]={
    EGGDEV_FMT_CMDLIST_TEXT,
    "cmdlist_text",
    0,0,
    EGGDEV_FMT_FLAG_TEXT,
  },
{0}};

/* Registry of converters.
 */

const struct eggdev_converter eggdev_converterv[]={
  {eggdev_cvt_rom_webzip,EGGDEV_FMT_ROM,EGGDEV_FMT_WEB_ZIP},
  {eggdev_cvt_rom_exe,EGGDEV_FMT_ROM,EGGDEV_FMT_EXE},
  {eggdev_cvt_rom_html,EGGDEV_FMT_ROM,EGGDEV_FMT_HTML},
  {eggdev_cvt_webzip_rom,EGGDEV_FMT_WEB_ZIP,EGGDEV_FMT_ROM},
  //{eggdev_cvt_exe_rom,EGGDEV_FMT_EXE,EGGDEV_FMT_ROM}, // This would require game code, which can't be delivered generically. Unless we start using a Wasm runtime for native.
  {eggdev_cvt_html_rom,EGGDEV_FMT_HTML,EGGDEV_FMT_ROM},
  {eggdev_cvt_png_gif,EGGDEV_FMT_PNG,EGGDEV_FMT_GIF},
  {eggdev_cvt_png_jpeg,EGGDEV_FMT_PNG,EGGDEV_FMT_JPEG},
  {eggdev_cvt_png_ico,EGGDEV_FMT_PNG,EGGDEV_FMT_ICO},
  {eggdev_cvt_gif_png,EGGDEV_FMT_GIF,EGGDEV_FMT_PNG},
  {eggdev_cvt_jpeg_png,EGGDEV_FMT_JPEG,EGGDEV_FMT_PNG},
  {eggdev_cvt_ico_png,EGGDEV_FMT_ICO,EGGDEV_FMT_PNG},
  {eggdev_cvt_wav_eau,EGGDEV_FMT_WAV,EGGDEV_FMT_EAU},
  {eggdev_cvt_midi_eau,EGGDEV_FMT_MIDI,EGGDEV_FMT_EAU},
  {eggdev_cvt_eau_midi,EGGDEV_FMT_EAU,EGGDEV_FMT_MIDI},
  {eggdev_cvt_eau_eautext,EGGDEV_FMT_EAU,EGGDEV_FMT_EAU_TEXT},
  {eggdev_cvt_eautext_eau,EGGDEV_FMT_EAU_TEXT,EGGDEV_FMT_EAU},
  {eggdev_compile_metadata,EGGDEV_FMT_METADATA,EGGDEV_FMT_METADATA_TEXT},
  {eggdev_uncompile_metadata,EGGDEV_FMT_METADATA_TEXT,EGGDEV_FMT_METADATA},
  {eggdev_compile_strings,EGGDEV_FMT_STRINGS,EGGDEV_FMT_STRINGS_TEXT},
  {eggdev_uncompile_strings,EGGDEV_FMT_STRINGS_TEXT,EGGDEV_FMT_STRINGS},
  {eggdev_compile_tilesheet,EGGDEV_FMT_TILESHEET,EGGDEV_FMT_TILESHEET_TEXT},
  {eggdev_uncompile_tilesheet,EGGDEV_FMT_TILESHEET_TEXT,EGGDEV_FMT_TILESHEET},
  {eggdev_compile_decalsheet,EGGDEV_FMT_DECALSHEET,EGGDEV_FMT_DECALSHEET_TEXT},
  {eggdev_uncompile_decalsheet,EGGDEV_FMT_DECALSHEET_TEXT,EGGDEV_FMT_DECALSHEET},
  {eggdev_compile_map,EGGDEV_FMT_MAP,EGGDEV_FMT_MAP_TEXT},
  {eggdev_uncompile_map,EGGDEV_FMT_MAP_TEXT,EGGDEV_FMT_MAP},
  {eggdev_compile_sprite,EGGDEV_FMT_SPRITE,EGGDEV_FMT_SPRITE_TEXT},
  {eggdev_uncompile_sprite,EGGDEV_FMT_SPRITE_TEXT,EGGDEV_FMT_SPRITE},
  {eggdev_compile_cmdlist,EGGDEV_FMT_CMDLIST,EGGDEV_FMT_CMDLIST_TEXT},
  {eggdev_uncompile_cmdlist,EGGDEV_FMT_CMDLIST_TEXT,EGGDEV_FMT_CMDLIST},
{0}};

/* Evaluate format name.
 */

int eggdev_fmt_eval(const char *src,int srcc) {
//TODO
}

/* Represent format name.
 */
 
int eggdev_fmt_repr(char *dst,int dsta,int fmt) {
//TODO
}

/* Guess format from content and path.
 */
 
int eggdev_fmt_guess_file(const void *src,int srcc,const char *path,int pathc) {
//TODO
}

/* Guess format from content and resource type.
 */
 
int eggdev_fmt_guess_res(const void *src,int srcc,int tid) {
//TODO
}

/* Preferred ROMMABLE format for a given source format.
 */
 
int eggdev_fmt_for_rom(int srcfmt) {
//TODO
}

/* Preferred PORTABLE format for a given source format.
 */
 
int eggdev_fmt_from_rom(int srcfmt) {
//TODO
}

/* Get converter, given two formats.
 */

const struct eggdev_converter *eggdev_converter_get(int dstfmt,int srcfmt) {
  const struct eggdev_converter *converter=eggdev_converterv;
  for (;converter->fn;converter++) {
    if (converter->dstfmt!=dstfmt) continue;
    if (converter->srcfmt!=srcfmt) continue;
    return converter;
  }
  return 0;
}

//XXX TEMPORARY: Primitive converter stubs.
int eggdev_cvt_rom_webzip(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_rom_exe(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_rom_html(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_webzip_rom(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_html_rom(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_png_gif(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_png_jpeg(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_png_ico(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_gif_png(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_jpeg_png(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_ico_png(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_wav_eau(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_midi_eau(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_eau_midi(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_eau_eautext(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_cvt_eautext_eau(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_metadata(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_metadata(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_strings(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_strings(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_tilesheet(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_tilesheet(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_decalsheet(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_decalsheet(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_map(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_map(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_sprite(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_sprite(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_compile_cmdlist(struct eggdev_convert_context *ctx) { return -1; }
int eggdev_uncompile_cmdlist(struct eggdev_convert_context *ctx) { return -1; }
