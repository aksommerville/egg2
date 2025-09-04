#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"
#include "builder.h"

/* Read file, compile resource, and add to ROM.
 */
 
static int eggdev_compile_data_res(struct builder *builder,struct eggdev_rom_writer *writer,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return builder_error(builder,"%s: Failed to read file.\n",path);
  struct sr_encoder dst={0};
  int err=eggdev_convert_for_rom(&dst,src,srcc,0,path,builder->log);
  free(src);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    if (err!=-2) builder_error(builder,"%s: Unspecified error compiling resource.\n",path);
    return -2;
  }
  int tid=0,rid=0;
  if (eggdev_res_ids_from_path(&tid,&rid,path)<0) {
    builder_error(builder,"%s: Failed to extract resource IDs from path.\n",path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  int resp=eggdev_rom_writer_search(writer,tid,rid);
  if (resp>=0) {
    builder_error(builder,"%s: Duplicate resource %d:%d\n",path,tid,rid);
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
 
static int eggdev_compile_data_rom(struct builder *builder,struct eggdev_rom_writer *writer,const char *path) {
  void *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return builder_error(builder,"%s: Failed to read file.\n",path);
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,src,srcc)<0) {
    free(src);
    return builder_error(builder,"%s: Malformed ROM.\n",path);
  }
  struct eggdev_res res;
  int err;
  while ((err=eggdev_rom_reader_next(&res,&reader))>0) {
    int p=eggdev_rom_writer_search(writer,res.tid,res.rid);
    if (p>=0) {
      free(src);
      return builder_error(builder,"%s: Duplicate resource %d:%d\n",path,res.tid,res.rid);
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
    int err=eggdev_compile_data_res(builder,&writer,req->path);
    if (err<0) {
      if (err!=-2) builder_error(builder,"%s: Unspecified error adding to ROM.\n",req->path);
      eggdev_rom_writer_cleanup(&writer);
      return -2;
    }
  }
  struct sr_encoder serial={0};
  int err=eggdev_rom_writer_encode(&serial,&writer);
  eggdev_rom_writer_cleanup(&writer);
  if (err<0) {
    if (err!=-2) builder_error(builder,"%s: Unspecified error encoding ROM.\n",file->path);
    sr_encoder_cleanup(&serial);
    return -2;
  }
  err=file_write(file->path,serial.v,serial.c);
  sr_encoder_cleanup(&serial);
  if (err<0) return builder_error(builder,"%s: Failed to write file\n",file->path);
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
      err=eggdev_compile_data_res(builder,&writer,req->path);
    } else if (req->hint==BUILDER_FILE_HINT_DATAROM) {
      err=eggdev_compile_data_rom(builder,&writer,req->path);
    }
    if (err<0) {
      if (err!=-2) builder_error(builder,"%s: Unspecified error merging ROMs.\n",req->path);
      eggdev_rom_writer_cleanup(&writer);
      return -2;
    }
  }
  struct sr_encoder serial={0};
  int err=eggdev_rom_writer_encode(&serial,&writer);
  eggdev_rom_writer_cleanup(&writer);
  if (err<0) {
    if (err!=-2) builder_error(builder,"%s: Unspecified error encoding ROM.\n",file->path);
    sr_encoder_cleanup(&serial);
    return -2;
  }
  err=file_write(file->path,serial.v,serial.c);
  sr_encoder_cleanup(&serial);
  if (err<0) return builder_error(builder,"%s: Failed to write file\n",file->path);
  file->ready=1;
  return 0;
}

/* Standalone HTML, from full ROM. (sync)
 */
 
