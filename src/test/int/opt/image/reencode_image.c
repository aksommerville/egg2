#include "test/egg_test.h"
#include "opt/image/image.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

/* Reencode the images in our demo, and report change in encoded size.
 * 2025-09-23: Validated that we produce legal images.
 * Results are not super impressive.
 * We tend to beat the GIMP only because we drop ancillary chunks and aggressively prefer bitpacked pixels.
 * When we select the same format, we tend to come out a little bigger than GIMP.
 * Probably due to heavy-handed filter choice heuristics in opt/image/image_encode.c.
 */
 
static int reencode_image_cb(const char *path,const char *base,char ftype,void *userdata) {
  // We leak memory in failure cases. Don't worry about it.
  void *before=0;
  int beforec=file_read(&before,path);
  EGG_ASSERT_CALL(beforec,"%s: read failed",path)
  
  int w=0,h=0;
  EGG_ASSERT_CALL(image_measure(&w,&h,before,beforec))
  int pixelslen=w*h*4;
  void *pixels=malloc(pixelslen);
  EGG_ASSERT(pixels,"%s: Failed to allocate %dx%d image",path,w,h)
  EGG_ASSERT_CALL(image_decode(pixels,pixelslen,before,beforec),"%s: Failed to decode image",path)
  
  struct sr_encoder after={0};
  EGG_ASSERT_CALL(image_encode(&after,pixels,pixelslen,w,h),"%s: Failed to reencode %dx%d image.",path,w,h);
  
  if (0) { // Log all conversions.
    fprintf(stderr,"%30s %10d => %10d\n",base,beforec,after.c);
  }
  
  if (0) { // Dump output for manual examination.
    if (dir_mkdirp("mid/reencode_image")>=0) {
      char dstpath[1024];
      int dstpathc=snprintf(dstpath,sizeof(dstpath),"mid/reencode_image/%s",base);
      if ((dstpathc>0)&&(dstpathc<sizeof(dstpath))) {
        file_write(dstpath,after.v,after.c);
      }
    }
  }
  
  sr_encoder_cleanup(&after);
  free(before);
  free(pixels);
  return 0;
}
 
EGG_ITEST(reencode_image) {
  return dir_read("src/demo/src/data/image",reencode_image_cb,0);
}
