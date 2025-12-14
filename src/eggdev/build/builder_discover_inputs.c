#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Ignore some known directories.
 */
 
static int builder_should_ignore_directory(struct builder *builder,const char *path,int pathc) {

  // First off, we only care about "ROOT/src/". Nothing else should land here, but be sure of it.
  if (!path) return 1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  if (pathc<builder->rootc) return 1;
  if (memcmp(builder->root,path,builder->rootc)) return 1;
  path+=builder->rootc;
  pathc-=builder->rootc;
  if ((pathc<5)||memcmp(path,"/src/",5)) return 1;
  path+=5;
  pathc-=5;
  
  // For now I'm only looking at directories immediately under src.
  const char *tld=path;
  int tldc=0;
  while ((tldc<pathc)&&(tld[tldc]!='/')) tldc++;
  if ((tldc==4)&&!memcmp(tld,"tool",4)) return 1;
  
  // Something else? Assume it's interesting.
  return 0;
}

/* Ignore individual files if they match a project-defined glob.
 */
 
static int builder_should_ignore_file(struct builder *builder,const char *path,int pathc) {
  const char *src;
  int srcc=eggdev_client_get_string(&src,"ignoreData",10);
  if (srcc<1) return 0;
  if (!path) return 1;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int srcp=0;
  while (srcp<srcc) {
    const char *pat=src+srcp;
    int patc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) patc++;
    if (sr_pattern_match(pat,patc,path,pathc)) return 1;
  }
  return 0;
}

/* Found a file.
 */
 
static int builder_discover_inputs_cb(const char *path,const char *base,char ftype,void *userdata) {
  struct builder *builder=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') {
    if (builder_should_ignore_directory(builder,path,-1)) return 0;
    return dir_read(path,builder_discover_inputs_cb,builder);
  }
  if (ftype!='f') {
    fprintf(stderr,"%s: Ignoring unexpected source file due to unexpected type '%c'\n",path,ftype);
    return 0;
  }
  if (builder_should_ignore_file(builder,path,-1)) return 0;
  struct builder_file *file=builder_add_file(builder,path,-1);
  if (!file) return -1;
  
  // Nothing under src is permissible for us to touch. They all start "ready".
  file->ready=1;
  
  // Anything under PROJ/src/data/ gets the RES hint.
  if ((file->pathc>=builder->rootc+10)&&!memcmp(file->path+builder->rootc,"/src/data/",10)) {
    file->hint=BUILDER_FILE_HINT_RES;
    
  // Anything ending ".c" gets the C hint.
  } else if ((file->pathc>=2)&&!memcmp(file->path+file->pathc-2,".c",2)) {
    file->hint=BUILDER_FILE_HINT_C;

  // Ditto ".m" for Objective-C. We're loose about "C", it just means "compilable source".
  } else if ((file->pathc>=2)&&!memcmp(file->path+file->pathc-2,".m",2)) {
    file->hint=BUILDER_FILE_HINT_C;
  
  // And finally, we can at least call it INPUT.
  } else {
    file->hint=BUILDER_FILE_HINT_INPUT;
  }
  return 0;
}

/* Discover inputs, main entry point.
 */
 
int builder_discover_inputs(struct builder *builder) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/src",builder->rootc,builder->root);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  int err=dir_read(path,builder_discover_inputs_cb,builder);
  if (err<0) return err;
  return 0;
}
