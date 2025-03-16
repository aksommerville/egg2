#include "eggdev/eggdev_internal.h"

/* Convert, main entry point.
 */
 
int eggdev_main_convert() {
  const char *srcpath=0,*dstpath=0;
  if (g.srcpathc==0) srcpath="<stdin>";
  else if (g.srcpathc==1) srcpath=g.srcpathv[0];
  else {
    fprintf(stderr,"%s: Multiple input paths.\n",g.exename);
    return -2;
  }
  if (g.dstpath) dstpath=g.dstpath;
  else dstpath="<stdout>";
  void *src=0;
  int srcc=eggdev_read_input(&src,srcpath);
  int err=-1;
  struct sr_encoder dst={0};
  int srcfmt=eggdev_fmt_eval(g.srcfmt,-1);
  if (g.dstfmt&&!strcmp(g.dstfmt,"rommable")) {
    err=eggdev_convert_for_rom(&dst,src,srcc,srcfmt,srcpath,0);
  } else if (g.dstfmt&&!strcmp(g.dstfmt,"portable")) {
    int tid=eggdev_tid_by_path_or_fmt(srcpath,-1,srcfmt);
    err=eggdev_convert_for_extraction(&dst,src,srcc,srcfmt,tid,0);
  } else {
    int dstfmt=eggdev_fmt_eval(g.dstfmt,-1);
    err=eggdev_convert_auto(&dst,src,srcc,dstfmt,srcfmt,dstpath,srcpath,0);
  }
  free(src);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during conversion.\n",srcpath);
    return -2;
  }
  if (eggdev_write_output(g.dstpath,dst.v,dst.c)<0) {
    sr_encoder_cleanup(&dst);
    fprintf(stderr,"%s: Failed to write file.\n",dstpath);
    return -2;
  }
  return 0;
}
