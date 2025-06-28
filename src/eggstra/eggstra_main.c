#include "eggstra.h"
#include <signal.h>

struct eggstra eggstra={0};

/* Signal.
 */
 
static void rcvsig(int sigid) {
  switch (sigid) {
    case SIGINT: if (++eggstra.sigc>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Main.
 */
 
int main(int argc,char **argv) {
  signal(SIGINT,rcvsig);
  int err=eggstra_configure(argc,argv);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading parameters.\n",eggstra.exename);
    return 1;
  }
  if (eggstra.sigc) return 0; // eg --help; configure wants to terminate.
  if (!eggstra.command) {
    eggstra_print_help(0,0);
    fprintf(stderr,"%s: Please provide a command.\n",eggstra.exename);
    return 1;
  }
       if (!strcmp(eggstra.command,"play")) err=eggstra_main_play();
  //else if (!strcmp(eggstra.command,"show")) err=eggstra_main_show();
  //else if (!strcmp(eggstra.command,"input")) err=eggstra_main_input();
  else {
    eggstra_print_help(0,0);
    fprintf(stderr,"%s: Unknown command '%s'.\n",eggstra.exename,eggstra.command);
    return 1;
  }
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error running command '%s'.\n",eggstra.exename,eggstra.command);
    return 1;
  }
  return 0;
}
