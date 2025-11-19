#include "eggdev/eggdev_internal.h"
#include <unistd.h>
#include <time.h>

#define FB_LIMIT 4096

/* Context.
 */
 
struct eggdev_project_context {
  char *name; // C identifier
  int namec;
  char *title; // For display
  int titlec;
  char *author;
  int authorc;
  int fbw,fbh;
  struct sr_encoder scratch;
};

static void eggdev_project_context_cleanup(struct eggdev_project_context *ctx) {
  if (ctx->name) free(ctx->name);
  if (ctx->title) free(ctx->title);
  if (ctx->author) free(ctx->author);
  sr_encoder_cleanup(&ctx->scratch);
}

/* Prompt for text into a newly-allocated string.
 */
 
static int eggdev_project_prompt(void *dstpp,struct eggdev_project_context *ctx,const char *prompt) {
  fprintf(stderr,"%s: ",prompt);
  char tmp[256];
  int tmpc=read(STDIN_FILENO,tmp,sizeof(tmp));
  if (tmpc<=0) return -1;
  while (tmpc&&((unsigned char)tmp[tmpc-1]<=0x20)) tmpc--;
  char *dst=malloc(tmpc+1);
  if (!dst) return -1;
  memcpy(dst,tmp,tmpc);
  dst[tmpc]=0;
  *(void**)dstpp=dst;
  return tmpc;
}

/* Validate project name.
 * This also checks whether the file already exists.
 * So (src) must be NUL-terminated, in addition to providing its length.
 */
 
static int eggdev_project_name_validate(const char *src,int srcc) {
  if ((srcc<1)||(srcc>32)||((src[0]>='0')&&(src[0]<='9'))) {
    fprintf(stderr,"%s: Project name must be a C identifier of 1..32 bytes.\n",g.exename);
    return -2;
  }
  const char *q=src;
  int i=srcc;
  for (;i-->0;q++) {
    if ((*q>='a')&&(*q<='z')) continue;
    if ((*q>='A')&&(*q<='Z')) continue;
    if ((*q>='0')&&(*q<='9')) continue;
    if (*q=='_') continue;
    fprintf(stderr,"%s: Project name must be a C identifier of 1..32 bytes.\n",g.exename);
    return -2;
  }
  char ftype=file_get_type(src);
  if (ftype) {
    fprintf(stderr,"%.*s: File already exists.\n",srcc,src);
    return -2;
  }
  return 0;
}

/* Parse and validate framebuffer dimensions.
 */
 
static int eggdev_project_parse_fb(struct eggdev_project_context *ctx,const char *src,int srcc) {
  int sepp=0;
  for (;sepp<srcc;sepp++) {
    if (src[sepp]=='x') {
      if (
        (sr_int_eval(&ctx->fbw,src,sepp)<2)||(ctx->fbw<1)||(ctx->fbw>FB_LIMIT)||
        (sr_int_eval(&ctx->fbh,src+sepp+1,srcc-sepp-1)<2)||(ctx->fbh<1)||(ctx->fbh>FB_LIMIT)
      ) break;
      return 0;
    }
  }
  fprintf(stderr,"%s: Invalid framebuffer dimensions '%.*s'. Limit %d per axis.\n",g.exename,srcc,src,FB_LIMIT);
  return -2;
}

/* Make a file or directory under the context's root.
 */
 
static int eggdev_project_mkdir(struct eggdev_project_context *ctx,const char *subpath) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/%s",ctx->namec,ctx->name,subpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (dir_mkdir(path)<0) {
    fprintf(stderr,"%s: mkdir failed\n",path);
    return -2;
  }
  return 0;
}

static int eggdev_project_write(struct eggdev_project_context *ctx,const char *subpath,const void *src,int srcc) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/%s",ctx->namec,ctx->name,subpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (file_write(path,src,srcc)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",path,srcc);
    return -2;
  }
  return 0;
}

static int eggdev_project_copy(struct eggdev_project_context *ctx,const char *srcsub,const char *dstsub) {
  char srcpath[1024];
  int srcpathc=snprintf(srcpath,sizeof(srcpath),"%s/%s",g.sdkpath,srcsub);
  if ((srcpathc<1)||(srcpathc>=sizeof(srcpath))) return -1;
  char dstpath[1024];
  int dstpathc=snprintf(dstpath,sizeof(dstpath),"%.*s/%s",ctx->namec,ctx->name,dstsub);
  if ((dstpathc<1)||(dstpathc>=sizeof(dstpath))) return -1;
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",srcpath);
    return -2;
  }
  int err=file_write(dstpath,src,srcc);
  free(src);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes.\n",dstpath,srcc);
    return -2;
  }
  return 0;
}

