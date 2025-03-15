#include "eggdev/eggdev_internal.h"
#include "builder.h"
#include <sys/stat.h>

/* Cleanup.
 */
 
void builder_file_del(struct builder_file *file) {
  if (!file) return;
  if (file->path) free(file->path);
  if (file->reqv) free(file->reqv);
  free(file);
}

/* New.
 */
 
struct builder_file *builder_file_new() {
  struct builder_file *file=calloc(1,sizeof(struct builder_file));
  if (!file) return 0;
  return file;
}

/* Set path.
 */

int builder_file_set_path(struct builder_file *file,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (file->path) free(file->path);
  file->path=nv;
  file->pathc=srcc;
  
  file->mtime=0;
  if (file->path) {
    struct stat st={0};
    if (stat(file->path,&st)>=0) {
      file->mtime=st.st_mtime;
    }
  }
  
  return 0;
}

/* Add prerequisite.
 */
 
int builder_file_add_req(struct builder_file *file,struct builder_file *req) {
  if (!file||!req) return -1;
  if (builder_file_depends_on(req,file)) {
    fprintf(stderr,"%s: Circular dependency involving %s\n",file->path,req->path);
    return -1;
  }
  if (builder_file_has_req(file,req)) return 0;
  if (file->reqc>=file->reqa) {
    int na=file->reqa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(file->reqv,sizeof(void*)*na);
    if (!nv) return -1;
    file->reqv=nv;
    file->reqa=na;
  }
  file->reqv[file->reqc++]=req;
  return 0;
}

/* Check direct dependency.
 */
 
int builder_file_has_req(const struct builder_file *file,const struct builder_file *req) {
  if (!file||!req) return 0;
  int i=file->reqc;
  while (i-->0) if (file->reqv[i]==req) return 1;
  return 0;
}

/* Check indirect dependency.
 */

int builder_file_depends_on(const struct builder_file *product,const struct builder_file *input) {
  if (!product||!input) return 0;
  if (product==input) return 1;
  int i=product->reqc;
  while (i-->0) if (builder_file_depends_on(product->reqv[i],input)) return 1;
  return 0;
}
