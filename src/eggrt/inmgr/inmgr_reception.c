#include "eggrt/eggrt_internal.h"

int inmgr_key(struct hostio_video *driver,int keycode,int value) {
  if ((value==0)||(value==1)) {
    // State zero or one, map to players.
    inmgr_mappable_event(&eggrt.inmgr,0,keycode,value);
  }
  return 1; // Zero if we want translated text events too; we don't.
}

void inmgr_connect(struct hostio_input *driver,int devid) {
  if (inmgr_device_connect(&eggrt.inmgr,driver,devid)<0) return;
}

void inmgr_disconnect(struct hostio_input *driver,int devid) {
  if (inmgr_device_disconnect(&eggrt.inmgr,driver,devid)<0) return;
}

void inmgr_button(struct hostio_input *driver,int devid,int btnid,int value) {
  if (!devid||!btnid) return; // (devid) and (btnid) zero are reserved, for keyboards and connection events.
  inmgr_mappable_event(&eggrt.inmgr,devid,btnid,value);
}
