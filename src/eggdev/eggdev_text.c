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

/* Resource IDs.
 */
 
int eggdev_res_ids_from_path(int *tid,int *rid,const char *path) {
  const char *tname=0,*base=path;
  int tnamec=0,basec=0;
  int pathp=0;
  for (;path[pathp];pathp++) {
    if (path[pathp]=='/') {
      tname=base;
      tnamec=basec;
      base=path+pathp+1;
      basec=0;
    } else {
      basec++;
    }
  }
  if ((basec==8)&&!memcmp(base,"metadata",8)) { *tid=EGG_TID_metadata; *rid=1; return 0; }
  if ((basec==9)&&!memcmp(base,"code.wasm",9)) { *tid=EGG_TID_code; *rid=1; return 0; }
  if ((*tid=eggdev_tid_eval(tname,tnamec))<1) return -1;
  int lang=0;
  if ((basec>=3)&&(base[0]>='a')&&(base[0]<='z')&&(base[1]>='a')&&(base[1]<='z')&&(base[2]=='-')) {
    lang=((base[0]-'a'+1)<<5)|(base[1]-'a'+1);
    base+=3;
    basec-=3;
  }
  if ((basec<1)||(base[0]<'0')||(base[0]>='9')) return -1;
  *rid=0;
  int basep=0;
  while ((basep<basec)&&(base[basep]>='0')&&(base[basep]<='9')) {
    (*rid)*=10;
    (*rid)+=base[basep++]-'0';
    if (*rid>0xffff) return -1;
  }
  if (!*rid) return -1;
  if (lang) {
    if (*rid>0x3f) return -1;
    (*rid)|=lang<<6;
  }
  return 0;
}
