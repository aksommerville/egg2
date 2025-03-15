#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"
#include "builder.h"

/* Resource IDs.
 */
 
static int eggdev_res_ids_from_path(int *tid,int *rid,const char *path) {
  const char *tname=0,*base=path;
  int tnamec=0,basec=0;
  int pathp=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') {
      tname=base;
      tnamec=basec;
      base=path+pathp+1;
      basec=0;
    } else {
      basec++;
    }
  }
  if ((basec==8)&&!memcmp(base,"metadata",8)) { *tid=EGG_TID_metadata; *rid=1; return 0; }
  if ((basec==9)&&!memcmp(base,"code.wasm",9)) { *tid=EGG_TID_code; *rid=1; return 0; }
  if ((*tid=eggdev_tid_eval(tname,tnamec))<1) return -1;
  int lang=0;
  if ((basec>=3)&&(base[0]>='a')&&(base[0]<='z')&&(base[1]>='a')&&(base[1]<='z')&&(base[2]=='-')) {
    lang=((base[0]-'a'+1)<<5)|(base[1]-'a'+1);
    base+=3;
    basec-=3;
  }
  if ((basec<1)||(base[0]<'0')||(base[0]>='9')) return -1;
  *rid=0;
  int basep=0;
  while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
    (*rid)*=10;
    (*rid)+=base[basep++]-'0';
    if (*rid>0xffff) return -1;
  }
  if (!*rid) return -1;
  if (lang) {
    if (*rid>0x3f) return -1;
    (*rid)|=lang<<6;
  }
  return 0;
}

/* Read file, compile resource, and add to ROM.
 */
 
static int eggdev_compile_data_res(struct eggdev_rom_writer *writer,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=eggdev_convert_for_rom(&dst,src,srcc,0,path);
  free(src);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error compiling resource.\n",path);
    return -2;
  }
  int tid=0,rid=0;
  if (eggdev_res_ids_from_path(&tid,&rid,path)<0) {
    fprintf(stderr,"%s: Failed to extract resource IDs from path.\n",path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  int resp=eggdev_rom_writer_search(writer,tid,rid);
  if (resp>=0) {
    fprintf(stderr,"%s: Duplicate resource %d:%d\n",path,tid,rid);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  resp=-resp-1;
  struct eggdev_rw_res *res=eggdev_rom_writer_insert(writer,resp,tid,rid);
  if (!res) {
    sr_encoder_cleanup(&dst);
    return -1;
  }
  eggdev_rw_res_handoff_serial(res,dst.v,dst.c);
  return 0;
}

/* Decode a ROM file into writer.
 */
 
static int eggdev_compile_data_rom(struct eggdev_rom_writer *writer,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",path);
    return -2;
  }
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,src,srcc)<0) {
    free(src);
    fprintf(stderr,"%s: Malformed ROM.\n",path);
    return -2;
  }
  struct eggdev_res res;
  int err;
  while ((err=eggdev_rom_reader_next(&res,&reader))>0) {
    int p=eggdev_rom_writer_search(writer,res.tid,res.rid);
    if (p>=0) {
      free(src);
      fprintf(stderr,"%s: Duplicate resource %d:%d\n",path,res.tid,res.rid);
      return -2;
    }
    p=-p-1;
    struct eggdev_rw_res *dst=eggdev_rom_writer_insert(writer,p,res.tid,res.rid);
    if (!dst) {
      free(src);
      return -1;
    }
    if (eggdev_rw_res_set_serial(dst,res.v,res.c)<0) {
      free(src);
      return -1;
    }
  }
  free(src);
  if (err<0) return err;
  return 0;
}

/* Data-only ROM. (sync)
 */
 
