/* builder_file.h
 * Represents one file, which may or may not exist.
 * Both input and output, and also may be outside the project.
 */
 
#ifndef BUILDER_FILE_H
#define BUILDER_FILE_H

struct builder_target;

#define BUILDER_FILE_HINT_INPUT 1 /* Among initial inputs, if we don't have something more specific to say. */
#define BUILDER_FILE_HINT_RES 2
#define BUILDER_FILE_HINT_C 3
#define BUILDER_FILE_HINT_DATAROM 4 /* ROM file with no code, shared among all targets. */
#define BUILDER_FILE_HINT_CODE1 5 /* Linked Wasm module. */
#define BUILDER_FILE_HINT_OBJ 6
#define BUILDER_FILE_HINT_FULLROM 7 /* ROM as a web prereq, containing code:1. */
#define BUILDER_FILE_HINT_STANDALONE 8
#define BUILDER_FILE_HINT_SEPARATE 9
#define BUILDER_FILE_HINT_EXE 10
#define BUILDER_FILE_HINT_DATAO 11 /* DATAROM wrapped in a native linkable object. */

struct builder_file {
  int id;
  char *path; // Includes project root but may be relative.
  int pathc;
  long mtime;
  int hint;
  struct builder_target *target; // optional, weak
  struct builder_file **reqv; // WEAK
  int reqc,reqa;
  int planned; // Nonzero if we've been added already as a step.
  int ready; // Nonzero if build is complete, or doesn't need built.
};

void builder_file_del(struct builder_file *file);
struct builder_file *builder_file_new();

int builder_file_set_path(struct builder_file *file,const char *src,int srcc);

int builder_file_add_req(struct builder_file *file,struct builder_file *req);
int builder_file_has_req(const struct builder_file *file,const struct builder_file *req);
int builder_file_depends_on(const struct builder_file *product,const struct builder_file *input);

struct builder_file *builder_file_req_with_hint(struct builder_file *file,int hint);

#endif
