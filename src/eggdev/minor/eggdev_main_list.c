#include "eggdev/eggdev_internal.h"
#include "util/res/res.h"
#include "opt/image/image.h"
#include "opt/eau/eau.h"

/* List, default format.
 */
 
static int eggdev_list_default(struct rom_reader *reader,const char *path) {
  struct rom_entry res;
  int err,tid=0,tnamec=0;
  char tname[32];
  while ((err=rom_reader_next(&res,reader))>0) {
    if (res.tid!=tid) {
      tnamec=eggdev_tid_repr(tname,sizeof(tname),res.tid);
      if ((tnamec<1)||(tnamec>sizeof(tname))) tnamec=sr_decsint_repr(tname,sizeof(tname),res.tid);
      tid=res.tid;
    }
    if (tid==EGG_TID_strings) {
      int lang=res.rid>>6;
      int subrid=res.rid&0x3f;
      char ln[2]={
        'a'+(lang>>5)-1,
        'a'+(lang&0x1f)-1,
      };
      if (subrid&&(ln[0]>='a')&&(ln[0]<='z')&&(ln[1]>='a')&&(ln[1]<='z')) {
        if (subrid>=10) fprintf(stdout,"%12.*s %.2s-%-d %7d\n",tnamec,tname,ln,subrid,res.c);
        else fprintf(stdout,"%12.*s %3.2s-%-d %7d\n",tnamec,tname,ln,subrid,res.c);
        continue;
      }
    }
    fprintf(stdout,"%12.*s %5d %7d\n",tnamec,tname,res.rid,res.c);
  }
  if (err<0) {
    fprintf(stderr,"%s: Error decoding ROM.\n",path);
    return -2;
  }
  return 0;
}

/* List, raw format.
 */
 
static int eggdev_list_raw(struct rom_reader *reader,const char *path) {
  struct rom_entry res;
  int err;
  while ((err=rom_reader_next(&res,reader))>0) {
    fprintf(stdout,"%3d %5d %7d\n",res.tid,res.rid,res.c);
  }
  if (err<0) {
    fprintf(stderr,"%s: Error decoding ROM.\n",path);
    return -2;
  }
  return 0;
}

/* List, summary.
 */
 
static int eggdev_list_summary(struct rom_reader *reader,const char *path) {
  int size_by_tid[256]={0};
  int count_by_tid[256]={0};
  int maxtid=0;
  struct rom_entry res;
  int err;
  while ((err=rom_reader_next(&res,reader))>0) {
    if ((res.tid<0)||(res.tid>0xff)) return -1;
    if (res.tid>maxtid) maxtid=res.tid;
    count_by_tid[res.tid]++;
    if (size_by_tid[res.tid]>INT_MAX-res.c) size_by_tid[res.tid]=INT_MAX;
    else size_by_tid[res.tid]+=res.c;
  }
  if (err<0) {
    fprintf(stderr,"%s: Error decoding ROM.\n",path);
    return -2;
  }
  int tid=0;
  for (;tid<=maxtid;tid++) {
    if (!count_by_tid[tid]) continue;
    char tname[32];
    int tnamec=eggdev_tid_repr(tname,sizeof(tname),tid);
    if ((tnamec<1)||(tnamec>sizeof(tname))) tnamec=sr_decsint_repr(tname,sizeof(tname),tid);
    // It's technically possible for a ROM to have more than INT_MAX size within one type.
    // Of course if that actually happened, we would have rejected the file at read time.
    if (size_by_tid[res.tid]>=INT_MAX) {
      fprintf(stdout,"%12.*s %5d Total size exceeds 2 GB (it's valid, we just can't print the number).\n",tnamec,tname,count_by_tid[tid]);
    } else {
      fprintf(stdout,"%12.*s %5d %7d\n",tnamec,tname,count_by_tid[tid],size_by_tid[tid]);
    }
  }
  return 0;
}

/* "size" format, showing opinionated data-savvy stats on one line.
 */
 
