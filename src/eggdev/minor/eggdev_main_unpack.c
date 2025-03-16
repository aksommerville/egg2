#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"

/* Unpack to existing directory with newly initialized rom reader.
 */
 
static int eggdev_unpack_inner(const char *dstpath,struct eggdev_rom_reader *reader,const char *srcpath) {
  struct eggdev_res res;
  int err,pvtid=0,tnamec=0;
  char tname[32];
  struct sr_encoder scratch={0};
  while ((err=eggdev_rom_reader_next(&res,reader))>0) {
  
    // Whenever the tid changes, we need a new directory.
    if (res.tid!=pvtid) {
      tnamec=eggdev_tid_repr(tname,sizeof(tname),res.tid);
      if ((tnamec<1)||(tnamec>sizeof(tname))) return -1;
      char path[1024];
      int pathc=snprintf(path,sizeof(path),"%s/%.*s",dstpath,tnamec,tname);
      if ((pathc<1)||(pathc>=sizeof(path))) return -1;
      if (dir_mkdir(path)<0) {
        fprintf(stderr,"%s: mkdir failed\n",path);
        sr_encoder_cleanup(&scratch);
        return -2;
      }
      pvtid=res.tid;
    }
    
    // If not verbatim, check for conversion.
    const void *serial=res.v;
    int serialc=res.c;
    if (!g.verbatim) {
      int srcfmt=eggdev_fmt_by_signature(serial,serialc);
      if (!srcfmt) srcfmt=eggdev_fmt_by_tid(res.tid);
      if (srcfmt) {
        int dstfmt=eggdev_fmt_portable(srcfmt);
        if (dstfmt&&(dstfmt!=srcfmt)) {
          eggdev_convert_fn convert=eggdev_get_converter(dstfmt,srcfmt);
          if (convert&&(convert!=eggdev_convert_noop)) {
            scratch.c=0;
            struct eggdev_convert_context ctx={
              .dst=&scratch,
              .src=serial,
              .srcc=serialc,
              .refname=srcpath,
            };
            if ((err=convert(&ctx))<0) {
              if (err!=-2) fprintf(stderr,"%s:%.*s:%d: Unspecified error converting.\n",srcpath,tnamec,tname,res.rid);
              sr_encoder_cleanup(&scratch);
              return -2;
            }
            serial=scratch.v;
            serialc=scratch.c;
          }
        }
      }
    }
    
    // Write it out.
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%s/%.*s/%d",dstpath,tnamec,tname,res.rid);
    if ((pathc<1)||(pathc>=sizeof(path))) {
      sr_encoder_cleanup(&scratch);
      return -1;
    }
    if (file_write(path,serial,serialc)<0) {
      sr_encoder_cleanup(&scratch);
      fprintf(stderr,"%s: Failed to write file, %d bytes.\n",path,serialc);
      return -2;
    }
  }
  if (err<0) {
    fprintf(stderr,"%s: Malformed ROM.\n",srcpath);
    sr_encoder_cleanup(&scratch);
    return -2;
  }
  sr_encoder_cleanup(&scratch);
  return 0;
}

/* Unpack, main entry point.
 */
 
int eggdev_main_unpack() {

  if (!g.dstpath) {
    fprintf(stderr,"%s: Output path required.\n",g.exename);
    return -2;
  }
  const char *srcpath=0;
  if (g.srcpathc>=1) srcpath=g.srcpathv[0];
  
  void *src=0;
  int srcc=eggdev_read_input(&src,srcpath);
  if (!srcpath) srcpath="<stdin>";
  if (srcc<0) {
    if (srcc!=2) fprintf(stderr,"%s: Failed to read input.\n",srcpath);
    return -2;
  }
  
  // If it's not an Egg ROM, convert it.
  int srcfmt=eggdev_fmt_by_signature(src,srcc);
  if (!srcfmt) srcfmt=eggdev_fmt_by_path(srcpath,-1);
  if (!srcfmt) srcfmt=EGGDEV_FMT_exe; // "exe" for a generic search thru the file for embedded rom.
  if (srcfmt!=EGGDEV_FMT_egg) {
    eggdev_convert_fn convert=eggdev_get_converter(EGGDEV_FMT_egg,srcfmt);
    if (!convert) {
      free(src);
      fprintf(stderr,"%s: Not an Egg ROM and no converter found.\n",srcpath);
      return -2;
    }
    struct sr_encoder dst={0};
    struct eggdev_convert_context ctx={
      .dst=&dst,
      .src=src,
      .srcc=srcc,
      .refname=srcpath,
    };
    int err=convert(&ctx);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      free(src);
      if (err!=-2) fprintf(stderr,"%s: Unspecified error converting ROM.\n",srcpath);
      return -2;
    }
    free(src);
    src=dst.v;
    srcc=dst.c;
  }
  
  struct eggdev_rom_reader reader={0};
  if (eggdev_rom_reader_init(&reader,src,srcc)<0) {
    free(src);
    return -1;
  }
  
  if (dir_mkdir(g.dstpath)<0) {
    fprintf(stderr,"%s: mkdir failed\n",g.dstpath);
    free(src);
    return -2;
  }
  
  int err=eggdev_unpack_inner(g.dstpath,&reader,srcpath);
  if (err<0) {
    dir_rmrf(g.dstpath);
    free(src);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error extracting resources.\n",srcpath);
    return -2;
  }
  
  free(src);
  return 0;
}
