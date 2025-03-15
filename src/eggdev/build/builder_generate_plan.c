#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Recursively plan one file.
 */
 
static int builder_generate_plan_1(struct builder *builder,struct builder_file *file) {
  if (file->ready) return 0;
  if (file->planned) return 0;
  file->planned=1; // Eagerly log it as planned. Shouldn't come up, but this is extra protection against infinite loops.
  
  // Make all my prereqs ready first (and theirs, and theirs...)
  int all_ready=1;
  int i=file->reqc;
  while (i-->0) {
    struct builder_file *req=file->reqv[i];
    int err=builder_generate_plan_1(builder,req);
    if (err<0) return err;
    if (!req->ready) all_ready=0;
    else if (req->mtime>file->mtime) all_ready=0;
  }
  
  // If all reqs are already ready and this file exists, no need to build. Flip it ready.
  if (all_ready) {
    if (file_get_type(file->path)=='f') {
      file->ready=1;
      return 0;
    }
  }
  
  // Cool, add it.
  if (builder->stepc>=builder->stepa) {
    int na=builder->stepa+32;
    if (na>INT_MAX/sizeof(struct builder_step)) return -1;
    void *nv=realloc(builder->stepv,sizeof(struct builder_step)*na);
    if (!nv) return -1;
    builder->stepv=nv;
    builder->stepa=na;
  }
  struct builder_step *step=builder->stepv+builder->stepc++;
  memset(step,0,sizeof(struct builder_step));
  step->file=file;
  return 0;
}

/* Generate plan, main entry point.
 */
 
int builder_generate_plan(struct builder *builder) {
  int i,err;
  for (i=builder->filec;i-->0;) {
    struct builder_file *file=builder->filev[i];
    if ((err=builder_generate_plan_1(builder,file))<0) return err;
  }
  
  /**
  {
    fprintf(stderr,"Generated %d-step plan:\n",builder->stepc);
    for (i=0;i<builder->stepc;i++) {
      struct builder_step *step=builder->stepv+i;
      fprintf(stderr,"  %.*s\n",step->file->pathc,step->file->path);
    }
  }
  /**/
  
  return 0;
}
