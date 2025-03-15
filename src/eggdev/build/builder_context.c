#include "eggdev/eggdev_internal.h"
#include "builder.h"
#include <unistd.h>

/* Cleanup.
 */
 
static void builder_target_cleanup(struct builder_target *target) {
}

static void builder_step_cleanup(struct builder_step *step) {
}
 
void builder_cleanup(struct builder *builder) {
  if (builder->root) free(builder->root);
  if (builder->projname) free(builder->projname);
  if (builder->processv) {
    while (builder->processc-->0) builder_process_cleanup(builder->processv+builder->processc);
    free(builder->processv);
  }
  if (builder->stepv) {
    while (builder->stepc-->0) builder_step_cleanup(builder->stepv+builder->stepc);
    free(builder->stepv);
  }
  if (builder->filev) {
    while (builder->filec-->0) builder_file_del(builder->filev[builder->filec]);
    free(builder->filev);
  }
  if (builder->targetv) {
    while (builder->targetc-->0) builder_target_cleanup(builder->targetv+builder->targetc);
    free(builder->targetv);
  }
}

/* Set projname as the last component of this path.
 */
 
static int builder_auto_projname(struct builder *builder,const char *path) {
  const char *src=path;
  int srcc=0,pathp=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') {
      src=path+pathp+1;
      srcc=0;
    } else {
      srcc++;
    }
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  if (builder->projname) free(builder->projname);
  builder->projname=nv;
  builder->projnamec=srcc;
  return 0;
}

/* Set root.
 */
 
int builder_set_root(struct builder *builder,const char *path,int pathc) {
  int err;
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  while ((pathc>1)&&(path[pathc-1]=='/')) pathc--;
  if ((err=eggdev_client_set_root(path,pathc))<0) return err;
  builder->rootc=0;
  if (builder->root) free(builder->root);
  if (!(builder->root=malloc(pathc+1))) return -1;
  memcpy(builder->root,path,pathc);
  builder->root[pathc]=0;
  builder->rootc=pathc;
  
  if ((pathc<2)||((pathc==1)&&(path[0]=='.'))) {
    char *cwd=getcwd(0,0);
    if (cwd) {
      builder_auto_projname(builder,cwd);
      free(cwd);
    }
  } else {
    builder_auto_projname(builder,builder->root);
  }
  
  return 0;
}

/* Add file.
 */
 
struct builder_file *builder_add_file(struct builder *builder,const char *path,int pathc) {
  if (builder->fileid_next>=INT_MAX) return 0;
  if (builder->filec>=builder->filea) {
    int na=builder->filea+32;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(builder->filev,sizeof(void*)*na);
    if (!nv) return 0;
    builder->filev=nv;
    builder->filea=na;
  }
  struct builder_file *file=builder_file_new();
  if (!file) return 0;
  file->id=builder->fileid_next++;
  if (builder_file_set_path(file,path,pathc)<0) {
    builder_file_del(file);
    return 0;
  }
  builder->filev[builder->filec++]=file;
  return file;
}

/* Find file by path.
 */
 
struct builder_file *builder_find_file(struct builder *builder,const char *path,int pathc) {
  if (!path) return 0;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *file=builder->filev[i];
    if (file->pathc!=pathc) continue;
    if (memcmp(file->path,path,pathc)) continue;
    return file;
  }
  return 0;
}

/* Main.
 */

int builder_main(struct builder *builder) {
  int err;
  if (!builder||(builder->rootc<1)) return -1;
  if (builder->filec) return -1;
  if (builder->targetc) return -1;
  if (builder->stepc) return -1;
  builder->fileid_next=1;
  if (builder->job_limit<1) builder->job_limit=4;
  
  if ((err=builder_populate_targets(builder))<0) return err;
  if ((err=builder_discover_inputs(builder))<0) return err;
  if ((err=builder_generate_restoc(builder))<0) return err;
  if ((err=builder_infer_outputs(builder))<0) return err;
  if ((err=builder_make_directories(builder))<0) return err;
  if ((err=builder_generate_plan(builder))<0) return err;
  if ((err=builder_execute_plan(builder))<0) return err;

  return 0;
}
