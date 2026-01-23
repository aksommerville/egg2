#include "egg-stdlib.h"

static char *qsort_scratch=0;
static int qsort_scratcha=0;

void qsort(void *p,size_t c,size_t size,int (*cmp)(const void *a,const void *b)) {
  if (c<2) return;
  if (size<1) return;
  if (size>qsort_scratcha) {
    void *nv=realloc(qsort_scratch,size);
    if (!nv) return;
    qsort_scratch=nv;
    qsort_scratcha=size;
  }
  int pivot=c>>1;
  #define AT(ix) (((char*)p)+((ix)*size))
  
  int lp=pivot;
  int i=lp;
  while (i-->0) {
    int q=cmp(AT(i),AT(lp));
    if (q>0) {
      memcpy(qsort_scratch,AT(i),size);
      memmove(AT(i),AT(i+1),size*(pivot-i));
      memcpy(AT(pivot),qsort_scratch,size);
      pivot--;
      lp--;
    } else if (q==0) {
      lp--;
      memcpy(qsort_scratch,AT(i),size);
      memmove(AT(i),AT(i+1),size*(lp-i));
      memcpy(AT(lp),qsort_scratch,size);
    }
  }
  
  int rp=pivot;
  for (i=rp+1;i<c;i++) {
    int q=cmp(AT(i),AT(rp));
    if (q<0) {
      memcpy(qsort_scratch,AT(i),size);
      memmove(AT(rp+1),AT(rp),size*(i-rp));
      memcpy(AT(rp),qsort_scratch,size);
      rp++;
    } else if (q==0) {
      rp++;
      memcpy(qsort_scratch,AT(i),size);
      memmove(AT(rp+1),AT(rp),size*(i-rp));
      memcpy(AT(rp),qsort_scratch,size);
    }
  }
  
  qsort(p,lp,size,cmp);
  qsort(AT(pivot+1),c-pivot-1,size,cmp);
  #undef AT
}
