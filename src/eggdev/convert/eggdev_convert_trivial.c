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
