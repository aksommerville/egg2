#include "eggstra.h"
#include "opt/serial/serial.h"
#include "opt/eau/eau.h"
#include <time.h>

/* --help
 */
 
void eggstra_print_help(const char *topic,int topicc) {
  if (!topic) topicc=0; else if (topicc<0) { topicc=0; while (topic[topicc]) topicc++; }
  fprintf(stderr,"\nUsage: %s COMMAND [OPTIONS|FILES...]\n\n",eggstra.exename);
  
  if (topicc) {
    fprintf(stderr,"Unknown help topic '%.*s'.\n\n",topicc,topic);
  } else {
    fprintf(stderr,
      "OPTIONS:\n"
      "  --help                  Print this message.\n"
      "\n"
      "COMMANDS:\n"
      "  play SOUNDFILE [--repeat] [--rate=HZ] [--chanc=1|2] [--driver=NAME] [--device=NAME] [--buffer=INT]\n"
      "\n"
    );
    /* TODO Possible other commands:
     *   show IMAGEFILE [--zoom=INT] [--driver=NAME] [--device=NAME]
     *   input [--driver=NAME] [--device=NAME] [--headers] [--events] [--quiet-axes]
     *   drivers
     */
  }
}

/* Add positional and dashed arguments.
 */
 
static int eggstra_srcpathv_add(const char *src) {
  if (eggstra.srcpathc>=eggstra.srcpatha) {
    int na=eggstra.srcpatha+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eggstra.srcpathv,sizeof(void*)*na);
    if (!nv) return -1;
    eggstra.srcpathv=nv;
    eggstra.srcpatha=na;
  }
  eggstra.srcpathv[eggstra.srcpathc++]=src;
  return 0;
}

static int eggstra_optv_add(const char *src) {
  while (src[0]=='-') src++;
  if (eggstra.optc>=eggstra.opta) {
    int na=eggstra.opta+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eggstra.optv,sizeof(void*)*na);
    if (!nv) return -1;
    eggstra.optv=nv;
    eggstra.opta=na;
  }
  eggstra.optv[eggstra.optc++]=src;
  return 0;
}

/* Receive command line, main entry point.
 */
 
int eggstra_configure(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggstra.exename=argv[0];
  else eggstra.exename="eggstra";
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if (!strcmp(arg,"--help")) {
      eggstra_print_help(0,0);
      eggstra.sigc=1;
      return 0;
    }
    if (!memcmp(arg,"--help=",7)) {
      eggstra_print_help(arg+7,-1);
      eggstra.sigc=1;
      return 0;
    }
    
    if (arg[0]=='-') {
      if (eggstra_optv_add(arg)<0) return -1;
      continue;
    }
    if (!eggstra.command) {
      eggstra.command=arg;
      continue;
    }
    if (eggstra_srcpathv_add(arg)<0) return -1;
  }
  return 0;
}

/* Get option.
 */
 
const char *eggstra_opt(const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  int i=0; for (;i<eggstra.optc;i++) {
    const char *opt=eggstra.optv[i];
    if (memcmp(opt,k,kc)) continue;
    if (!opt[kc]) return "";
    if (opt[kc]!='=') continue;
    return opt+kc+1;
  }
  return 0;
}

int eggstra_opti(const char *k,int kc,int fallback) {
  const char *src=eggstra_opt(k,kc);
  if (!src) return fallback;
  int v;
  if (sr_int_eval(&v,src,-1)<1) return fallback;
  return v;
}

/* stdin.
 */
 
int eggstra_read_stdin(void *dstpp) {
  int dstc=0,dsta=8192;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  for (;;) {
    if (dstc>=dsta) {
      if (dsta>=0x10000000) {
        fprintf(stderr,"%s: stdin is supplying too much data. Abort around %d bytes\n",eggstra.exename,dstc);
        free(dst);
        return -2;
      }
      void *nv=realloc(dst,dsta<<=1);
      if (!nv) {
        free(dst);
        return -1;
      }
      dst=nv;
    }
    int err=read(STDIN_FILENO,dst+dstc,dsta-dstc);
    if (err<0) {
      free(dst);
      fprintf(stderr,"%s: Error reading stdin.\n",eggstra.exename);
      return -2;
    }
    if (!err) {
      *(void**)dstpp=dst;
      return dstc;
    }
    dstc+=err;
  }
}

/* File or stdin.
 */

int eggstra_single_input(void *dstpp) {
  if (eggstra.srcpathc<1) return eggstra_read_stdin(dstpp);
  if (eggstra.srcpathc>1) {
    fprintf(stderr,"%s: Expected 1 input path, found %d.\n",eggstra.exename,eggstra.srcpathc);
    return -2;
  }
  const char *path=eggstra.srcpathv[0];
  if ((path[0]=='-')&&(path[1]==0)) return eggstra_read_stdin(dstpp);
  int dstc=file_read(dstpp,path);
  if (dstc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  return dstc;
}

/* Real or CPU time in floating-point seconds.
 */
 
double eggstra_now_real() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

double eggstra_now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

/* Combined CHDRs from EAU file.
 * Allocates (*dstpp) on success, but not if zero.
 */
 
static int eggstract_chdrs_chunk(void *dstpp,const uint8_t *src,int srcc) {
  struct eau_file file={0};
  if (eau_file_decode(&file,src,srcc)<0) return -1;
  if (file.chdrc<1) return 0;
  void *dst=malloc(file.chdrc);
  if (!dst) return -1;
  memcpy(dst,file.chdr,file.chdrc);
  *(void**)dstpp=dst;
  return file.chdrc;
}

/* Find one CHDR by chid, in the CHDRs chunk.
 * (*dstpp) is WEAK.
 */
 
static int eggstract_chdr(void *dstpp,const uint8_t *src,int srcc,int fqpid) {
  struct eau_chdr_reader reader={.v=src,.c=srcc};
  struct eau_chdr_entry entry;
  while (eau_chdr_reader_next(&entry,&reader)>0) {
    if (entry.chid!=fqpid) continue;
    *(const void**)dstpp=entry.v;
    return entry.c;
  }
  return 0;
}

/* Get an SDK instrument.
 */
 
int eggstra_get_chdr(void *dstpp,int fqpid) {
  if ((fqpid<0)||(fqpid>0xff)) return 0; // Not encodable in a straight EAU file like the SDK instruments.

  // Find the SDK instruments if we haven't tried yet.
  if (!eggstra.chdrc) {
    const char *EGG_SDK=getenv("EGG_SDK");
    if (!EGG_SDK||!EGG_SDK[0]) {
      fprintf(stderr,"%s: Standard instruments not found (EGG_SDK not defined). Will use undesirable default instruments.\n",eggstra.exename);
      eggstra.chdrc=-1;
      return 0;
    }
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%s/src/eggdev/instruments.eau",EGG_SDK);
    if ((pathc<1)||(pathc>=sizeof(path))) {
      eggstra.chdrc=-1;
      return 0;
    }
    uint8_t *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) {
      fprintf(stderr,"%s: Standard instruments not found at '%.*s'. Will use undesirable default instruments.\n",eggstra.exename,pathc,path);
      eggstra.chdrc=-1;
      return 0;
    }
    eggstra.chdrc=eggstract_chdrs_chunk(&eggstra.chdr,src,srcc);
    free(src);
    if (!eggstra.chdrc) eggstra.chdrc=-1;
  }
  
  if (eggstra.chdrc<0) return 0;
  return eggstract_chdr(dstpp,eggstra.chdr,eggstra.chdrc,fqpid);
}