/* Generate the boilerplate .gitignore.
 */
 
static int gen_gitignore(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "*~\n"
    "~*\n"
    "mid\n"
    "out\n"
    ".DS_Store\n"
  ,-1)<0) return -1;
  return eggdev_project_write(ctx,".gitignore",ctx->scratch.v,ctx->scratch.c);
}

/* Generate the boilerplate Makefile.
 * It will assume that eggdev is on the user's path.
 */
 
static int gen_makefile(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "all:\n"
    ".SILENT:\n"
    "\n"
    "ifeq (,$(EGG2_SDK))\n"
    "  EGG2_SDK:=../egg2\n"
    "endif\n"
    "EGGDEV:=$(EGG2_SDK)/out/eggdev\n"
    "\n"
    "all:;$(EGGDEV) build\n"
    "clean:;rm -rf mid out\n"
    "run:;$(EGGDEV) run\n"
  ,-1)<0) return -1;
  
  if (sr_encode_fmt(&ctx->scratch,
    "web-run:all;$(EGGDEV) serve --htdocs=out/%.*s-web.zip --project=.\n",
    ctx->namec,ctx->name
  )<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,
    "edit:;$(EGGDEV) serve \\\n"
    "  --htdocs=/data:src/data \\\n"
    "  --htdocs=EGG_SDK/src/web \\\n"
    "  --htdocs=EGG_SDK/src/editor \\\n"
    "  --htdocs=src/editor \\\n"
    "  --htdocs=/synth.wasm:EGG_SDK/out/web/synth.wasm \\n"
    "  --htdocs=/build:out/%.*s-web.zip \\\n"
    "  --htdocs=/out:out \\\n"
    "  --writeable=src/data \\\n"
    "  --project=.\n"
  ,ctx->namec,ctx->name)<0) return -1;
  
  return eggdev_project_write(ctx,"Makefile",ctx->scratch.v,ctx->scratch.c);
}

/* Generate metadata:1.
 */
 
static int gen_metadata(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  int year=0,month=0,day=0;
  time_t t=time(0);
  struct tm tm={0};
  if (localtime_r(&t,&tm)==&tm) {
    year=1900+tm.tm_year;
    month=1+tm.tm_mon;
    day=tm.tm_mday;
  }
  if (sr_encode_fmt(&ctx->scratch,"fb=%dx%d\n",ctx->fbw,ctx->fbh)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"players=1\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"lang=en\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"title=%.*s\n",ctx->titlec,ctx->title)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"title$=2\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"author=%.*s\n",ctx->authorc,ctx->author)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"copyright=(c) %d %.*s\n",year,ctx->authorc,ctx->author)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"iconImage=1\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"version=0.0.1\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"time=%04d-%02d-%02d\n",year,month,day)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#freedom=free|intact|limited|restricted\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#posterImage=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#desc=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#advisory=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#rating=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#required=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#optional=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#genre=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#tags=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#contact=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#homepage=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#source=\n")<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#persistKey=\n")<0) return -1;
  return eggdev_project_write(ctx,"src/data/metadata",ctx->scratch.v,ctx->scratch.c);
}

/* Generate strings:en-1.
 * We only generate a file for English, and we assume that our prompts were answered in English.
 * (Because the questions were posed in English. I don't think that's a lot to assume.)
 */
 
static int gen_strings(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_fmt(&ctx->scratch,"1 English\n")<0) return -1; // strings:1:1 is always the language's own name, by convention.
  if (sr_encode_fmt(&ctx->scratch,"2 %.*s\n",ctx->titlec,ctx->title)<0) return -1;
  return eggdev_project_write(ctx,"src/data/strings/en-1",ctx->scratch.v,ctx->scratch.c);
}

/* Generate shared_symbols.h.
 */
 
