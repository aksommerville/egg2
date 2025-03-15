/* builder.h
 * Build context.
 * Does interact with eggdev's globals.
 */
 
#ifndef BUILDER_H
#define BUILDER_H

#include "builder_file.h"

struct builder {
  char *root;
  int rootc;
  char *projname;
  int projnamec;
  struct builder_file **filev;
  int filec,filea;
  int fileid_next;
  
  // Target strings come from the build config. They are constants.
  struct builder_target {
    const char *name; int namec;
    const char *opt; int optc;
    const char *cc; int ccc;
    const char *ld; int ldc;
    const char *ldpost; int ldpostc;
    const char *pkg; int pkgc;
    const char *exesfx; int exesfxc;
  } *targetv;
  int targetc,targeta;
  
  /* egg_res_toc.h and the various directories are made separately; they don't get steps.
   * (those also don't have "files" associated with them).
   */
  struct builder_step {
    struct builder_file *file; // WEAK, the file being produced.
  } *stepv;
  int stepc,stepa;
};

void builder_cleanup(struct builder *builder);

/* Changes the global project root.
 */
int builder_set_root(struct builder *builder,const char *path,int pathc);

int builder_main(struct builder *builder);

/* Internal use.
 *********************************************************************************************/
 
struct builder_file *builder_add_file(struct builder *builder,const char *path,int pathc);
struct builder_file *builder_find_file(struct builder *builder,const char *path,int pathc);

/* The gross steps of the build process, all called from builder_main().
 */
int builder_populate_targets(struct builder *builder);
int builder_discover_inputs(struct builder *builder);
int builder_generate_restoc(struct builder *builder);
int builder_infer_outputs(struct builder *builder);
int builder_make_directories(struct builder *builder);
int builder_generate_plan(struct builder *builder);
int builder_execute_plan(struct builder *builder);

#endif
