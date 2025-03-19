/* inmgr.h
 * Input manager, for managing input.
 */
 
#ifndef INMGR_H
#define INMGR_H

#define EGGRT_PLAYER_LIMIT 8
#define EGGRT_EVTQ_SIZE 256 /* Arbitrary. If the event queue doesn't get read, we overwrite after so many. */

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

/* Internal use only.
 ***************************************************************************************************/
 
void inmgr_map_keyboard(struct inmgr *inmgr);
void inmgr_unmap_keyboard(struct inmgr *inmgr);
void inmgr_map_gamepads(struct inmgr *inmgr);
void inmgr_unmap_gamepads(struct inmgr *inmgr);
void inmgr_show_cursor(struct inmgr *inmgr,int show);
void inmgr_lock_cursor(struct inmgr *inmgr,int lock);

#endif
