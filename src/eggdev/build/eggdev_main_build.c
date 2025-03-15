#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Build, main entry point.
 */
 
int eggdev_main_build() {
  int err;
  const char *root;
  if (g.srcpathc==0) root=".";
  else if (g.srcpathc==1) root=g.srcpathv[0];
  else {
    fprintf(stderr,"%s: Too many inputs.\n",g.exename);
    return -2;
  }
  struct builder builder={0};
  if ((err=builder_set_root(&builder,root,-1))<0) {
    builder_cleanup(&builder);
    return err;
  }
  err=builder_main(&builder);
  builder_cleanup(&builder);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error building project.\n",root);
    return -2;
  }
  return 0;
}
