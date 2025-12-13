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

/* Sort a bunch of steps by path alphabetically.
 * Actually it doesn't really matter what we sort by as long as it's consistent across runs.
 * So we'll do a slightly more convenient length test first.
 * The point is, if my last build failed at File A, and File A is still broken, I want this build to fail on File A too.
 */
 
static int builder_stepcmp(const void *A,const void *B) {
  const struct builder_step *a=A,*b=B;
  if (a->file->pathc<b->file->pathc) return -1;
  if (a->file->pathc>b->file->pathc) return 1;
  return memcmp(a->file->path,b->file->path,a->file->pathc);
}
 
static void builder_sort_steps(struct builder *builder,struct builder_step *stepv,int stepc) {
  qsort(stepv,stepc,sizeof(struct builder_step),builder_stepcmp);
}

/* How many steps at the start of (stepv) could be run in arbitrary order?
 * Or, index of the first step which has a prerequisite among the preceding steps.
 */
 
static int builder_count_commutative_steps(struct builder *builder,struct builder_step *stepv,int stepc) {
  if (stepc<1) return 0;
  int fratc=1;
  while (fratc<stepc) {
    struct builder_step *step=stepv+fratc;
    int i=fratc;
    while (i-->0) {
      if (builder_file_has_req(step->file,stepv[i].file)) return fratc;
    }
    fratc++;
  }
  return fratc;
}

/* Optional step after generating a valid plan.
 * Where adjacent steps do not depend on each other, sort them by path alphabetically.
 * This encourages a more deterministic order to the build.
 * eg you have 3 files with lots of errors in them, File A will be the first we try every time.
 */
 
static int builder_sort_plan(struct builder *builder) {
  int stepp=0;
  while (stepp<builder->stepc) {
    int fratc=builder_count_commutative_steps(builder,builder->stepv+stepp,builder->stepc-stepp);
    if (fratc<0) return fratc;
    if (fratc<1) fratc=1;
    if (fratc>1) builder_sort_steps(builder,builder->stepv+stepp,fratc);
    stepp+=fratc;
  }
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
  if ((err=builder_sort_plan(builder))<0) return err;
  
  /**
  {
    fprintf(stderr,"Generated %d-step plan:\n",builder->stepc);
    for (i=0;i<builder->stepc;i++) {
      struct builder_step *step=builder->stepv+i;
      fprintf(stderr,"  %.*s\n",step->file->pathc,step->file->path);
    }
    fprintf(stderr,"--- end of plan ---\n"); // Must visually terminate the list, otherwise it blends in with the build log!
  }
  /**/
  
  return 0;
}
