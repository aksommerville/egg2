#include "eggdev_internal.h"

/* Main.
 */
 
int main(int argc,char **argv) {
  int err=eggdev_configure(argc,argv);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error loading configuration.\n",g.exename);
    return 1;
  }
  if (g.terminate) return 0;
  switch (g.command) {
    #define _(tag) case EGGDEV_COMMAND_##tag: err=eggdev_main_##tag(); break;
    EGGDEV_COMMAND_FOR_EACH
    #undef _
    default: {
        fprintf(stderr,"%s: No command provided. Try '--help'?\n",g.exename);
        return -2;
      }
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'.\n",g.exename,eggdev_command_repr(g.command));
    return 1;
  }
  return 0;
}
