#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"

/* Add record to writer and HANDOFF provided serial to it.
 */
 
static int eggdev_pack_handoff(struct eggdev_rom_writer *writer,int tid,int rid,void *v,int c,const char *path) {
  int p=eggdev_rom_writer_search(writer,tid,rid);
  if (p>=0) {
    char trepr[16];
    int treprc=eggdev_tid_repr(trepr,sizeof(trepr),tid);
    if ((treprc<1)||(treprc>sizeof(trepr))) treprc=0;
    fprintf(stderr,"%s: Resource ID conflict %.*s:%d.\n",path,treprc,trepr,rid);
    return -2;
  }
  p=-p-1;
  struct eggdev_rw_res *res=eggdev_rom_writer_insert(writer,p,tid,rid);
  if (!res) return -1;
  eggdev_rw_res_handoff_serial(res,v,c);
  return 0;
}

/* Read file, convert if needed, and add to writer.
 */
 
static int eggdev_pack_1(struct eggdev_rom_writer *writer,const char *path,int tid,int rid) {
  int err;
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  if (g.verbatim) {
    if ((err=eggdev_pack_handoff(writer,tid,rid,src,srcc,path))<0) {
      free(src);
      return err;
    }
    return 0;
  }
  struct sr_encoder dst={0};
  if ((err=eggdev_convert_for_rom(&dst,src,srcc,0,path,0))<0) {
    free(src);
    sr_encoder_cleanup(&dst);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error converting file for ROM.\n",path);
    return -2;
  }
  free(src);
  if ((err=eggdev_pack_handoff(writer,tid,rid,dst.v,dst.c,path))<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  return 0;
}

/* Iterate inner directory.
 */
 
static int eggdev_pack_cb_bottom(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_rom_writer *writer=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='f') {
    fprintf(stderr,"%s: Unexpected file type in resources directory.\n",path);
    return -2;
  }
  int tid=0,rid=0;
  if ((eggdev_res_ids_from_path(&tid,&rid,path)<0)||(tid<1)||(rid<1)) {
    fprintf(stderr,"%s: Failed to read resource IDs from path.\n",path);
    return -2;
  }
  return eggdev_pack_1(writer,path,tid,rid);
}

/* Iterate top directory.
 */
 
static int eggdev_pack_cb_top(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_rom_writer *writer=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='f') {
    if (!strcmp(base,"metadata")) return eggdev_pack_1(writer,path,EGG_TID_metadata,1);
    if (!strcmp(base,"code.wasm")) return eggdev_pack_1(writer,path,EGG_TID_code,1);
    fprintf(stderr,"%s: Unexpected file in data directory.\n",path);
    return -2;
  }
  if (ftype!='d') {
    fprintf(stderr,"%s: Unexpected file in data directory.\n",path);
    return -2;
  }
  return dir_read(path,eggdev_pack_cb_bottom,writer);
}

/* Pack, main entry point.
 */
 
int eggdev_main_pack() {

  if (g.srcpathc!=1) {
    fprintf(stderr,"%s: Expected exactly one input path.\n",g.exename);
    return -2;
  }
  if (!g.dstpath) {
    fprintf(stderr,"%s: Output path required.\n",g.exename);
    return -2;
  }
  const char *srcpath=g.srcpathv[0];
  const char *dstpath=g.dstpath;
  
  /* If (srcpath) ends "src/data", assume it's a project in the conventional layout.
   * That means we can use project symbols, and greatly increase the odds of successful compilation.
   * But if (verbatim) was requested, don't bother, no point.
   */
  if (srcpath&&!g.verbatim) {
    int srcpathc=0;
    while (srcpath[srcpathc]) srcpathc++;
    if ((srcpathc>=8)&&!memcmp(srcpath+srcpathc-8,"src/data",8)) {
      const char *projpath=srcpath;
      int projpathc=srcpathc-8;
      if (projpathc<1) {
        eggdev_client_set_root(".",1);
      } else if ((projpathc<2)||(projpath[projpathc-1]!='/')) {
        // Oh! It wasn't actually "src". Nevermind.
      } else {
        projpathc--;
        eggdev_client_set_root(projpath,projpathc);
      }
    }
  }
  
  struct eggdev_rom_writer writer={0};
  int err=dir_read(srcpath,eggdev_pack_cb_top,&writer);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to read input files.\n",srcpath);
    eggdev_rom_writer_cleanup(&writer);
    return -2;
  }
  
  struct sr_encoder dst={0};
  if ((err=eggdev_rom_writer_encode(&dst,&writer))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to encode ROM.\n",srcpath);
    eggdev_rom_writer_cleanup(&writer);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  
  if ((err=eggdev_write_output(dstpath,dst.v,dst.c))<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to write output.\n",dstpath);
    eggdev_rom_writer_cleanup(&writer);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  
  sr_encoder_cleanup(&dst);
  eggdev_rom_writer_cleanup(&writer);
  return 0;
}