int build_datarom(struct builder *builder,struct builder_file *file) {
  struct eggdev_rom_writer writer={0};
  int i=0; for (;i<file->reqc;i++) {
    struct builder_file *req=file->reqv[i];
    int err=eggdev_compile_data_res(&writer,req->path);
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error adding to ROM.\n",req->path);
      eggdev_rom_writer_cleanup(&writer);
      return -2;
    }
  }
  struct sr_encoder serial={0};
  int err=eggdev_rom_writer_encode(&serial,&writer);
  eggdev_rom_writer_cleanup(&writer);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding ROM.\n",file->path);
    sr_encoder_cleanup(&serial);
    return -2;
  }
  err=file_write(file->path,serial.v,serial.c);
  sr_encoder_cleanup(&serial);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file\n",file->path);
    return -2;
  }
  file->ready=1;
  return 0;
}

/* Full ROM, combining a data-only ROM and code:1. (sync)
 */
 
int build_fullrom(struct builder *builder,struct builder_file *file) {
  struct eggdev_rom_writer writer={0};
  int i=0; for (;i<file->reqc;i++) {
    struct builder_file *req=file->reqv[i];
    int err=0;
    if (req->hint==BUILDER_FILE_HINT_CODE1) {
      err=eggdev_compile_data_res(&writer,req->path);
    } else if (req->hint==BUILDER_FILE_HINT_DATAROM) {
      err=eggdev_compile_data_rom(&writer,req->path);
    }
    if (err<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error merging ROMs.\n",req->path);
      eggdev_rom_writer_cleanup(&writer);
      return -2;
    }
  }
  struct sr_encoder serial={0};
  int err=eggdev_rom_writer_encode(&serial,&writer);
  eggdev_rom_writer_cleanup(&writer);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error encoding ROM.\n",file->path);
    sr_encoder_cleanup(&serial);
    return -2;
  }
  err=file_write(file->path,serial.v,serial.c);
  sr_encoder_cleanup(&serial);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file\n",file->path);
    return -2;
  }
  file->ready=1;
  return 0;
}

/* Standalone HTML, from full ROM. (sync)
 */
 
int build_standalone(struct builder *builder,struct builder_file *file) {
  struct builder_file *rom=builder_file_req_with_hint(file,BUILDER_FILE_HINT_FULLROM);
  if (!rom) {
    fprintf(stderr,"%s: Expected ROM among prereqs.\n",file->path);
    return -2;
  }
  struct sr_encoder dst={0};
  void *src=0;
  int srcc=file_read(&src,rom->path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",rom->path);
    return -2;
  }
  struct eggdev_convert_context ctx={
    .dst=&dst,
    .src=src,
    .srcc=srcc,
    .refname=rom->path,
  };
  int err=eggdev_html_from_egg(&ctx);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error wrapping in HTML.\n",rom->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  err=file_write(file->path,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file.\n",file->path);
    return -2;
  }
  file->ready=1;
  return 0;
}

/* Separate web bundle, zipping the HTML and ROM. (sync)
 */
 
int build_separate(struct builder *builder,struct builder_file *file) {
  struct builder_file *rom=builder_file_req_with_hint(file,BUILDER_FILE_HINT_FULLROM);
  if (!rom) {
    fprintf(stderr,"%s: Expected ROM among prereqs.\n",file->path);
    return -2;
  }
  struct sr_encoder dst={0};
  void *src=0;
  int srcc=file_read(&src,rom->path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",rom->path);
    return -2;
  }
  struct eggdev_convert_context ctx={
    .dst=&dst,
    .src=src,
    .srcc=srcc,
    .refname=rom->path,
  };
  int err=eggdev_zip_from_egg(&ctx);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error packing Zip archive.\n",rom->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  err=file_write(file->path,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file.\n",file->path);
    return -2;
  }
  file->ready=1;
  return 0;
}

/* Link an executable or Wasm module. (async)
 */
 
