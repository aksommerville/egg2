#include "eggdev_internal.h"
#include <unistd.h>

/* File read and write helpers.
 */
 
int eggdev_read_input(void *dstpp,const char *path) {
  if (!path||!path[0]) return eggdev_read_stdin(dstpp);
  if ((path[0]=='-')&&!path[1]) return eggdev_read_stdin(dstpp);
  if (!strcmp(path,"<stdin>")) return eggdev_read_stdin(dstpp);
  return file_read(dstpp,path);
}

int eggdev_write_output(const char *path,const void *src,int srcc) {
  if (!path||!path[0]) return eggdev_write_stdout(src,srcc);
  if ((path[0]=='-')&&!path[1]) return eggdev_write_stdout(src,srcc);
  if (!strcmp(path,"<stdout>")) return eggdev_write_stdout(src,srcc);
  return file_write(path,src,srcc);
}

int eggdev_read_stdin(void *dstpp) {
  int dstc=0,dsta=8192;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  for (;;) {
    if (dstc>=dsta) {
      if (dsta>=0x10000000) {
        free(dst);
        return -1;
      }
      dsta<<=1;
      void *nv=realloc(dst,dsta);
      if (!nv) {
        free(dst);
        return -1;
      }
      dst=nv;
    }
    int err=read(STDIN_FILENO,dst+dstc,dsta-dstc);
    if (err<0) {
      free(dst);
      return -1;
    }
    if (!err) return dstc;
    dstc+=err;
  }
}

int eggdev_write_stdout(const void *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    int err=write(STDOUT_FILENO,(char*)src+srcp,srcc-srcp);
    if (err<=0) return -1;
    srcp+=err;
  }
  return 0;
}
