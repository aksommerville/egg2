/* builder.h
 * Build context.
 * Does interact with eggdev's globals.
 */
 
#ifndef BUILDER_H
#define BUILDER_H

#include "builder_file.h"

struct builder {

  /* WEAK.
   * Owner may assign directly to capture messages here instead of stderr.
   */
  struct sr_encoder *log;

  char *root;
  int rootc;
  char *projname;
  int projnamec;
  struct builder_file **filev;
  int filec,filea;
  int fileid_next;
  int job_limit;
  
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
  
  struct builder_process {
    int pid;
    int running;
    int fd;
    struct builder_step *step; // WEAK
    char *cmd; // for diagnostics only
    int cmdc;
  } *processv;
  int processc,processa;

  /* metadata:1, uncompiled.
   * This gets populated when you ask for a metadata field.
   * For the forseeable future, only MacOS uses it.
   */
  char *metadata;
  int metadatac;
};

void builder_cleanup(struct builder *builder);

/* You should also change the global project root.
 * We don't because in the HTTP case, that would mean flushing project every time, and
 * then it would be out of scope for /api/convert calls.
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

/* Fine steps, per file.
 */
int build_datarom(struct builder *builder,struct builder_file *file);
int build_fullrom(struct builder *builder,struct builder_file *file);
int build_standalone(struct builder *builder,struct builder_file *file);
int build_separate(struct builder *builder,struct builder_file *file);
int build_mac_plist(struct builder *builder,struct builder_file *file);
int builder_schedule_link(struct builder *builder,struct builder_step *step);
int builder_schedule_compile(struct builder *builder,struct builder_step *step);
int builder_schedule_datao(struct builder *builder,struct builder_step *step);
int builder_schedule_mac_icns(struct builder *builder,struct builder_step *step);
int builder_schedule_mac_nib(struct builder *builder,struct builder_step *step);

void builder_process_cleanup(struct builder_process *process);
int builder_begin_command(struct builder *builder,struct builder_step *step,const char *cmd,int cmdc,const void *in,int inc);

struct strlist {
  struct strlist_entry {
    char *v;
    int c;
  } *v;
  int c,a;
};
void strlist_cleanup(struct strlist *strlist);
int strlist_has(const struct strlist *strlist,const char *src,int srcc);
char *strlist_add(struct strlist *strlist,const char *src,int srcc);

int builder_error(struct builder *builder,const char *fmt,...);
#define builder_log(builder,fmt,...) builder_error(builder,fmt,##__VA_ARGS__)

/* Load metadata if needed.
 * Returns a WEAK pointer to the field's value, or zero if absent (empty and absent are equivalent).
 */
int builder_get_metadata(void *vpp,struct builder *builder,const char *k,int kc);

/* Get an uncompiled data resource.
 * On success, (*dstpp) is STRONG, caller frees it.
 * If we return zero, (*dstpp) was allocated, and that denotes a present but empty file.
 * (type) is the resource name and must match the directory's name exactly.
 */
int builder_get_resource(void *dstpp,struct builder *builder,const char *type,int typec,int rid);

#endif
