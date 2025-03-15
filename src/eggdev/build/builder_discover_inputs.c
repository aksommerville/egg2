#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Found a file.
 */
 
static int builder_discover_inputs_cb(const char *path,const char *base,char ftype,void *userdata) {
  struct builder *builder=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return dir_read(path,builder_discover_inputs_cb,builder);
  if (ftype!='f') {
    fprintf(stderr,"%s: Ignoring unexpected source file due to unexpected type '%c'\n",path,ftype);
    return 0;
  }
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