static int eggdev_list_size(struct rom_reader *reader,const char *path) {
  struct {
    int code; // Raw sum of serialc. (NB zero for native executables; we're just talking about Wasm "code" resources).
    int images; // Pixels.
    int imagec;
    int songs; // Milliseconds.
    int songc;
    int sounds; // Milliseconds.
    int soundc;
    int maps; // Square meters.
    int mapc;
  } report={0};
  struct rom_entry res;
  int err;
  while ((err=rom_reader_next(&res,reader))>0) {
    switch (res.tid) {
      case EGG_TID_code: report.code+=res.c; break;
      case EGG_TID_image: {
          report.imagec++;
          int w=0,h=0;
          if (image_measure(&w,&h,res.v,res.c)>=0) {
            report.images+=w*h;
          }
        } break;
      case EGG_TID_song: {
          report.songc++;
          report.songs+=eau_estimate_duration(res.v,res.c);
        } break;
      case EGG_TID_sound: {
          report.soundc++;
          report.sounds+=eau_estimate_duration(res.v,res.c);
        } break;
      case EGG_TID_map: {
          report.mapc++;
          struct map_res map={0};
          if (map_res_decode(&map,res.v,res.c)>=0) {
            report.maps+=map.w*map.h;
          }
        } break;
    }
  }
  int songms=report.songs%1000;
  int songs=report.songs/1000;
  int songm=songs/60;
  songs%=60;
  int soundms=report.sounds%1000;
  int sounds=report.sounds/1000;
  int soundm=sounds/60;
  sounds%=60;
  fprintf(stderr,
    "%s: rom=%d code=%d image=%dpx*%d song=%d:%02d.%03d*%d sound=%d:%02d.%03d*%d map=%dm*%d\n",
    path,reader->c,report.code,report.images,report.imagec,
    songm,songs,songms,report.songc,
    soundm,sounds,soundms,report.soundc,
    report.maps,report.mapc
  );
  return 0;
}

/* List a regular file that isn't a ROM.
 * Optionally return the logical size for caller's aggregation.
 */
 
struct sizerpt {
  int runtime; // ms; for songs
  int pixelc; // for images
  int filec; // Only used by aggregator; total count of files examined.
  int usedc; // 0..filec, how many participated in the count.
};
 
static int eggdev_main_list_non_rom(struct sizerpt *sizerpt,const void *src,int srcc,const char *path,int fmt) {
  switch (fmt) {
    
    case EGGDEV_FMT_png: {
        int w=0,h=0;
        if (image_measure(&w,&h,src,srcc)>=0) {
          if (sizerpt) {
            sizerpt->pixelc=w*h;
            sizerpt->usedc=1;
          }
          fprintf(stderr,"%s: PNG, %dx%d\n",path,w,h);
        }
      } break;
      
    case EGGDEV_FMT_mid: {
        struct sr_encoder eau={0};
        struct sr_convert_context ctx={
          .dst=&eau,
          .src=src,
          .srcc=srcc,
          .refname=path,
        };
        int err=eau_cvt_eau_midi(&ctx);
        if (err<0) {
          sr_encoder_cleanup(&eau);
          return err;
        }
        int ms=eau_estimate_duration(eau.v,eau.c);
        sr_encoder_cleanup(&eau);
        if (ms<0) break;
        if (sizerpt) {
          sizerpt->runtime=ms;
          sizerpt->usedc=1;
        }
        int s=ms/1000; ms%=1000;
        int m=s/60; s%=60;
        int h=m/60; m%=60;
        fprintf(stderr,"%s: MIDI, %d:%02d:%02d.%03d\n",path,h,m,s,ms);
      } break;
      
    case EGGDEV_FMT_eau: {
        int ms=eau_estimate_duration(src,srcc);
        if (ms<0) break;
        if (sizerpt) {
          sizerpt->runtime=ms;
          sizerpt->usedc=1;
        }
        int s=ms/1000; ms%=1000;
        int m=s/60; s%=60;
        int h=m/60; m%=60;
        fprintf(stderr,"%s: EAU, %d:%02d:%02d.%03d\n",path,h,m,s,ms);
      } break;
      
  }
  return 0;
}

/* List a path that isn't a regular file.
 * It can only be a directory, and only with `--format=size`.
 */
 