int builder_schedule_link(struct builder *builder,struct builder_step *step) {
  struct builder_file *file=step->file;
  struct builder_target *target=file->target;
  if (!target) {
    fprintf(stderr,"%s: Trying to link file but we didn't record its target.\n",file->path);
    return -2;
  }
  if (!target->ldc) {
    fprintf(stderr,"%s: Target '%.*s' has no linker.\n",file->path,target->namec,target->name);
    return -2;
  }
  struct sr_encoder cmd={0};
  if (sr_encode_fmt(&cmd,"%.*s -o%.*s",target->ldc,target->ld,file->pathc,file->path)<0) { sr_encoder_cleanup(&cmd); return -1; }
  int i=0; for (;i<file->reqc;i++) {
    struct builder_file *req=file->reqv[i];
    if (sr_encode_fmt(&cmd," %.*s",req->pathc,req->path)<0) { sr_encoder_cleanup(&cmd); return -1; }
  }
  if (sr_encode_fmt(&cmd," %.*s",target->ldpostc,target->ldpost)<0) { sr_encoder_cleanup(&cmd); return -1; }
  int err=builder_begin_command(builder,step,cmd.v,cmd.c,0,0);
  sr_encoder_cleanup(&cmd);
  return err;
}

/* Compile a C file. (async)
 */
 
int builder_schedule_compile(struct builder *builder,struct builder_step *step) {
  struct builder_file *ofile=step->file;
  if (!ofile->target) {
    fprintf(stderr,"%s: Trying to compile file but we didn't record its target.\n",ofile->path);
    return -2;
  }
  if (!ofile->target->ccc) {
    fprintf(stderr,"%s: Target '%.*s' has no C compiler.\n",ofile->path,ofile->target->namec,ofile->target->name);
    return -2;
  }
  struct builder_file *cfile=builder_file_req_with_hint(ofile,BUILDER_FILE_HINT_C);
  if (!cfile) {
    fprintf(stderr,"%s: Expected a '.c' prereq\n",ofile->path);
    return -2;
  }
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"%.*s -o%.*s %.*s",ofile->target->ccc,ofile->target->cc,ofile->pathc,ofile->path,cfile->pathc,cfile->path);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  return builder_begin_command(builder,step,cmd,cmdc,0,0);
}

/* Assembly for the rom wrapper.
 */
 
static int builder_generate_datao_assembly(struct sr_encoder *dst,const char *path,int pathc) {
  return sr_encode_fmt(dst,
    ".global _egg_embedded_rom,_egg_embedded_rom_size\n"
    "_egg_embedded_rom:\n"
    ".incbin \"%.*s\"\n"
    "_egg_embedded_rom_size:\n"
    ".int (_egg_embedded_rom_size-_egg_embedded_rom)\n"
  ,pathc,path);
}

/* Generate temporary assembly, and assemble it to get the linkable ROM. (async)
 */
 
int builder_schedule_datao(struct builder *builder,struct builder_step *step) {
  int err;
  struct builder_file *file=step->file;
  struct builder_target *target=file->target;
  if (!target) {
    fprintf(stderr,"%s: Trying to compile file but we didn't record its target.\n",file->path);
    return -2;
  }
  if (!target->ccc) {
    fprintf(stderr,"%s: Target '%.*s' has no C compiler.\n",file->path,target->namec,target->name);
    return -2;
  }
  struct builder_file *romfile=builder_file_req_with_hint(file,BUILDER_FILE_HINT_DATAROM);
  if (!romfile) {
    fprintf(stderr,"%s: Expected an '.egg' prereq\n",file->path);
    return -2;
  }
  struct sr_encoder assembly={0};
  if ((err=builder_generate_datao_assembly(&assembly,romfile->path,romfile->pathc))<0) {
    sr_encoder_cleanup(&assembly);
    return err;
  }
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"%.*s -xassembler-with-cpp -o%.*s -",target->ccc,target->cc,file->pathc,file->path);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) { sr_encoder_cleanup(&assembly); return -1; }
  err=builder_begin_command(builder,step,cmd,cmdc,assembly.v,assembly.c);
  sr_encoder_cleanup(&assembly);
  return err;
}
