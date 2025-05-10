/* inmgr.h
 * Input manager, for managing input.
 */
 
#ifndef INMGR_H
#define INMGR_H

#define EGGRT_PLAYER_LIMIT 8
#define EGGRT_EVTQ_SIZE 256 /* Arbitrary. If the event queue doesn't get read, we overwrite after so many. */

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

  /* (evtp) in 0..EGGRT_EVTQ_SIZE-1.
   * (evtc) in 0..EGGRT_EVTQ_SIZE.
   * (evtq[evtp]) is the oldest queued event.
   */
  struct egg_event evtq[EGGRT_EVTQ_SIZE];
  int evtp,evtc;
  
  /* Bitfields, (1<<EGG_EVENT_*).
   * (evtmask_capable) are the events we think we can produce, judged from the input and video drivers.
   */
  uint32_t evtmask;
  uint32_t evtmask_capable;
  
  // Most recent mouse position, unmapped.
  int mousex,mousey;
  
  struct inmgr_device **devicev;
  int devicec,devicea;
};

void inmgr_quit(struct inmgr *inmgr);
int inmgr_init(struct inmgr *inmgr);
int inmgr_update(struct inmgr *inmgr);

/* If (type) is relevant, push a new event on the queue and return.
 * The event will be zero except for (type).
 * If this type is not being listened for, return null.
 */
struct egg_event *inmgr_evtq_push(struct inmgr *inmgr,int type);

/* Copy up to (dsta) events and return count copied.
 * Leaves some events queued, if (dst) is not long enough.
 */
int inmgr_evtq_pop(struct egg_event *dst,int dsta,struct inmgr *inmgr);

/* Change event mask.
 * You can read it directly, but only use this function to modify it.
 */
int inmgr_event_enable(struct inmgr *inmgr,int evttype,int enable);

/* hostio event hooks.
 * These can be assigned directly to the video and input drivers.
 * We'll use globals (eggrt.inmgr).
 */
int inmgr_key(struct hostio_video *driver,int keycode,int value);
void inmgr_text(struct hostio_video *driver,int codepoint);
void inmgr_mmotion(struct hostio_video *driver,int x,int y);
void inmgr_mbutton(struct hostio_video *driver,int btnid,int value);
void inmgr_mwheel(struct hostio_video *driver,int dx,int dy);
void inmgr_connect(struct hostio_input *driver,int devid);
void inmgr_disconnect(struct hostio_input *driver,int devid);
void inmgr_button(struct hostio_input *driver,int devid,int btnid,int value);

/* Device inspection, matching Egg Platform API exactly.
 */
int inmgr_get_device_name(char *dst,int dsta,int *vid,int *pid,int *version,struct inmgr *inmgr,int devid);
int inmgr_get_device_button(int *btnid,int *hidusage,int *lo,int *hi,int *rest,struct inmgr *inmgr,int devid,int btnix);

/* Internal use only.
 ***************************************************************************************************/
 
void inmgr_map_keyboard(struct inmgr *inmgr);
void inmgr_unmap_keyboard(struct inmgr *inmgr);
void inmgr_map_gamepads(struct inmgr *inmgr);
void inmgr_unmap_gamepads(struct inmgr *inmgr);
void inmgr_show_cursor(struct inmgr *inmgr,int show);
void inmgr_lock_cursor(struct inmgr *inmgr,int lock);

void inmgr_mappable_event(struct inmgr *inmgr,int devid,int btnid,int value);

/* If either of these fails, don't report the event to the client.
 * Disconnect will fail if the device isn't registered (which would be the case if its connect had failed).
 * Any inmgr_mappable_event() on a device whose inmgr_device_connect() failed will quietly noop.
 * The system keyboard should have (0,0) for (driver,devid); these must be nonzero for all other devices.
 * Connecting or disconnecting DOES NOT queue an event. Caller should if appropriate.
 */
int inmgr_device_connect(struct inmgr *inmgr,struct hostio_input *driver,int devid);
int inmgr_device_disconnect(struct inmgr *inmgr,struct hostio_input *driver,int devid);

int inmgr_find_device_by_devid(struct inmgr *inmgr,int devid);
struct inmgr_device *inmgr_get_device_by_devid(struct inmgr *inmgr,int devid);

void inmgr_device_require_player(struct inmgr *inmgr,struct inmgr_device *device);

#endif
