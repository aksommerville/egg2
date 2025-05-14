/* inmgr.h
 * Input manager, for managing input.
 */
 
#ifndef INMGR_H
#define INMGR_H

#define EGGRT_PLAYER_LIMIT 8

/* Stateless actions mappable to keyboard.
 * There's no technical need for it, but for my sanity, keep these the same as src/web/js/Input.js:ACTION_*.
 */
#define INMGR_ACTION_QUIT                 0x00010001
#define INMGR_ACTION_FULLSCREEN           0x00010002 /* toggle */

struct inmgr;

#include "inmgr_device.h"

struct inmgr {

  /* (playerclo,playerchi) are declared in metadata:1.
   * Our owner should populate these before init, and we sanitize aggressively at init.
   * (playerc) is the count of player states *including the aggregate zero*.
   */
  int playerclo,playerchi;
  int playerc;
  int playerv[1+EGGRT_PLAYER_LIMIT];
  
  struct inmgr_device **devicev;
  int devicec,devicea;
};

void inmgr_quit(struct inmgr *inmgr);
int inmgr_init(struct inmgr *inmgr);
int inmgr_update(struct inmgr *inmgr);

/* hostio event hooks.
 * These can be assigned directly to the video and input drivers.
 * We'll use globals (eggrt.inmgr).
 */
int inmgr_key(struct hostio_video *driver,int keycode,int value);
void inmgr_connect(struct hostio_input *driver,int devid);
void inmgr_disconnect(struct hostio_input *driver,int devid);
void inmgr_button(struct hostio_input *driver,int devid,int btnid,int value);

/* Internal use only.
 ***************************************************************************************************/

void inmgr_mappable_event(struct inmgr *inmgr,int devid,int btnid,int value);

/* Disconnect will fail if the device isn't registered (which would be the case if its connect had failed).
 * Any inmgr_mappable_event() on a device whose inmgr_device_connect() failed will quietly noop.
 * The system keyboard should have (0,0) for (driver,devid); these must be nonzero for all other devices.
 */
int inmgr_device_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
int inmgr_device_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid);

int inmgr_find_device_by_devid(struct inmgr *inmgr,int devid);
struct inmgr_device *inmgr_get_device_by_devid(struct inmgr *inmgr,int devid);

void inmgr_device_require_player(struct inmgr *inmgr,struct inmgr_device *device);

#endif