static int gen_shared_symbols(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "/* shared_symbols.h\n"
    " * This file is consumed by eggdev and editor, in addition to compiling in with the game.\n"
    " */\n"
    "\n"
    "#ifndef SHARED_SYMBOLS_H\n"
    "#define SHARED_SYMBOLS_H\n"
    "\n"
    "#define NS_sys_tilesize 16\n"
    "// Define (mapw,maph) if you're using fixed-size maps.\n"
    "//#define NS_sys_mapw 20\n"
    "//#define NS_sys_maph 11\n"
    "#define NS_sys_bgcolor 0x000000\n"
    "#define EGGDEV_ignoreData \"\" /* Comma-delimited glob patterns for editor to ignore under src/data/ */\n"
    "\n"
    "#define CMD_map_image     0x20 /* u16:imageid */\n"
    "// 'position' or 'neighbors' (or neither). Not both.\n"
    "//#define CMD_map_position  0x21 /* u8:horz, u8:vert */\n"
    "//#define CMD_map_neighbors 0x60 /* u16:west, u16:east, u16:north, u16:south */\n"
    "#define CMD_map_sprite    0x61 /* u16:position, u16:spriteid, u32:arg */\n"
    "#define CMD_map_door      0x62 /* u16:position, u16:mapid, u16:dstposition, u16:arg */\n"
    "\n"
    "#define CMD_sprite_image 0x20 /* u16:imageid */\n"
    "#define CMD_sprite_tile  0x21 /* u8:tileid, u8:xform */\n"
    "#define CMD_sprite_type  0x22 /* u16:sprtype */\n"
    "\n"
    "#define NS_tilesheet_physics 1\n"
    "#define NS_tilesheet_family 0\n"
    "#define NS_tilesheet_neighbors 0\n"
    "#define NS_tilesheet_weight 0\n"
    "\n"
    "#define NS_physics_vacant 0\n"
    "#define NS_physics_solid 1\n"
    "\n"
    "// Editor uses the comment after a 'sprtype' symbol as a prompt in the new-sprite modal.\n"
    "// Should match everything after 'spriteid' in the CMD_map_sprite args.\n"
    "#define NS_sprtype_dummy 0 /* (u32)0 */\n"
    "#define FOR_EACH_SPRTYPE \\\n"
    "  _(dummy)\n"
    "\n"
    "#endif\n"
  ,-1)<0) return -1;
  return eggdev_project_write(ctx,"src/game/shared_symbols.h",ctx->scratch.v,ctx->scratch.c);
}

/* Generate the game's central header.
 */
 
static int gen_header(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_raw(&ctx->scratch,
    "#ifndef EGG_GAME_MAIN_H\n"
    "#define EGG_GAME_MAIN_H\n"
    "\n"
    "#include \"egg/egg.h\"\n"
    "#include \"opt/stdlib/egg-stdlib.h\"\n"
    "#include \"opt/graf/graf.h\"\n"
    "#include \"egg_res_toc.h\"\n"
    "#include \"shared_symbols.h\"\n"
    "\n"
  ,-1)<0) return -1;
  if (sr_encode_fmt(&ctx->scratch,"#define FBW %d\n#define FBH %d\n",ctx->fbw,ctx->fbh)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,
    "\n"
    "extern struct g {\n"
    "  void *rom;\n"
    "  int romc;\n"
    "  struct graf graf;\n"
    "} g;\n"
    "\n"
    "#endif\n"
  ,-1)<0) return -1;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/src/game/%.*s.h",ctx->namec,ctx->name,ctx->namec,ctx->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  return file_write(path,ctx->scratch.v,ctx->scratch.c);
}

/* Generate main.c.
 */
 
static int gen_main(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (sr_encode_fmt(&ctx->scratch,"#include \"%.*s.h\"\n",ctx->namec,ctx->name)<0) return -1;
  if (sr_encode_raw(&ctx->scratch,
    "\n"
    "struct g g={0};\n"
    "\n"
    "void egg_client_quit(int status) {\n"
    "}\n"
    "\n"
    "int egg_client_init() {\n"
    "\n"
    "  int fbw=0,fbh=0;\n"
    "  egg_texture_get_size(&fbw,&fbh,1);\n"
    "  if ((fbw!=FBW)||(fbh!=FBH)) {\n"
    "    fprintf(stderr,\"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\\n\",fbw,fbh,FBW,FBH);\n"
    "    return -1;\n"
    "  }\n"
    "\n"
    "  g.romc=egg_rom_get(0,0);\n"
    "  if (!(g.rom=malloc(g.romc))) return -1;\n"
    "  egg_rom_get(g.rom,g.romc);\n"
    "\n"
    "  srand_auto();\n"
    "\n"
    "  //TODO\n" // TODO Initialize other standard client-side utilities?
    "\n"
    "  return 0;\n"
    "}\n"
    "\n"
    "void egg_client_update(double elapsed) {\n"
    "  //TODO\n"
    "}\n"
    "\n"
    "void egg_client_render() {\n"
    "  graf_reset(&g.graf);\n"
    "  //TODO\n"
    "  graf_flush(&g.graf);\n"
    "}\n"
  ,-1)<0) return -1;
  return eggdev_project_write(ctx,"src/game/main.c",ctx->scratch.v,ctx->scratch.c);
}

