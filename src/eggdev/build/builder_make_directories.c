#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Trivial list of strings, ones we've done so far.
 */
 
struct strlist {
  struct strlist_entry {
    char *v;
    int c;
  } *v;
  int c,a;
};

static void strlist_cleanup(struct strlist *strlist) {
  if (strlist->v) {
    while (strlist->c-->0) free(strlist->v[strlist->c].v);
    free(strlist->v);
  }
}

static int strlist_has(const struct strlist *strlist,const char *src,int srcc) {
  const struct strlist_entry *entry=strlist->v;
  int i=strlist->c;
  for (;i-->0;entry++) {
    if (entry->c!=srcc) continue;
    if (memcmp(entry->v,src,srcc)) continue;
    return 1;
  }
  return 0;
}

static char *strlist_add(struct strlist *strlist,const char *src,int srcc) {
  if (strlist->c>=strlist->a) {
    int na=strlist->a+32;
    if (na>INT_MAX/sizeof(struct strlist_entry)) return 0;
    void *nv=realloc(strlist->v,sizeof(struct strlist_entry)*na);
    if (!nv) return 0;
    strlist->v=nv;
    strlist->a=na;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return 0;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  struct strlist_entry *entry=strlist->v+strlist->c++;
  entry->v=nv;
  entry->c=srcc;
  return nv;
}

/* Make directories, anything that's going to contain a file we make.
 */
 
int builder_make_directories(struct builder *builder) {
  struct strlist strlist={0};
  int i=builder->filec;
  while (i-->0) {
    struct builder_file *file=builder->filev[i];
    // There's only two directories we're allowed to build under: PROJECT/mid and PROJECT/out
    if (file->pathc<builder->rootc+5) continue;
    if (memcmp(file->path,builder->root,builder->rootc)) continue;
    if (
      memcmp(file->path+builder->rootc,"/mid/",5)&&
      memcmp(file->path+builder->rootc,"/out/",5)
    ) continue;
    // Lop off the basename.
    int dirc=file->pathc-1;
    while (dirc&&(file->path[dirc]!='/')) dirc--;
    // Skip if we already touched it.
    if (strlist_has(&strlist,file->path,dirc)) continue;
    // mkdir -p, and add to the list.
    char *zpath=strlist_add(&strlist,file->path,dirc);
    if (!zpath) { strlist_cleanup(&strlist); return -1; }
    if (dir_mkdirp(zpath)<0) { strlist_cleanup(&strlist);return -1; }
  }
  strlist_cleanup(&strlist);
  return 0;
}
