#include "eggrun_internal.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"
#include "opt/zip/zip.h"

const char *eggrun_rom_path=0;

/* Extract ROM from HTML.
 */
 
static int eggrun_eggstract_html(void *dstpp,const char *src,int srcc,const char *path) {
  // We can be a little naive about this. We're looking for the exact strings "<egg-rom>" and "</egg-rom>", and there can't be tags in between.
  int openp=-1,closep=-1,srcp=0;
  int stopp=srcc-10;
  for (;srcp<=stopp;srcp++) {
    if (!memcmp(src+srcp,"<egg-rom>",9)) {
      srcp+=9;
      openp=srcp;
      break;
    }
  }
  if (openp<0) {
    fprintf(stderr,"%s: Not an Egg HTML (<egg-rom> tag not found)\n",path);
    return -2;
  }
  for (;srcp<=stopp;srcp++) {
    if (!memcmp(src+srcp,"</egg-rom>",10)) {
      closep=srcp;
      break;
    }
  }
  if (closep<0) {
    fprintf(stderr,"%s: Unclosed <egg-rom> tag.\n",path);
    return -1;
  }
  // Then decode base-64, and that yields an Egg ROM with code:1, or should.
  const char *bsrc=src+openp;
  int bsrcc=closep-openp;
  int dsta=(bsrcc*3+2)/4;
  void *dst=malloc(dsta);
  if (!dst) return -1;
  int dstc=sr_base64_decode(dst,dsta,bsrc,bsrcc);
  if ((dstc<0)||(dstc>dsta)) {
    free(dst);
    fprintf(stderr,"%s: Failed to decode base64-encoded ROM.\n",path);
    return -2;
  }
  *(void**)dstpp=dst;
  return dstc;
}

/* Extract ROM from ZIP.
 */
 
static int eggrun_eggstract_zip(void *dstpp,const uint8_t *src,int srcc,const char *path) {
  struct zip_reader reader={0};
  if (zip_reader_init(&reader,src,srcc)<0) {
    fprintf(stderr,"%s: Failed to start read of ZIP file.\n",path);
    return -2;
  }
  struct zip_file file={0};
  int ok=0,dstc=-1;
  while (zip_reader_next(&file,&reader)>0) {
    if ((file.usize>=4)&&!memcmp(file.udata,"\0ERM",4)) {
      ok=1;
      break;
    }
  }
  if (ok) {
    void *dst=malloc(file.usize);
    if (!dst) {
      zip_reader_cleanup(&reader);
      return -1;
    }
    memcpy(dst,file.udata,file.usize);
    dstc=file.usize;
    *(void**)dstpp=dst;
  } else {
    fprintf(stderr,"%s: Archive does not contain any Egg ROMs.\n",path);
    dstc=-2;
  }
  zip_reader_cleanup(&reader);
  return dstc;
}

/* Extract ROM from arbitrary binary file.
 */
 