/* Generate image:1, a 16x16 placeholder icon.
 */
 
static int gen_icon(struct eggdev_project_context *ctx) {
  return eggdev_project_copy(ctx,"etc/skeleton/appicon.png","src/data/image/1-appicon.png");
}

/* Generate the empty editor override files.
 */
 
static int gen_editor(struct eggdev_project_context *ctx) {
  int err;
  if ((err=eggdev_project_copy(ctx,"src/editor/override.css","src/editor/override.css"))<0) return err;
  if ((err=eggdev_project_copy(ctx,"src/editor/Override.js","src/editor/Override.js"))<0) return err;
  return 0;
}

/* Generate a blank GitHub-friendly README.
 */
 
static int gen_readme(struct eggdev_project_context *ctx) {
  ctx->scratch.c=0;
  if (ctx->titlec) {
    if (sr_encode_fmt(&ctx->scratch,"# %.*s\n\n",ctx->titlec,ctx->title)<0) return -1;
  } else {
    if (sr_encode_fmt(&ctx->scratch,"# %.*s\n\n",ctx->namec,ctx->name)<0) return -1;
  }
  if (sr_encode_raw(&ctx->scratch,"Requires [Egg v2](https://github.com/aksommerville/egg2) to build.\n",-1)<0) return -1;
  return eggdev_project_write(ctx,"README.md",ctx->scratch.v,ctx->scratch.c);
}

/* Generate project.
 * All inputs must have been gathered and validated before this.
 */
 
static int eggdev_project_commit(struct eggdev_project_context *ctx) {
  int err;
  
  if ((err=eggdev_project_mkdir(ctx,""))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src"))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src/game"))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src/editor"))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src/data"))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src/data/image"))<0) return err;
  if ((err=eggdev_project_mkdir(ctx,"src/data/strings"))<0) return err;
  
  if ((err=gen_gitignore(ctx))<0) return err;
  if ((err=gen_makefile(ctx))<0) return err;
  if ((err=gen_metadata(ctx))<0) return err;
  if ((err=gen_strings(ctx))<0) return err;
  if ((err=gen_shared_symbols(ctx))<0) return err;
  if ((err=gen_header(ctx))<0) return err;
  if ((err=gen_main(ctx))<0) return err;
  if ((err=gen_icon(ctx))<0) return err;
  if ((err=gen_editor(ctx))<0) return err;
  if ((err=gen_readme(ctx))<0) return err;
  
  return 0;
}

/* Project wizard, main entry point.
 */
 
static int eggdev_main_project_inner(struct eggdev_project_context *ctx) {
  int err;
  
  fprintf(stderr,"*********************************\n");
  fprintf(stderr,"* Egg New Project Wizard\n");
  fprintf(stderr,"*********************************\n");
  
  if ((ctx->namec=eggdev_project_prompt(&ctx->name,ctx,"Project name"))<0) return ctx->namec;
  if ((err=eggdev_project_name_validate(ctx->name,ctx->namec))<0) return err;
  
  if ((ctx->titlec=eggdev_project_prompt(&ctx->title,ctx,"Display name"))<0) return ctx->titlec;
  if ((ctx->authorc=eggdev_project_prompt(&ctx->author,ctx,"Your name"))<0) return ctx->authorc;
  
  char *tmp=0;
  int tmpc=eggdev_project_prompt(&tmp,ctx,"Framebuffer (WIDTHxHEIGHT)");
  if (tmpc<0) return tmpc;
  err=eggdev_project_parse_fb(ctx,tmp,tmpc);
  free(tmp);
  if (err<0) return err;
  
  if ((err=eggdev_project_commit(ctx))<0) {
    dir_rmrf(ctx->name);
    return err;
  }
  
  return 0;
}
 
int eggdev_main_project() {
  struct eggdev_project_context ctx={0};
  int err=eggdev_main_project_inner(&ctx);
  eggdev_project_context_cleanup(&ctx);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error configuring new project.\n",g.exename);
    return -2;
  }
  return 0;
}
