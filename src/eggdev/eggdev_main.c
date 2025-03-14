#include "eggdev_internal.h"
#include "opt/zip/zip.h"

struct g g={0};

/* Main.
 */
 
static int cb_write_zip(const char *path,const char *base,char ftype,void *userdata) {
  struct zip_writer *writer=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='f') return 0;
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -1;
  }
  int basec=0; while (base[basec]) basec++;
  struct zip_file file={
    .zip_version=20,
    .flags=0,
    .compression=0,
    .mtime=0,
    .mdate=0,
    .crc=0,
    .csize=0,
    .usize=srcc,
    .name=base,
    .namec=basec,
    .extra=0,
    .extrac=0,
    .cdata=0,
    .udata=src,
  };
  if (zip_writer_add(writer,&file)<0) {
    fprintf(stderr,"%s: zip_writer_add failed\n",path);
    free(src);
    return -1;
  }
  free(src);
  return 0;
}
 
int main(int argc,char **argv) {
  g.exename="eggdev";
  fprintf(stderr,"eggdev main\n");
  
  /* looking good *
  const char *path="testing.zip";
  fprintf(stderr,"Trying to read Zip file '%s'...\n",path);
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"...read failed\n");
    return 1;
  }
  struct zip_reader reader={0};
  if (zip_reader_init(&reader,src,srcc)<0) {
    fprintf(stderr,"...zip_reader_init failed\n");
    return 1;
  }
  struct zip_file file;
  int err;
  while ((err=zip_reader_next(&file,&reader))>0) {
    fprintf(stderr,"  %6d/%6d %d %.*s\n",file.csize,file.usize,file.compression,file.namec,file.name);
  }
  free(src);
  if (err<0) fprintf(stderr,"...error\n");
  else fprintf(stderr,"...ok\n");
  /**/
  
  struct zip_writer writer={0};
  int err=dir_read("src/eggdev",cb_write_zip,&writer);
  if (err<0) {
    fprintf(stderr,"error %d\n",err);
    return 1;
  }
  void *src=0;
  int srcc=zip_writer_finish(&src,&writer);
  if (srcc<0) {
    fprintf(stderr,"zip_writer_finish: %d\n",srcc);
    return 1;
  }
  if (file_write("scrambled_by_egg.zip",src,srcc)<0) {
    fprintf(stderr,"Failed to write file, %d bytes\n",srcc);
    return 1;
  }
  zip_writer_cleanup(&writer);
  
  return 0;
}