static int eggrun_measure_rom(const uint8_t *src,int srcc) {
  int srcp=4; // Signature has already been checked.
  for (;;) {
    if (srcp>=srcc) return 0; // No terminator. Invalid.
    uint8_t lead=src[srcp++];
    if (!lead) return srcp; // Terminator. Valid.
    switch (lead&0xc0) {
      case 0x00: break; // TID
      case 0x40: { // RID
          if (srcp>srcc-1) return -1;
          srcp++;
        } break;
      case 0x80: { // RES
          if (srcp>srcc-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len++;
          if (srcp>srcc-len) return -1;
          srcp+=len;
        } break;
      default: return -1; // Reserved, invalid.
    }
  }
}
 
static int eggrun_eggstract_binary(void *dstpp,const uint8_t *src,int srcc,const char *path) {
  /* The signature string "\0ERM" may occur more than once in an executable.
   * When we find it, scan the entire ROM following it.
   * The first valid ROM that contains at least one resource, ie len>5, we'll keep it.
   * If we end up only with a valid but empty ROM, keep it even though it will fail at load.
   * (It wouldn't be correct to say "Not an Egg ROM" in those cases).
   */
  int srcp=0,stopp=srcc-4,candidatec=0;
  const void *candidate=0;
  while (srcp<stopp) {
    if (memcmp(src+srcp,"\0ERM",4)) {
      srcp++;
    } else {
      int len=eggrun_measure_rom(src+srcp,srcc-srcp);
      if (len<4) {
        srcp++;
      } else if (len>candidatec) {
        candidate=src+srcp;
        candidatec=len;
        if (candidatec>5) break;
        srcp+=len;
      }
    }
  }
  if (candidatec<4) {
    fprintf(stderr,"%s: Not an Egg ROM.\n",path);
    return -2;
  }
  void *dst=malloc(candidatec);
  if (!dst) return -1;
  memcpy(dst,candidate,candidatec);
  *(void**)dstpp=dst;
  return candidatec;
}

/* Extract ROM from input file.
 * Always populates (*dstpp) on success. May populate with (src) verbatim, if it's a proper ROM file.
 * Be careful not to free both in that case.
 */
 
static int eggrun_eggstract(void *dstpp,const uint8_t *src,int srcc,const char *path) {
  eggrun_rom_path=path;
  
  /* If it starts with the Egg ROM Signature, take the rest on faith.
   */
  if ((srcc>=4)&&!memcmp(src,"\0ERM",4)) {
    *(const void**)dstpp=src;
    return srcc;
  }
  
  /* If it's HTML, locate the base64-encoded ROM and decode it into a new buffer.
   */
  if ((srcc>=9)&&(!memcmp(src,"<!DOCTYPE",9)||!memcmp(src,"<!doctype",9))) {
    // lol, we don't care what the "doctype" actually says it is; its presence means it can only be HTML.
    return eggrun_eggstract_html(dstpp,(char*)src,srcc,path);
  }
  
  /* If it starts "PK" it's probably ZIP. At least, it won't be any other known format.
   * If you happen to invent a time machine, please tell PKWare to use an unambiguous signature.
   * Tell Compuserve too; GIF has the same problem.
   */
  if ((srcc>=2)&&!memcmp(src,"PK",2)) {
    return eggrun_eggstract_zip(dstpp,src,srcc,path);
  }
  
  /* It could be a native eggzekutable with the ROM embedded.
   * When we produce those executables, we strip code:1, so they won't be useable here.
   * But give it a bona fide attempt anyway.
   */
  return eggrun_eggstract_binary(dstpp,src,srcc,path);
}

/* Load file, main entry point.
 */
 
int eggrun_load_file(void *dstpp,int argc,char **argv) {
  
  const char *path=0;
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    /* First positional argument is the ROM path.
     * Keep reading so we can fail sensibly if there's more than one.
     */
    if (arg[0]!='-') {
      if (path) {
        fprintf(stderr,"%s: Multiple ROM paths.\n",eggrt.exename);
        return -2;
      }
      path=arg;
      argv[argi-1]="";
      continue;
    }
    
    /* Important to skip arguments the same way eggrt does:
     *   "-kvv" "-k vv" "--kk=vv" or "--kk vv"
     * Both single and double dash options can consume two arguments.
     * We don't care about the content of arguments.
     * That could change in the future if there are eggrun-specific concerns, we could consume and blank them here like we do the ROM path.
     */
    if (!arg[1]) continue; // Single dash alone, eat it.
    if (arg[1]!='-') { // Single dash...
      if (arg[2]) continue; // Single dash with value included.
      if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) argi++; // Consume next arg.
      continue;
    }
    if (!arg[2]) continue; // Double dash alone, eat it.
    int havev=0,i=2;
    for (;arg[i];i++) if (arg[i]=='=') {
      havev=1;
      break;
    }
    if (havev) continue; // Double dash with value included.
    if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) argi++; // Consume next arg.
  }
  
  if (!path) {
    fprintf(stderr,"%s: ROM path required.\n",eggrt.exename);
    return -2;
  }
  
  void *serial=0;
  int serialc=file_read(&serial,path);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  
  void *rom=0;
  int romc=eggrun_eggstract(&rom,serial,serialc,path);
  if (romc<0) {
    free(serial);
    if (romc!=-2) fprintf(stderr,"%s: Not an Egg ROM.\n",path);
    return -2;
  }
  
  eggrt.rompath=path;
  if (rom!=serial) free(serial);
  *(void**)dstpp=rom;
  return romc;
}
