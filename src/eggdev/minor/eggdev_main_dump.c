#include "eggdev/eggdev_internal.h"
#include "util/res/res.h"

/* Generic hex dump.
 */
 
void eggdev_dump_serial(const uint8_t *src,int srcc) {
  const int rowlen=16;
  const int show_offset=1;
  const int show_ascii=1;
  int srcp=0;
  while (srcp<srcc) {
    if (show_offset) {
      fprintf(stdout,"%08x |",srcp);
    }
    int i;
    for (i=0;i<rowlen;i++) {
      if (srcp+i>=srcc) fprintf(stdout,"   ");
      else fprintf(stdout," %02x",src[srcp+i]);
    }
    if (show_ascii) {
      fprintf(stdout," | ");
      for (i=0;i<rowlen;i++) {
        if (srcp+i>=srcc) break;
        uint8_t ch=src[srcp+i];
        if ((ch>=0x20)&&(ch<=0x7e)) fprintf(stdout,"%c",ch);
        else fprintf(stdout,".");
      }
    }
    srcp+=rowlen;
    fprintf(stdout,"\n");
  }
  if (show_offset) {
    fprintf(stdout,"%08x\n",srcc);
  }
}

/* Dump, main entry point.
 */
 
int eggdev_main_dump() {

  /* Acquire the ROM path and resource ID.
   * Reading from stdin is allowed, but you have to say so explicitly (with "-").
   * We're using eggdev_symbol_eval() but not setting a project root.
   * If we ever want to allow smarter lookups, it's just a matter of getting the project root and eggdev_client_set_root().
   */
  int err,tid,rid;
  if (g.srcpathc!=2) {
    fprintf(stderr,"%s: Expected 'ROM TYPE:ID'\n",g.exename);
    return -2;
  }
  const char *srcpath=g.srcpathv[0];
  {
    const char *src=g.srcpathv[1];
    int srcp=0,ok=0;
    for (;src[srcp];srcp++) {
      if (src[srcp]==':') {
        if (eggdev_symbol_eval(&tid,src,srcp,EGGDEV_NSTYPE_RESTYPE,0,0)<0) {
          fprintf(stderr,"%s: Unknown resource type '%.*s'\n",g.exename,srcp,src);
          return -2;
        }
        if (eggdev_symbol_eval(&rid,src+srcp+1,-1,EGGDEV_NSTYPE_RES,src,srcp)<0) {
          fprintf(stderr,"%s: Failed to evaluate resource id '%s' for type '%.*s'\n",g.exename,src+srcp+1,srcp,src);
          return -2;
        }
        ok=1;
        break;
      }
    }
    if (!ok) {
      fprintf(stderr,"%s: Expected 'TYPE:ID', found '%s'\n",g.exename,src);
      return -2;
    }
  }
  
  /* Acquire the ROM.
   * TODO I'm not crazy about the length of this bit. Shouldn't we have a convenience somewhere for it?
   */
  void *rom=0;
  int romc=eggdev_read_input(&rom,srcpath);
  if (!strcmp(srcpath,"-")) srcpath="<stdin>";
  if (romc<0) {
    if (romc!=-2) fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return -2;
  }
  int srcfmt=eggdev_fmt_by_signature(rom,romc);
  if (srcfmt!=EGGDEV_FMT_egg) {
    sr_convert_fn convert=eggdev_get_converter(EGGDEV_FMT_egg,srcfmt);
    if (!convert) {
      fprintf(stderr,"%s: Invalid ROM, no converter found.\n",srcpath);
      free(rom);
      return -2;
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
  
  /* Locate the resource, then call up to dump it.
   */
  struct rom_reader reader={0};
  if (rom_reader_init(&reader,rom,romc)<0) {
    fprintf(stderr,"%s: Invalid ROM.\n",srcpath);
    free(rom);
    return -2;
  }
  int ok=0;
  struct rom_entry res;
  while ((err=rom_reader_next(&res,&reader))>0) {
    if ((res.tid==tid)&&(res.rid==rid)) {
      ok=1;
      eggdev_dump_serial(res.v,res.c);
      break;
    }
  }
  free(rom);
  if (err<0) {
    fprintf(stderr,"%s: Invalid ROM.\n",srcpath);
    return -2;
  }
  if (!ok) {
    fprintf(stderr,"%s: Resource %d:%d not found.\n",srcpath,tid,rid);
    return -2;
  }
  return 0;
}
