#include "test/egg_test.h"

/* Filter bits.
 */
 
static int string_in_list(const char *q,int qc,const char *list) {
  if (!list) return 0;
  if (!q) return 0;
  if (qc<0) { qc=0; while (q[qc]) qc++; }
  if (!qc) return 0;
  while (*list) {
    if ((unsigned char)(*list)<=0x20) { list++; continue; }
    const char *token=list;
    int tokenc=0;
    while ((unsigned char)(*list)>0x20) { list++; tokenc++; }
    if ((tokenc==qc)&&!memcmp(q,token,tokenc)) return 1;
  }
  return 0;
}

static int any_string_in_list(const char *a,const char *b) {
  if (!a||!b) return 0;
  while (*a) {
    if ((unsigned char)(*a)<=0x20) { a++; continue; }
    const char *token=a;
    int tokenc=0;
    while ((unsigned char)(*a)>0x20) { a++; tokenc++; }
    if (string_in_list(token,tokenc,b)) return 1;
  }
  return 0;
}

static const char *egg_basename(const char *path) {
  if (!path) return 0;
  const char *base=path;
  int i=0; for (;path[i];i++) if (path[i]=='/') base=path+i+1;
  return base;
}

/* Tests filter.
 */
 
int egg_test_filter(const char *name,const char *tags,const char *file,int ignore) {
  const char *filter=getenv("EGG_TEST_FILTER");
  if (filter&&filter[0]) {
    if (string_in_list(name,-1,filter)) return 1;
    if (string_in_list(egg_basename(file),-1,filter)) return 1;
    if (any_string_in_list(tags,filter)) return 1;
    return 0;
  }
  if (ignore) return 0;
  return 1;
}

/* Is string printable?
 */

int egg_string_printable(const char *src,int srcc) {
  if (srcc<0) return 0;
  if (!srcc) return 1;
  if (!src) return 0;
  if (srcc>100) return 0;
  for (;srcc-->0;src++) {
    if ((*src<0x20)||(*src>0x7e)) return 0;
  }
  return 1;
}
