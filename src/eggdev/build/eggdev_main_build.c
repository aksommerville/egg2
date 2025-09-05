#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Build, main entry point.
 */
 
int eggdev_main_build() {
  int err;
  const char *root;
  if (g.srcpathc==0) root=".";
  else if (g.srcpathc==1) root=g.srcpathv[0];
  else {
    fprintf(stderr,"%s: Too many inputs.\n",g.exename);
    return -2;
  }
  if ((err=eggdev_client_set_root(root,-1))<0) return err;
  struct builder builder={0};
  if ((err=builder_set_root(&builder,root,-1))<0) {
    builder_cleanup(&builder);
    return err;
  }
  err=builder_main(&builder);
  builder_cleanup(&builder);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error building project.\n",root);
    return -2;
  }
  return 0;
}

/* Run, after build succeeded.
 * We don't fork() etc like builder, we just call system() with a shell command.
 */
 
static int eggdev_run_post_build(
  struct builder *builder,
  const char *native_name,int native_namec,
  const char *packaging,int packagingc
) {

  /* Locate the builder's file record for the executable.
   * There has to be one.
   */
  struct builder_file *exefile=0;
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *file=builder->filev[i];
    if (file->hint!=BUILDER_FILE_HINT_EXE) continue;
    if (!file->target) continue;
    if (file->target->namec!=native_namec) continue;
    if (memcmp(file->target->name,native_name,native_namec)) continue;
    exefile=file;
    break;
  }
  if (!exefile) {
    fprintf(stderr,"%.*s: Failed to locate executable for target '%.*s'.\n",builder->rootc,builder->root,native_namec,native_name);
    return -2;
  }
  
  /* For the usual "exe" packaging, nice and simple.
   */
  int err=-1;
  if ((packagingc==3)&&!memcmp(packaging,"exe",3)) {
    err=system(exefile->path);
    
  /* With "macos" packaging, we need to extract the bundle name and `open` it.
   * Launching the executable directly will not work.
   */
  } else if ((packagingc==5)&&!memcmp(packaging,"macos",5)) {
    const char *path=exefile->path;
    int pathc=exefile->pathc;
    while ((pathc>=5)&&memcmp(path+pathc-5,".app/",5)) pathc--;
    if (pathc<5) {
      fprintf(stderr,
        "%.*s: Executable for target '%.*s' does not appear to be in a MacOS app bundle, despite declared 'macos' packaging.\n",
        exefile->pathc,exefile->path,native_namec,native_name
      );
      return -2;
    }
    pathc--;
    char cmd[1024];
    // --reopen-tty is something eggrt for MacOS understands, to enable logging.
    // It's crazy but true: In MacOS, GUI apps don't connect to your terminal by default.
    // And if anyone knows a prettier way to get that happening, please tell me!
    int cmdc=snprintf(cmd,sizeof(cmd),"open -W %.*s --args --reopen-tty=$(tty)",pathc,path);
    if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
    err=system(cmd);
    
  } else {
    fprintf(stderr,
      "%s: Unknown packaging '%.*s' for target '%.*s'. Expected 'exe' or 'macos'.\n",
      exefile->path,packagingc,packaging,native_namec,native_name
    );
    err=-2;
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%.*s: Error launching game.\n",exefile->pathc,exefile->path);
    return -2;
  }
  return 0;
}

/* Run, main entry point.
 */
 
int eggdev_main_run() {
  int err;
  const char *root;
  if (g.srcpathc==0) root=".";
  else if (g.srcpathc==1) root=g.srcpathv[0];
  else {
    fprintf(stderr,"%s: Too many inputs.\n",g.exename);
    return -2;
  }
  
  /* Acquire the native target's name, and bail if there isn't one.
   * Also bail if the native target's packaging is "web". That makes no sense.
   */
  const char *native_name=0;
  int native_namec=eggdev_config_get(&native_name,"EGG_NATIVE_TARGET",-1);
  if (native_namec<1) {
    fprintf(stderr,"%s: EGG_NATIVE_TARGET unset.\n",g.exename);
    return -2;
  }
  char pkgkey[64];
  int pkgkeyc=snprintf(pkgkey,sizeof(pkgkey),"%.*s_PACKAGING",native_namec,native_name);
  if ((pkgkeyc<1)||(pkgkeyc>sizeof(pkgkey))) return -1;
  const char *packaging=0;
  int packagingc=eggdev_config_get(&packaging,pkgkey,pkgkeyc);
  if (packagingc<1) {
    fprintf(stderr,"%s: %.*s unset.\n",g.exename,pkgkeyc,pkgkey);
    return -2;
  } else if ((packagingc==3)&&!memcmp(packaging,"web",3)) {
    fprintf(stderr,"%s: Running targets with 'web' packaging is not possible thru eggdev. Build, then open in your browser.\n",g.exename);
    return -2;
  }
  
  /* Build, exactly the same as eggdev_main_build().
   */
  if ((err=eggdev_client_set_root(root,-1))<0) return err;
  struct builder builder={0};
  if ((err=builder_set_root(&builder,root,-1))<0) {
    builder_cleanup(&builder);
    return err;
  }
  err=builder_main(&builder);
  if (err<0) {
    builder_cleanup(&builder);
    if (err!=-2) fprintf(stderr,"%s: Unspecified error building project.\n",root);
    return -2;
  }
  
  err=eggdev_run_post_build(&builder,native_name,native_namec,packaging,packagingc);
  builder_cleanup(&builder);
  return err;
}
