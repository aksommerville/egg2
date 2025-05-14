/* inmgr_device.h
 * Record of a connected device, its mapping details, and its live state.
 * System keyboard and joysticks both get represented as devices.
 */
 
#ifndef INMGR_DEVICE_H
#define INMGR_DEVICE_H

/* In general, you can only map to one bit for standard mapping.
 * We make an exception for the dpad, it can be two two-way axes or an eight-way hat.
 */
#define EGG_BTN_HORZ (EGG_BTN_LEFT|EGG_BTN_RIGHT)
#define EGG_BTN_VERT (EGG_BTN_UP|EGG_BTN_DOWN)
#define EGG_BTN_DPAD (EGG_BTN_HORZ|EGG_BTN_VERT)

struct inmgr_button {
// From device:
  int btnid;
  int hidusage;
  int lo,hi,rest; // Do not add buttons if (lo>=hi).
// Constant, from map:
  int dstbtnid; // Zero=unmapped, 1..0x8000=button, >0xffff=action.
  int srclo,srchi; // Inclusive "on" range for single bit or action. Inclusive thresholds for axes. Inclusive valid range for hats (srclo==srchi-7).
// Volatile mapping:
  int srcvalue,dstvalue;
};

struct inmgr_device {
  int devid;
  struct hostio_input *driver; // WEAK, null for (devid) zero.
  struct inmgr_button *buttonv;
  int buttonc,buttona;
  int playerid; // Zero if unassigned.
  int dstmask; // All Egg btnids we have mappings for.
  int state;
};

void inmgr_device_del(struct inmgr_device *device);
struct inmgr_device *inmgr_device_new();

int inmgr_device_query_config(struct inmgr *inmgr,struct inmgr_device *device);
int inmgr_device_init_keyboard(struct inmgr *inmgr,struct inmgr_device *device);

int inmgr_device_buttonv_search(struct inmgr_device *device,int btnid);
struct inmgr_button *inmgr_device_buttonv_get(struct inmgr_device *device,int btnid);

#endif
