#include "test/egg_test.h"
#include "egg_itest_toc.h"

int main(int argc,char **argv) {
  const struct egg_itest *itest=egg_itestv;
  int i=sizeof(egg_itestv)/sizeof(struct egg_itest);
  for (;i-->0;itest++) {
    const char *result;
    if (!egg_test_filter(itest->name,itest->tags,itest->file,itest->ignore)) result="SKIP";
    else if (itest->fn()<0) result="FAIL";
    else result="PASS";
    fprintf(stderr,"EGG_TEST %s %s %s:%d %s\n",result,itest->name,itest->file,itest->line,itest->tags);
  }
  return 0;
}
