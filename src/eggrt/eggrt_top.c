#include "eggrt_internal.h"
#include <unistd.h>

struct eggrt eggrt={0};

/* Quit.
 */
 
void eggrt_quit(int status) {
  
  eggrt_call_client_quit(status);
  
  if (!status) {
    //TODO Report performance.
    fprintf(stderr,"%s:%d:TODO: Performance report\n",__FILE__,__LINE__);
  }
  
  //TODO Tear down drivers.
  fprintf(stderr,"%s:%d:TODO: Tear down drivers.\n",__FILE__,__LINE__);
}

/* Init.
 */
 
int eggrt_init() {
  int err;
  
  //TODO Prep drivers.
  fprintf(stderr,"%s:%d:TODO: Driver setup.\n",__FILE__,__LINE__);
  
  // Initialize client.
  if ((err=eggrt_call_client_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error %d from egg_client_init.\n",eggrt.exename,err);
    return -2;
  }
  
  return 0;
}

/* Update.
 */
 
int eggrt_update() {
  int err;

  //TODO Tick clock.
  usleep(200000);
  double elapsed=0.016666;
  
  //TODO Update drivers.
  
  // Update client.
  if ((err=eggrt_call_client_update(elapsed))<0) return err;
  
  //TODO Render.
  if ((err=eggrt_call_client_render())<0) return err;
  
  return 0;
}
