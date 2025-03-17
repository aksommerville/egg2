#include "eggrt_internal.h"
#include <sys/signal.h>

/* Signal handler.
 */
 
static volatile int eggrt_sigc=0;
 
static void eggrt_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++eggrt_sigc>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",eggrt.exename);
        exit(1);
      } break;
  }
}

/* Main.
 */

int main(int argc,char **argv) {

  int err=eggrt_configure(argc,argv);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading configuration.\n",eggrt.exename);
    return 1;
  }
  if (eggrt.terminate) return 0;
  
  signal(SIGINT,eggrt_signal);
  
  if ((err=eggrt_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error during initialization.\n",eggrt.exename);
    eggrt_quit(1);
    return 1;
  }
  
  while (!eggrt.terminate&&!eggrt_sigc) {
    if ((err=eggrt_update())<0) {
      if (err!=-2) fprintf(stderr,"%s Unspecified error updating runtime.\n",eggrt.exename);
      eggrt_quit(1);
      return 1;
    }
  }
  
  eggrt_quit(0);
  return 0;
}
