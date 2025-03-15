#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Choose a step from within the given indices (lo inclusive, hi exclusive).
 * We return (lo<=n<hi) if a step is ready to begin, or <0 if we're stalled.
 */
 
static int builder_choose_next_step(struct builder *builder,int lo,int hi) {
  const struct builder_step *step=builder->stepv+lo;
  int p=lo; for (;p<hi;p++,step++) {
    if (step->file->ready) continue;
    int reqs_ready=1;
    struct builder_file **req=step->file->reqv;
    int reqi=step->file->reqc;
    for (;reqi-->0;req++) {
      if (!(*req)->ready) {
        reqs_ready=0;
        break;
      }
    }
    if (reqs_ready) return p;
  }
  return -1;
}

/* If this step can be done synchronously, do it.
 * Otherwise launch the process for it and prepare the necessary bookkeeping.
 * Call only when all prereqs are ready.
 */
 
static int builder_begin_step(struct builder *builder,struct builder_step *step) {
  fprintf(stderr,"BUILD FILE: %s\n",step->file->path);//TODO
  if (file_write(step->file->path,0,0)<0) return -1;//XXX Actually produce a file, so I'm leaving physical evidence...
  step->file->ready=1;
  return 0;
}

/* No steps are ready to build.
 * If we have processes in flight, wait for at least one to finish.
 * Otherwise fail.
 */
 
static int builder_wait(struct builder *builder) {
  //TODO
  fprintf(stderr,"%s: PANIC! Entered %s with no jobs running.\n",g.exename,__func__);
  return -2;
}

/* Execute plan, main entry point.
 */
 
int builder_execute_plan(struct builder *builder) {
  fprintf(stderr,"%s: %d steps\n",__func__,builder->stepc);
  int stepplo=0,stepphi=builder->stepc,err;
  while (stepplo<stepphi) {
    int stepp=builder_choose_next_step(builder,stepplo,stepphi);
    fprintf(stderr,"%s: Chose step %d from %d..%d\n",__func__,stepp,stepplo,stepphi);
    if (stepp>=0) {
      if ((err=builder_begin_step(builder,builder->stepv+stepp))<0) return err;
    } else {
      if ((err=builder_wait(builder))<0) return err;
    }
    while ((stepplo<stepphi)&&builder->stepv[stepplo].file->ready) stepplo++;
    while ((stepplo<stepphi)&&builder->stepv[stepphi-1].file->ready) stepphi--;
  }
  return 0;
}
