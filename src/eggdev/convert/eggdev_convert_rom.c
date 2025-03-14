/* eggdev_convert_rom.c
 * Converters that reformat a ROM or executable, or something in that neighborhood.
 */

#include "eggdev/eggdev_internal.h"
#include "opt/zip/zip.h"

/* Validate and measure ROM.
 * We don't validate that tid and rid stay in range, only what's necessary to measure it.
 */
 
static int eggdev_measure_rom(const uint8_t *src,int srcc) {
  if (!src||(srcc<4)||memcmp(src,"\0ERM",4)) return -1;
  int srcp=4;
  for (;;) {
    if (srcp>=srcc) return -1; // Terminator required.
    uint8_t lead=src[srcp++];
    if (!lead) return srcp; // Terminator.
    switch (lead&0xc0) {
      case 0x00: break; // TID, single byte.
      case 0x40: srcp++; break; // RID
      case 0x80: { // RES
          if (srcp>srcc-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len++;
          if (srcp>srcc-len) return -1;
          srcp+=len;
        } break;
      default: return -1; // RESERVED
    }
  }
}

/* ROM from executable, ie search and extract.
 */
 
int eggdev_egg_from_exe(struct eggdev_convert_context *ctx) {
  const uint8_t *src=ctx->src;
  int srcc=ctx->srcc;
  int srcp=0,stopp=srcc-4,gotempty=0;
  while (srcp<=stopp) {
    if (memcmp(src+srcp,"\0ERM",4)) { srcp++; continue; }
    int c=eggdev_measure_rom(src+srcp,srcc-srcp);
    if (c>5) {
      return sr_encode_raw(ctx->dst,src+srcp,c);
    } else if (c==5) {
      // A 5-byte ROM, just the signature and terminator, is technically legal, but also likely to happen by accident.
      // Keep looking.
      gotempty=1;
      srcp++;
    } else {
      // If measurement failed, skip it and keep looking.
      // It's not suprising for the string "\0ERM" to appear in an executable in some context other than the embedded ROM.
      srcp++;
    }
  }
  if (gotempty) return sr_encode_raw(ctx->dst,"\0ERM\0",5);
  return eggdev_convert_error(ctx,"Egg ROM not found.");
}

/* ROM from Zip. Must be "/game.bin".
 */

int eggdev_egg_from_zip(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO zip utils
  return -2;
}

/* ROM from Standalone HTML.
 */
 
int eggdev_egg_from_html(struct eggdev_convert_context *ctx) {
  /* No need to overengineer this with an HTML parser or anything.
   * Just find <egg-rom> and </egg-rom>, and the text between them is the base64-encoded ROM.
   */
  const char *src=ctx->src;
  int srcc=ctx->srcc;
  int stopp=srcc-10;
  int beginp=-1,endp=-1;
  int srcp=0; for (;srcp<=stopp;srcp++) {
    if (beginp<0) {
      if (!memcmp(src+srcp,"<egg-rom>",9)) beginp=srcp+9;
    } else {
      if (!memcmp(src+srcp,"</egg-rom>",10)) endp=srcp;
    }
  }
  if (beginp<0) return eggdev_convert_error(ctx,"<egg-rom> tag not found.");
  if (endp<beginp) return eggdev_convert_error(ctx,"Unclosed <egg-rom> tag.");
  // Our base64 decoder does tolerate whitespace anywhere, which will probably happen here.
  for (;;) {
    int err=sr_base64_decode((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,src+beginp,endp-beginp);
    if (err<0) return eggdev_convert_error(ctx,"Malformed base64 in <egg-rom> tag.");
    if (ctx->dst->c<=ctx->dst->a-err) {
      ctx->dst->c+=err;
      break;
    }
    if (sr_encoder_require(ctx->dst,err)<0) return -1;
  }
  return 0;
}

/* Zip from ROM.
 */
 
int eggdev_zip_from_egg(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO zip utils and Separate HTML runtime
  return -2;
}

/* Standalone HTML from ROM.
 */
 
int eggdev_html_from_egg(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO Standalone HTML runtime
  return -2;
}
