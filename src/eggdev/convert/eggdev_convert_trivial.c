/* eggdev_convert_trivial.c
 * Data converters that lean on other converters, or are too simple to worry much about.
 */

#include "eggdev/eggdev_internal.h"
#include "opt/image/image.h"

/* Noop is as trivial as you'd think.
 */

int eggdev_convert_noop(struct eggdev_convert_context *ctx) {
  return sr_encode_raw(ctx->dst,ctx->src,ctx->srcc);
}

/* Lots of converters are just a chain of two other converters.
 */

static int eggdev_convert_with_intermediate(
  struct eggdev_convert_context *ctx,
  eggdev_convert_fn cvt2, // Output from intermediate.
  eggdev_convert_fn cvt1 // Intermediate from input.
) {
  if (!ctx||!cvt1||!cvt2) return -1;
  struct sr_encoder middata={0};
  struct eggdev_convert_context midctx=*ctx;
  midctx.dst=&middata;
  int err=cvt1(&midctx);
  if (err<0) {
    sr_encoder_cleanup(&middata);
    return err;
  }
  midctx=*ctx;
  midctx.src=middata.v;
  midctx.srcc=middata.c;
  err=cvt2(&midctx);
  sr_encoder_cleanup(&middata);
  return err;
}

int eggdev_zip_from_html(struct eggdev_convert_context *ctx) {
  return eggdev_convert_with_intermediate(ctx,eggdev_zip_from_egg,eggdev_egg_from_html);
}

int eggdev_zip_from_exe(struct eggdev_convert_context *ctx) {
  return eggdev_convert_with_intermediate(ctx,eggdev_zip_from_egg,eggdev_egg_from_exe);
}

int eggdev_html_from_zip(struct eggdev_convert_context *ctx) {
  return eggdev_convert_with_intermediate(ctx,eggdev_html_from_egg,eggdev_egg_from_zip);
}

int eggdev_html_from_exe(struct eggdev_convert_context *ctx) {
  return eggdev_convert_with_intermediate(ctx,eggdev_html_from_egg,eggdev_egg_from_exe);
}

/* sprite is just cmdlist with a signature and a specific namespace.
 */

int eggdev_sprite_from_sprtxt(struct eggdev_convert_context *ctx) {
  if (sr_encode_raw(ctx->dst,"\0ESP",4)<0) return -1;
  struct eggdev_convert_context modctx=*ctx;
  modctx.ns="sprite";
  modctx.nsc=6;
  return eggdev_cmdlist_from_cmdltxt(&modctx);
}

int eggdev_sprtxt_from_sprite(struct eggdev_convert_context *ctx) {
  if ((ctx->srcc<4)||memcmp(ctx->src,"\0ESP",4)) return -1;
  struct eggdev_convert_context modctx=*ctx;
  modctx.ns="sprite";
  modctx.nsc=6;
  modctx.src=(char*)ctx->src+4;
  modctx.srcc=ctx->srcc-4;
  return eggdev_cmdltxt_from_cmdlist(&modctx);
}

/* Images are trivial -- the "image" unit does all the real work.
 */
 
int eggdev_png_from_png(struct eggdev_convert_context *ctx) {
  int w=0,h=0;
  if (image_measure(&w,&h,ctx->src,ctx->srcc)<0) {
    return eggdev_convert_error(ctx,"Failed to decode PNG file.");
  }
  void *pixels=calloc(w<<2,h);
  if (!pixels) return -1;
  if (image_decode(pixels,w*h*4,ctx->src,ctx->srcc)<0) {
    free(pixels);
    return eggdev_convert_error(ctx,"Failed to decode PNG file.");
  }
  int err=image_encode(ctx->dst,pixels,w*h*4,w,h);
  free(pixels);
  if (err<0) return eggdev_convert_error(ctx,"Failed to reencode PNG file.");
  return 0;
}
