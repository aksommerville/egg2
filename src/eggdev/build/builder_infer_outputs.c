#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Slice out the portion of a C file's path between "/src/" and the final dot.
 */
 
static int builder_get_cfile_stem(void *dstpp,const char *src,int srcc) {
  int dotp=-1,slashp=-1;
  int i=srcc; while (i-->0) {
    if ((src[i]=='.')&&(dotp<0)&&(slashp<0)) dotp=i;
    else if (src[i]=='/') {
      if ((i>=4)&&!memcmp(src+i-4,"/src",4)) {
        slashp=i;
        break;
      }
    }
  }
  if (dotp>=0) srcc=dotp;
  if (slashp>=0) {
    src+=slashp+1;
    srcc-=slashp+1;
  }
  *(const void**)dstpp=src;
  return srcc;
}

/* Apply rules from a per-object makefile.
 */
 
static int builder_add_dfile_text(struct builder *builder,struct builder_file *ofile,const char *src,int srcc) {
  int srcp=0;
  // First skip to beyond the first colon.
  while ((srcp<srcc)&&(src[srcp++]!=':')) ;
  // Then each whitespace-delimited token is a prereq. Ignore "\" and stop on ";".
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if ((tokenc==1)&&(token[0]=='\\')) continue;
    if ((tokenc==1)&&(token[0]==';')) break;
    
    /* When gcc generates Makefiles, it strips the leading "./" which we depend on.
     * Restore it.
     */
    char mangle[1024];
    if (
      (builder->rootc==1)&&
      (builder->root[0]=='.')&&
      (tokenc>=4)&&
      !memcmp(token,"src/",4)
    ) {
      int manglec=2+tokenc;
      if (manglec>=sizeof(mangle)) continue;
      memcpy(mangle,"./",2);
      memcpy(mangle+2,token,tokenc);
      token=mangle;
      tokenc+=2;
    }
    
    /* When an include path contains ".." entries, gcc emits those verbatim.
     * We need them canonicalized.
     */
    char mangle2[1024];
    int m2p=0,tp=0;
    while (tp<tokenc) {
      if (token[tp]=='/') {
        if (m2p>=sizeof(mangle2)) { m2p=0; break; }
        if (m2p&&(mangle2[m2p-1]!='/')) {
          mangle2[m2p++]='/';
        }
        tp++;
      } else {
        const char *sub=token+tp;
        int subc=0;
        while ((tp<tokenc)&&(token[tp]!='/')) { tp++; subc++; }
        if ((subc==2)&&(sub[0]=='.')&&(sub[1]=='.')) {
          while (m2p&&(mangle2[m2p-1]=='/')) m2p--;
          while (m2p&&(mangle2[m2p-1]!='/')) m2p--;
          while (m2p&&(mangle2[m2p-1]=='/')) m2p--;
        } else {
          if (m2p>sizeof(mangle2)-subc) { m2p=0; break; }
          memcpy(mangle2+m2p,sub,subc);
          m2p+=subc;
        }
      }
    }
    if (!m2p||(m2p>=sizeof(mangle2))) continue;
    mangle2[m2p]=0;
    token=mangle2;
    tokenc=m2p;
    
    if (tokenc<builder->rootc+5) continue;
    if (memcmp(token,builder->root,builder->rootc)) continue;
    if (memcmp(token+builder->rootc,"/src/",5)) continue;
    
    struct builder_file *req=builder_find_file(builder,token,tokenc);
    if (!req) continue;
    if (builder_file_add_req(ofile,req)<0) return -1;
  }
  return 0;
}

/* If a ".d" file exists for this ".o" file, read it and add any prereqs that belong to this project.
 * Prereqs that live anywhere else, assume they don't change and don't need to be tracked.
 */
 
static int builder_add_dfile(struct builder *builder,struct builder_file *ofile) {
  char path[1024];
  if (ofile->pathc>=sizeof(path)) return 0;
  memcpy(path,ofile->path,ofile->pathc+1);
  path[ofile->pathc-1]='d';
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return 0;
  int err=builder_add_dfile_text(builder,ofile,src,srcc);
  free(src);
  return err;
}

