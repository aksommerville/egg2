#include "eggdev_internal.h"

/* Line number.
 */
 
int eggdev_lineno(const char *src,int srcc) {
  int lineno=1,srcp=0;
  for (;srcp<srcc;srcp++) {
    if (src[srcp]==0x0a) lineno++;
  }
  return lineno;
}

/* Relative path.
 */
 
int eggdev_relative_path(char *dst,int dsta,const char *ref,int refc,const char *rel,int relc) {
  if (!dst||(dsta<0)) dsta=0;
  if (!ref) refc=0; else if (refc<0) { refc=0; while (ref[refc]) refc++; }
  if (!rel) relc=0; else if (relc<0) { relc=0; while (rel[relc]) relc++; }
  
  // Trim one entry off (ref), if it doesn't end with a slash.
  while (refc&&(ref[refc-1]!='/')) refc--;
  
  // Resolve leading empty, dot, and double-dot entries.
  while (relc>0) {
    int leadc=0;
    while ((leadc<relc)&&(rel[leadc]!='/')) leadc++;
    if (leadc==0) { // Absolute paths not allowed. Trim the slash and proceed.
      rel++;
      relc--;
    } else if ((leadc==1)&&(rel[0]=='.')) { // Single dot is meaningless.
      rel++;
      relc--;
    } else if ((leadc==2)&&(rel[0]=='.')&&(rel[1]=='.')) { // Back one level off (ref).
      rel+=2;
      relc-=2;
      while (refc&&(ref[refc-1]=='/')) refc--;
      while (refc&&(ref[refc-1]!='/')) refc--;
    } else {
      break;
    }
  }
  
  // Concatenate.
  while (refc&&(ref[refc-1]=='/')) refc--;
  if (!refc) {
    if (relc<=dsta) memcpy(dst,rel,relc);
    if (relc<dsta) dst[relc]=0;
    return relc;
  }
  int dstc=refc+1+relc;
  if (dstc>dsta) return dstc;
  memcpy(dst,ref,refc);
  dst[refc]='/';
  memcpy(dst+refc+1,rel,relc);
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