static int cb_irregular(const char *path,const char *base,char ftype,void *userdata) {
  struct sizerpt *sizerpt=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return dir_read(path,cb_irregular,sizerpt);
  if (ftype=='f') {
    sizerpt->filec++;
    void *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Failed to read file.\n",path);
      return -2;
    }
    struct sizerpt tmprpt={0};
    int fmt=eggdev_fmt_by_signature(src,srcc);
    int err=eggdev_main_list_non_rom(&tmprpt,src,srcc,path,fmt);
    free(src);
    if ((err<0)||!tmprpt.usedc) return 0;
    sizerpt->usedc++;
    sizerpt->runtime+=tmprpt.runtime;
    sizerpt->pixelc+=tmprpt.pixelc;
    return 0;
  }
  return 0;
}
 
static int eggdev_main_list_irregular(const char *path) {
  if (!g.format||strcmp(g.format,"size")) return -1;
  struct sizerpt sizerpt={0};
  int err=dir_read(path,cb_irregular,&sizerpt);
  if (err<0) return err;
  int ms=sizerpt.runtime;
  if (ms<0) ms=0;
  int second=ms/1000; ms%=1000;
  int minute=second/60; second%=60;
  int hour=minute/60; minute%=60;
  fprintf(stderr,
    "%s: Totals from %d/%d files: songs=%d:%02d:%02d.%03d pixels=%d\n",
    path,sizerpt.usedc,sizerpt.filec,
    hour,minute,second,ms,
    sizerpt.pixelc
  );
  return 0;
}

/* List ROM, main entry point.
 */
 
int eggdev_main_list() {
  int err;
  const char *srcpath=0;
  if (g.srcpathc>1) {
    fprintf(stderr,"%s: Multiple inputs.\n",g.exename);
    return -2;
  } else if (g.srcpathc==1) {
    srcpath=g.srcpathv[0];
  } else {
    srcpath="-";
  }
  void *rom=0;
  int romc=eggdev_read_input(&rom,srcpath);
  if (!strcmp(srcpath,"-")) srcpath="<stdin>";
  if (romc<0) {
    err=eggdev_main_list_irregular(srcpath);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Failed to read file.\n",srcpath);
      return -2;
    }
    return 0;
  }
  int srcfmt=eggdev_fmt_by_signature(rom,romc);
  if (srcfmt!=EGGDEV_FMT_egg) {
    sr_convert_fn convert=eggdev_get_converter(EGGDEV_FMT_egg,srcfmt);
    if (!convert) {
      if (g.format&&!strcmp(g.format,"size")) {
        err=eggdev_main_list_non_rom(0,rom,romc,srcpath,srcfmt);
      } else {
        err=-1;
      }
      free(rom);
      if (err<0) {
        if (err!=-2) fprintf(stderr,"%s: Invalid ROM, no converter found.\n",srcpath);
        return -2;
      }
      return 0;
    }
    struct sr_encoder dst={0};
    struct sr_convert_context ctx={
      .dst=&dst,
      .src=rom,
      .srcc=romc,
      .refname=srcpath,
    };
    if ((err=convert(&ctx))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error converting file.\n",srcpath);
      free(rom);
    }
    free(rom);
    rom=dst.v;
    romc=dst.c;
  }
  struct rom_reader reader={0};
  if (rom_reader_init(&reader,rom,romc)<0) {
    fprintf(stderr,"%s: Invalid ROM.\n",srcpath);
    free(rom);
    return -2;
  }
  if (!g.format) err=eggdev_list_default(&reader,srcpath);
  else if (!strcmp(g.format,"default")) err=eggdev_list_default(&reader,srcpath);
  else if (!strcmp(g.format,"summary")) err=eggdev_list_summary(&reader,srcpath);
  else if (!strcmp(g.format,"size")) err=eggdev_list_size(&reader,srcpath);
  else if (!strcmp(g.format,"raw")) err=eggdev_list_raw(&reader,srcpath);
  else {
    fprintf(stderr,"%s: Unknown list format '%s'. Expected one of: default summary size raw.\n",g.exename,g.format);
    err=-2;
  }
  free(rom);
  return err;
}
