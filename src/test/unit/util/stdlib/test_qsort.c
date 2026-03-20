#include "test/egg_test.h"
#include <stdio.h>
#include <sys/time.h>

/* Ensure we're using the fake one, and not what stdlib provides.
 * To my pleasant surprise, it's fine to declare our own version even when real libc is in play.
 */
#ifdef USE_real_stdlib
  #undef USE_real_stdlib
#endif
#include "util/stdlib/qsort.c"
#include "util/stdlib/rand.c"

double egg_time_real() {
  struct timeval tv;
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

/* 20260320: I've observed our qsort to make incorrect decisions in the wild.
 * It happened to a list of 8.
 */
 
static int validate_ints(const int *v,int c) {
  if (c<1) return 0;
  int min=v[0];
  for (;c-->0;v++) {
    if (*v<min) return -1;
    min=*v;
  }
  return 0;
}

static int cmp_int(const void *a,const void *b) {
  int A=*(int*)a,B=*(int*)b;
  return A-B;
}

static void print_ints(const char *desc,const int *v,int c) {
  fprintf(stderr,"%s(%s): ",__func__,desc);
  for (;c-->0;v++) fprintf(stderr,"%d,",*v);
  fprintf(stderr,"\n");
}
 
static int qsort_ints() {
  #define _(...) { \
    int v[]={__VA_ARGS__}; \
    int c=sizeof(v)/sizeof(int); \
    qsort(v,c,sizeof(int),cmp_int); \
    /** print_ints(#__VA_ARGS__,v,c); /**/ \
    EGG_ASSERT_CALL(validate_ints(v,c),"qsort failed for ints: %s",#__VA_ARGS__) \
  }
  _(3,2,3,1) // Fails: 2,3,1,3
  _(1,2,3,4,5,6,7,8)
  _(8,7,6,5,4,3,2,1) // Fails: 3,1,2,4,7,5,6,8
  _(2,3,4,5,1,6,7,8)
  _(1,2,3,4,1,2,3,4)
  _(4,4,3,3,2,2,1,1)
  _(4,3,2,1,1,2,3,4)
  #undef _
  return 0;
}

/* I'm still not 100% confident.
 * Exhaustive tests are out of the question, there's no such thing.
 * But let's run millions of randomized sorts.
 * One million reps runs almost instantly. At ten million, you notice it spinning.
 * If you enable the per-rep logging, keep it down to 100k or so.
 */
 
static int qsort_giant_random() {
  int seed=(int)(time(0));
  srand(seed);
  fprintf(stderr,"%s: Random seed %d\n",__func__,seed);
  
  int repc=1000000;
  fprintf(stderr,"%s: Running %d repetitions...\n",__func__,repc);
  while (repc-->0) {
  
    int len=2+rand()%20;
    int v[32];
    int c=0;
    while (c<len) v[c++]=rand()%10; // %10 to keep them small and raise the possibility of collisions.
    //int i=0; for (;i<c;i++) fprintf(stderr,"%d,",v[i]); fprintf(stderr,"\n");
    qsort(v,c,sizeof(int),cmp_int);
    EGG_ASSERT_CALL(validate_ints(v,c))
  
  }
  return 0;
}

/* Our implementation of qsort is supposed to be stable.
 * Verify by sorting a bunch of equivalent things that we are able to distinguish.
 */
 
struct thing {
  int name; // Sort by this.
  int id; // Will be initially sorted, qsort will not examine, and must remain sorted.
};

static int thingcmp_for_qsort(const void *a,const void *b) {
  const struct thing *A=a,*B=b;
  return A->name-B->name;
}

static int thingcmp_for_validation(const void *a,const void *b) {
  const struct thing *A=a,*B=b;
  return A->id-B->id;
}

static int validate_things(const struct thing *v,int c) {
  for (;c-->1;v++) {
    if (thingcmp_for_qsort(v,v+1)<0) continue; // Different names. Don't care how their ids compare.
    if (thingcmp_for_validation(v,v+1)>0) return -1;
  }
  return 0;
}

static void dump_things(const char *desc,const struct thing *v,int c) {
  fprintf(stderr,"%s:%s:",__func__,desc);
  for (;c-->0;v++) fprintf(stderr," (%d,%d)",v->name,v->id);
  fprintf(stderr,"\n");
}
 
static int qsort_stable() {
  struct thing v[]={
    {100,  1},
    {  1,  2},
    {100,  3},
    {  1,  4},
    { 50,  5},
    {500,  6},
    {100,  7},
    { 50,  8},
    {  1,  9},
    {500, 10},
    {100, 11},
  };
  int c=sizeof(v)/sizeof(struct thing);
  //dump_things("BEFORE",v,c);
  qsort(v,c,sizeof(struct thing),thingcmp_for_qsort);
  //dump_things("AFTER",v,c);
  EGG_ASSERT_CALL(validate_things(v,c))
  return 0;
}

/* TOC.
 */

int main(int argc,char **argv) {
  EGG_UTEST(qsort_ints,qsort)
  XXX_EGG_UTEST(qsort_giant_random,qsort)
  EGG_UTEST(qsort_stable,qsort)
  return 0;
}