int build_standalone(struct builder *builder,struct builder_file *file) {
  struct builder_file *rom=builder_file_req_with_hint(file,BUILDER_FILE_HINT_FULLROM);
  if (!rom) return builder_error(builder,"%s: Expected ROM among prereqs.\n",file->path);
  struct sr_encoder dst={0};
  void *src=0;
  int srcc=file_read(&src,rom->path);
  if (srcc<0) return builder_error(builder,"%s: Failed to read file.\n",rom->path);
  struct eggdev_convert_context ctx={
    .dst=&dst,
    .src=src,
    .srcc=srcc,
    .refname=rom->path,
  };
  int err=eggdev_html_from_egg(&ctx);
  free(src);
  if (err<0) {
    if (err!=-2) builder_error(builder,"%s: Unspecified error wrapping in HTML.\n",rom->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  err=file_write(file->path,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) return builder_error(builder,"%s: Failed to write file.\n",file->path);
  file->ready=1;
  return 0;
}

/* Separate web bundle, zipping the HTML and ROM. (sync)
 */
 
int build_separate(struct builder *builder,struct builder_file *file) {
  struct builder_file *rom=builder_file_req_with_hint(file,BUILDER_FILE_HINT_FULLROM);
  if (!rom) return builder_error(builder,"%s: Expected ROM among prereqs.\n",file->path);
  struct sr_encoder dst={0};
  void *src=0;
  int srcc=file_read(&src,rom->path);
  if (srcc<0) return builder_error(builder,"%s: Failed to read file.\n",rom->path);
  struct eggdev_convert_context ctx={
    .dst=&dst,
    .src=src,
    .srcc=srcc,
    .refname=rom->path,
  };
  int err=eggdev_zip_from_egg(&ctx);
  free(src);
  if (err<0) {
    if (err!=-2) builder_error(builder,"%s: Unspecified error packing Zip archive.\n",rom->path);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  err=file_write(file->path,dst.v,dst.c);
  sr_encoder_cleanup(&dst);
  if (err<0) return builder_error(builder,"%s: Failed to write file.\n",file->path);
  file->ready=1;
  return 0;
}

/* Link an executable or Wasm module. (async)
 */
 
int builder_schedule_link(struct builder *builder,struct builder_step *step) {
  struct builder_file *file=step->file;
  struct builder_target *target=file->target;
  if (!target) return builder_error(builder,"%s: Trying to link file but we didn't record its target.\n",file->path);
  if (!target->ldc) return builder_error(builder,"%s: Target '%.*s' has no linker.\n",file->path,target->namec,target->name);
  struct sr_encoder cmd={0};
  if (sr_encode_fmt(&cmd,"%.*s -o%.*s",target->ldc,target->ld,file->pathc,file->path)<0) { sr_encoder_cleanup(&cmd); return -1; }
  int i=0; for (;i<file->reqc;i++) {
    struct builder_file *req=file->reqv[i];
    if (sr_encode_fmt(&cmd," %.*s",req->pathc,req->path)<0) { sr_encoder_cleanup(&cmd); return -1; }
  }
  if ((target->pkgc==3)&&!memcmp(target->pkg,"web",3)) {
    // Web builds don't link against a runtime -- their runtime is in javascript, at a higher level.
    if (sr_encode_fmt(&cmd," %s/out/%.*s/libeggrt-headless.a",g.sdkpath,target->namec,target->name)<0) { sr_encoder_cleanup(&cmd); return -1; }
  } else {
    // Everything else requires libeggrt.
    if (sr_encode_fmt(&cmd," %s/out/%.*s/libeggrt.a",g.sdkpath,target->namec,target->name)<0) { sr_encoder_cleanup(&cmd); return -1; }
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
  if (!ofile->target) return builder_error(builder,"%s: Trying to compile file but we didn't record its target.\n",ofile->path);
  if (!ofile->target->ccc) return builder_error(builder,"%s: Target '%.*s' has no C compiler.\n",ofile->path,ofile->target->namec,ofile->target->name);
  struct builder_file *cfile=builder_file_req_with_hint(ofile,BUILDER_FILE_HINT_C);
  if (!cfile) return builder_error(builder,"%s: Expected a '.c' prereq\n",ofile->path);
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),
    "%.*s -I%.*s/mid -I%s/src -o%.*s %.*s",
    ofile->target->ccc,ofile->target->cc,
    builder->rootc,builder->root,
    g.sdkpath,
    ofile->pathc,ofile->path,
    cfile->pathc,cfile->path
  );
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  return builder_begin_command(builder,step,cmd,cmdc,0,0);
}

/* Assembly for the rom wrapper.
 */
 
static int builder_generate_datao_assembly(struct sr_encoder *dst,const char *path,int pathc) {
  /* TODO 2025-09-04: Under MacOS, referring to '_egg_embedded_rom', one underscore, causes an actual link aagainst '__egg_embedded_rom', two underscores.
   * I need to understand why this happens, then maybe replace this bit with something more robust.
   * It's different under Linux; there we use the same single-underscore name everywhere.
   * For now, it works to use an extra underscore here in the assembly file.
   */
  #if USE_ismac
    return sr_encode_fmt(dst,
      ".global __egg_embedded_rom,__egg_embedded_rom_size\n"
      "__egg_embedded_rom:\n"
      ".incbin \"%.*s\"\n"
      "__egg_embedded_rom_size:\n"
      ".int (__egg_embedded_rom_size-__egg_embedded_rom)\n"
    ,pathc,path);
  #endif
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
  if (!target) return builder_error(builder,"%s: Trying to compile file but we didn't record its target.\n",file->path);
  if (!target->ccc) return builder_error(builder,"%s: Target '%.*s' has no C compiler.\n",file->path,target->namec,target->name);
  struct builder_file *romfile=builder_file_req_with_hint(file,BUILDER_FILE_HINT_DATAROM);
  if (!romfile) return builder_error(builder,"%s: Expected an '.egg' prereq\n",file->path);
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
