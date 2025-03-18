#include "eggdev/eggdev_internal.h"
#include "opt/res/res.h"

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
      if (subrid&&(ln[0]>='a')&&(ln[0]<='z')&&(ln[1]>='a')&&(ln[2]<='z')) {
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
    if (romc!=-2) fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return -2;
  }
  int srcfmt=eggdev_fmt_by_signature(rom,romc);
  if (srcfmt!=EGGDEV_FMT_egg) {
    eggdev_convert_fn convert=eggdev_get_converter(EGGDEV_FMT_egg,srcfmt);
    if (!convert) {
      fprintf(stderr,"%s: Invalid ROM, no converter found.\n",srcpath);
      free(rom);
      return -2;
    }
    struct sr_encoder dst={0};
    struct eggdev_convert_context ctx={
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
  else if (!strcmp(g.format,"raw")) err=eggdev_list_raw(&reader,srcpath);
  else {
    fprintf(stderr,"%s: Unknown list format '%s'. Expected one of: default summary raw.\n",g.exename,g.format);
    err=-2;
  }
  free(rom);
  return err;
}
