#include "eggrt/eggrt_internal.h"

/* Receive mappable keyboard or joystick event.
 */
 
void inmgr_mappable_event(struct inmgr *inmgr,int devid,int btnid,int value) {
  fprintf(stderr,"%s %d.%d=%d\n",__func__,devid,btnid,value);
}