/* Infer outputs for a C file.
 */
 
static int builder_infer_outputs_c(struct builder *builder,struct builder_file *cfile) {
  int i=builder->targetc;
  struct builder_target *target=builder->targetv;
  for (;i-->0;target++) {
    const char *stem=0;
    int stemc=builder_get_cfile_stem(&stem,cfile->path,cfile->pathc);
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%.*s/mid/%.*s/%.*s.o",builder->rootc,builder->root,target->namec,target->name,stemc,stem);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    struct builder_file *ofile=builder_add_file(builder,path,pathc);
    if (!ofile) return -1;
    ofile->target=target;
    ofile->hint=BUILDER_FILE_HINT_OBJ;
    if (builder_file_add_req(ofile,cfile)<0) return -1;
    if (builder_add_dfile(builder,ofile)<0) return -1;
  }
  return 0;
}

/* Add the separate HTML template as a prereq for the appropriate outputs.
 * This isn't strictly necessary, but it does force a rebuild when the templates change.
 */

static int builder_add_separate_html_source(struct builder *builder,struct builder_file *output) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/out/separate.html",g.sdkpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *file=builder_add_file(builder,path,pathc);
  if (!file) return -1;
  file->ready=1; // eggdev will not build anything under egg itself
  if (builder_file_add_req(output,file)<0) return -1;
  return 0;
}

/* Singleton outputs for "web" packaging.
 */
 
