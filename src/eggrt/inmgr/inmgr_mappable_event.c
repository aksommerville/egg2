#include "eggrt/eggrt_internal.h"

/* Dispatch action.
 */
 
static void inmgr_dispatch_action(struct inmgr *inmgr,int action) {
  //fprintf(stderr,"TODO %s 0x%08x\n",__func__,action);
  switch (action) {
    case INMGR_ACTION_QUIT: eggrt.terminate=1; eggrt.status=0; break;
    case INMGR_ACTION_FULLSCREEN: hostio_toggle_fullscreen(eggrt.hostio); break;
  }
}

/* Set mapped button on some device.
 */
 
static void inmgr_set_button(struct inmgr *inmgr,struct inmgr_device *device,int btnid,int value) {
  if (value) {
    if (device->state&btnid) return;
    inmgr_device_require_player(inmgr,device);
    device->state|=btnid;
    inmgr->playerv[device->playerid]|=btnid;
    inmgr->playerv[0]|=btnid;
  } else {
    if (!(device->state&btnid)) return;
    device->state&=~btnid;
    if ((device->playerid>0)&&(device->playerid<=inmgr->playerc)) {
      inmgr->playerv[device->playerid]&=~btnid;
      inmgr->playerv[0]&=~btnid;
    }
  }
}

/* Receive mappable keyboard or joystick event.
 */
 
void inmgr_mappable_event(struct inmgr *inmgr,int devid,int btnid,int value) {
  //fprintf(stderr,"%s %d.0x%08x=%d\n",__func__,devid,btnid,value);
  
  // Find the device and button.
  struct inmgr_device *device=inmgr_get_device_by_devid(inmgr,devid);
  if (!device) return;
  struct inmgr_button *button=inmgr_device_buttonv_get(device,btnid);
  if (!button) return;
  
  // If input value unchanged, forget it. This shouldn't come up much.
  if (button->srcvalue==value) return;
  button->srcvalue=value;
  
  // (dstbtnid) unset, ignore the event.
  if (!button->dstbtnid) {
    return;
  
  // Wider than 16 bits, it's a stateless action. Map like two-state, and trigger when it changes to On.
  } else if (button->dstbtnid>=0x10000) {
    int dstvalue=(value>=button->srclo)?1:0;
    if (dstvalue==button->dstvalue) return;
    button->dstvalue=dstvalue;
    if (dstvalue) inmgr_dispatch_action(inmgr,button->dstbtnid);
  
  // Hats affect all 4 dpad buttons.
  } else if (button->dstbtnid==EGG_BTN_DPAD) {
    value-=button->srclo;
    if ((value<0)||(value>7)) value=-1;
    if (value==button->dstvalue) return;
    int pvx=0,pvy=0,nxx=0,nxy=0;
    switch (button->dstvalue) {
      case 1: case 2: case 3: pvx=1; break;
      case 5: case 6: case 7: pvx=-1; break;
    }
    switch (button->dstvalue) {
      case 7: case 0: case 1: pvy=-1; break;
      case 5: case 4: case 3: pvy=1; break;
    }
    button->dstvalue=value;
    switch (value) {
      case 1: case 2: case 3: nxx=1; break;
      case 5: case 6: case 7: nxx=-1; break;
    }
    switch (value) {
      case 7: case 0: case 1: nxy=-1; break;
      case 5: case 4: case 3: nxy=1; break;
    }
    if (pvx!=nxx) {
           if (pvx<0) inmgr_set_button(inmgr,device,EGG_BTN_LEFT,0);
      else if (pvx>0) inmgr_set_button(inmgr,device,EGG_BTN_RIGHT,0);
           if (nxx<0) inmgr_set_button(inmgr,device,EGG_BTN_LEFT,1);
      else if (nxx>0) inmgr_set_button(inmgr,device,EGG_BTN_RIGHT,1);
    }
    if (pvy!=nxy) {
           if (pvy<0) inmgr_set_button(inmgr,device,EGG_BTN_UP,0);
      else if (pvy>0) inmgr_set_button(inmgr,device,EGG_BTN_DOWN,0);
           if (nxy<0) inmgr_set_button(inmgr,device,EGG_BTN_UP,1);
      else if (nxy>0) inmgr_set_button(inmgr,device,EGG_BTN_DOWN,1);
    }

  // Two-way axes affect either (LEFT,RIGHT) or (UP,DOWN).
  } else if ((button->dstbtnid==EGG_BTN_HORZ)||(button->dstbtnid==EGG_BTN_VERT)) {
    int LO=button->dstbtnid&(EGG_BTN_LEFT|EGG_BTN_UP);
    int HI=button->dstbtnid&(EGG_BTN_RIGHT|EGG_BTN_DOWN);
    value=(value<=button->srclo)?-1:(value>=button->srchi)?1:0;
    if (value==button->dstvalue) return;
         if (button->dstvalue<0) inmgr_set_button(inmgr,device,LO,0);
    else if (button->dstvalue>0) inmgr_set_button(inmgr,device,HI,0);
    button->dstvalue=value;
         if (value<0) inmgr_set_button(inmgr,device,LO,1);
    else if (value>0) inmgr_set_button(inmgr,device,HI,1);
  
  // And finally, two-state buttons map pretty simple.
  } else {
    value=(value>=button->srclo)?1:0;
    if (value==button->dstvalue) return;
    button->dstvalue=value;
    inmgr_set_button(inmgr,device,button->dstbtnid,value);
  }
}

/* Connect device.
 */
 
int inmgr_device_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  if (driver) {
    if (devid<1) return -1;
  } else {
    if (devid) return -1;
  }

  if (inmgr->devicec>=inmgr->devicea) {
    int na=inmgr->devicea+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(inmgr->devicev,sizeof(void*)*na);
    if (!nv) return -1;
    inmgr->devicev=nv;
    inmgr->devicea=na;
  }
  
  struct inmgr_device *device=inmgr_device_new();
  if (!device) return -1;
  device->driver=driver;
  device->devid=devid;
  
  if (driver) {
    if (inmgr_device_query_config(inmgr,device)<0) {
      inmgr_device_del(device);
      return -1;
    }
  } else {
    if (inmgr_device_init_keyboard(inmgr,device)<0) {
      inmgr_device_del(device);
      return -1;
    }
  }
  
  // Don't map to a player yet. That happens when we receive the first significant event on each device.
  
  inmgr->devicev[inmgr->devicec++]=device;

  return 0;
}

/* Disconnect device.
 */
 
int inmgr_device_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid) {
  int p=inmgr_find_device_by_devid(inmgr,devid);
  if (p<0) return -1;
  struct inmgr_device *device=inmgr->devicev[p];
  inmgr->devicec--;
  memmove(inmgr->devicev+p,inmgr->devicev+p+1,sizeof(void*)*(inmgr->devicec-p));
  
  //TODO Drop player state.
  
  inmgr_device_del(device);
  return 0;
}