static int builder_infer_target_outputs_web(struct builder *builder,struct builder_target *target,struct builder_file *datarom) {

  // code.wasm, aka code:1.
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/mid/%.*s/code.wasm",builder->rootc,builder->root,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *wasmfile=builder_add_file(builder,path,pathc);
  if (!wasmfile) return -1;
  wasmfile->target=target;
  wasmfile->hint=BUILDER_FILE_HINT_CODE1;
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *ofile=builder->filev[i];
    if (ofile->hint!=BUILDER_FILE_HINT_OBJ) continue;
    if (ofile->target!=target) continue;
    if (builder_file_add_req(wasmfile,ofile)<0) return -1;
  }
  
  // code:1 also depends on libeggrt-headless.
  pathc=snprintf(path,sizeof(path),"%s/out/%.*s/libeggrt-headless.a",g.sdkpath,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *libfile=builder_add_file(builder,path,pathc);
  if (!libfile) return -1;
  libfile->target=target;
  libfile->hint=0;
  libfile->ready=1;
  if (builder_file_add_req(wasmfile,libfile)<0) return -1;
  
  // A proper Egg ROM. This is portable and everything, so I'm putting under out instead of mid.
  pathc=snprintf(path,sizeof(path),"%.*s/out/%.*s-%.*s.egg",builder->rootc,builder->root,builder->projnamec,builder->projname,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *romfile=builder_add_file(builder,path,pathc);
  if (!romfile) return -1;
  romfile->target=target;
  romfile->hint=BUILDER_FILE_HINT_FULLROM;
  if (builder_file_add_req(romfile,datarom)<0) return -1;
  if (builder_file_add_req(romfile,wasmfile)<0) return -1;
  
  // Aside the ROM, we also need the precompiled synthesizer. This lives in the SDK.
  // eggdev_zip_from_egg() does the actual reading of it, and finds it on its own. We list here only to ensure the zip rebuilds when dirty.
  pathc=snprintf(path,sizeof(path),"%s/out/%.*s/synth.wasm",g.sdkpath,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *synthfile=builder_add_file(builder,path,pathc);
  if (!synthfile) return -1;
  synthfile->target=target;
  synthfile->hint=BUILDER_FILE_HINT_SYNTH_WASM;
  synthfile->ready=1;
  
  // Separate HTML in a Zip archive.
  pathc=snprintf(path,sizeof(path),"%.*s/out/%.*s-%.*s.zip",builder->rootc,builder->root,builder->projnamec,builder->projname,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *separate=builder_add_file(builder,path,pathc);
  if (!separate) return -1;
  separate->target=target;
  separate->hint=BUILDER_FILE_HINT_SEPARATE;
  if (builder_file_add_req(separate,romfile)<0) return -1;
  if (builder_file_add_req(separate,synthfile)<0) return -1;
  if (builder_add_separate_html_source(builder,separate)<0) return -1;
  
  return 0;
}

/* Singleton outputs for "exe" packaging.
 */
 
static int builder_infer_target_outputs_exe(struct builder *builder,struct builder_target *target,struct builder_file *datarom) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/mid/%.*s/data.o",builder->rootc,builder->root,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *datao=builder_add_file(builder,path,pathc);
  if (!datao) return -1;
  datao->target=target;
  datao->hint=BUILDER_FILE_HINT_DATAO;
  if (builder_file_add_req(datao,datarom)<0) return -1;
  
  pathc=snprintf(path,sizeof(path),"%.*s/out/%.*s-%.*s%.*s",builder->rootc,builder->root,builder->projnamec,builder->projname,target->namec,target->name,target->exesfxc,target->exesfx);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *exefile=builder_add_file(builder,path,pathc);
  if (!exefile) return -1;
  exefile->target=target;
  exefile->hint=BUILDER_FILE_HINT_EXE;
  if (builder_file_add_req(exefile,datao)<0) return -1;
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *ofile=builder->filev[i];
    if (ofile->hint!=BUILDER_FILE_HINT_OBJ) continue;
    if (ofile->target!=target) continue;
    if (builder_file_add_req(exefile,ofile)<0) return -1;
  }
  
  // Add libeggrt as the final prereq.
  pathc=snprintf(path,sizeof(path),"%s/out/%.*s/libeggrt.a",g.sdkpath,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *libfile=builder_add_file(builder,path,pathc);
  if (!libfile) return -1;
  libfile->target=target;
  libfile->hint=0; // "external; don't build", we don't have or need a hint for that.
  libfile->ready=1;
  if (builder_file_add_req(exefile,libfile)<0) return -1;
  
  return 0;
}

/* Singleton outputs for "macos" packaging.
 */
 
static int builder_infer_target_outputs_macos(struct builder *builder,struct builder_target *target,struct builder_file *datarom) {
  char path[1024];
  int pathc;
  char bundle[1024];
  int bundlec=snprintf(bundle,sizeof(bundle),"%.*s/out/%.*s-%.*s.app",builder->rootc,builder->root,builder->projnamec,builder->projname,target->namec,target->name);
  if ((bundlec<1)||(bundlec>=sizeof(bundle))) return -1;
  
  /* DATAO, same as other native targets.
   */
  pathc=snprintf(path,sizeof(path),"%.*s/mid/%.*s/data.o",builder->rootc,builder->root,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *datao=builder_add_file(builder,path,pathc);
  if (!datao) return -1;
  datao->target=target;
  datao->hint=BUILDER_FILE_HINT_DATAO;
  if (builder_file_add_req(datao,datarom)<0) return -1;
  
  /* Straight executable like "exe" packaging, but slightly different formation of the path.
   */
  pathc=snprintf(path,sizeof(path),"%.*s/Contents/MacOS/%.*s",bundlec,bundle,builder->projnamec,builder->projname);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *exefile=builder_add_file(builder,path,pathc);
  if (!exefile) return -1;
  exefile->target=target;
  exefile->hint=BUILDER_FILE_HINT_EXE;
  if (builder_file_add_req(exefile,datao)<0) return -1;
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *ofile=builder->filev[i];
    if (ofile->hint!=BUILDER_FILE_HINT_OBJ) continue;
    if (ofile->target!=target) continue;
    if (builder_file_add_req(exefile,ofile)<0) return -1;
  }
  
  // Add libeggrt as the final prereq.
  pathc=snprintf(path,sizeof(path),"%s/out/%.*s/libeggrt.a",g.sdkpath,target->namec,target->name);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *libfile=builder_add_file(builder,path,pathc);
  if (!libfile) return -1;
  libfile->target=target;
  libfile->hint=0;
  libfile->ready=1;
  if (builder_file_add_req(exefile,libfile)<0) return -1;

  /* BUNDLE/Contents/Resources/appicon.icns
   * The input image files should be a prereq, but they're complicated to determine and not expected to change much.
   * I think it's OK to force the user to delete 'out' if they change these things.
   */
  pathc=snprintf(path,sizeof(path),"%.*s/Contents/Resources/appicon.icns",bundlec,bundle);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *icnsfile=builder_add_file(builder,path,pathc);
  if (!icnsfile) return -1;
  icnsfile->target=target;
  icnsfile->hint=BUILDER_FILE_HINT_MAC_ICNS;

  /* BUNDLE/Contents/Resources/Main.nib
   * We depend on a template in the SDK, and the app's metadata. But like icons, not bothering with prereq check.
   */
  pathc=snprintf(path,sizeof(path),"%.*s/Contents/Resources/Main.nib",bundlec,bundle);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *nibfile=builder_add_file(builder,path,pathc);
  if (!nibfile) return -1;
  nibfile->target=target;
  nibfile->hint=BUILDER_FILE_HINT_MAC_NIB;

  /* BUNDLE/Contents/Info.plist
   * We depend on a template in the SDK, and the app's metadata. But like icons, not bothering with prereq check.
   */
  pathc=snprintf(path,sizeof(path),"%.*s/Contents/Info.plist",bundlec,bundle);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  struct builder_file *plistfile=builder_add_file(builder,path,pathc);
  if (!plistfile) return -1;
  plistfile->target=target;
  plistfile->hint=BUILDER_FILE_HINT_MAC_PLIST;
  
  return 0;
}

/* Infer outputs, main entry point.
 */
 
int builder_infer_outputs(struct builder *builder) {
  int err,i;
  
  /* There's a data-only ROM that we'll build just once, to share for all targets.
   * All data files will get added as prereqs to it.
   */
  struct builder_file *datarom=0;
  {
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%.*s/mid/data.egg",builder->rootc,builder->root);
    if ((pathc<1)||(pathc>=sizeof(path))) return -1;
    if (!(datarom=builder_add_file(builder,path,pathc))) return -1;
    datarom->hint=BUILDER_FILE_HINT_DATAROM;
  }

  /* Add an object file for each C file, and install resource files as reqs of the data rom.
   */
  for (i=builder->filec;i-->0;) {
    struct builder_file *file=builder->filev[i];
    switch (file->hint) {
      case BUILDER_FILE_HINT_RES: if (builder_file_add_req(datarom,file)<0) return err; break;
      case BUILDER_FILE_HINT_C: if ((err=builder_infer_outputs_c(builder,file))<0) return err; break;
    }
  }
  
  /* Add output and intermediate files per target.
   */
  struct builder_target *target=builder->targetv;
  for (i=builder->targetc;i-->0;target++) {
    if ((target->pkgc==3)&&!memcmp(target->pkg,"web",3)) {
      if ((err=builder_infer_target_outputs_web(builder,target,datarom))<0) return err;
    } else if ((target->pkgc==3)&&!memcmp(target->pkg,"exe",3)) {
      if ((err=builder_infer_target_outputs_exe(builder,target,datarom))<0) return err;
    } else if ((target->pkgc==5)&&!memcmp(target->pkg,"macos",5)) {
      if ((err=builder_infer_target_outputs_macos(builder,target,datarom))<0) return err;
    } else {
      fprintf(stderr,
        "%s:WARNING: No output rules for packaging '%.*s' used by target '%.*s' [%s:%d]\n",
        g.exename,target->pkgc,target->pkg,target->namec,target->name,__FILE__,__LINE__
      );
    }
  }
  
  /**
  {
    fprintf(stderr,"%s: Done listing output and intermediate files...\n",__func__);
    for (i=0;i<builder->filec;i++) {
      struct builder_file *file=builder->filev[i];
      fprintf(stderr,"  %s, hint %d, %d prereqs\n",file->path,file->hint,file->reqc);
    }
  }
  /**/
  
  return 0;
}
