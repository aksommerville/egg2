#include "eggrt/eggrt_internal.h"

/* Quit.
 */
 
void inmgr_quit(struct inmgr *inmgr) {
}

/* Init.
 */
 
int inmgr_init(struct inmgr *inmgr) {

  inmgr->playerclo=eggrt.metadata.playerclo;
  inmgr->playerchi=eggrt.metadata.playerchi;
  if (inmgr->playerclo<1) inmgr->playerclo=1;
  if (inmgr->playerchi>EGGRT_PLAYER_LIMIT) inmgr->playerchi=EGGRT_PLAYER_LIMIT;
  if (inmgr->playerclo>inmgr->playerchi) inmgr->playerclo=inmgr->playerchi;
  inmgr->playerc=1+inmgr->playerchi;
  
  // (evtmask) is initially zero, that's mandated by our spec.
  
  inmgr->evtmask_capable=0;
  if (eggrt.hostio&&eggrt.hostio->video) {
    if (eggrt.hostio->video->type->provides_input) {
      inmgr->evtmask_capable|=(
        EGG_EVENT_KEY|
        EGG_EVENT_TEXT|
        EGG_EVENT_MMOTION|
        EGG_EVENT_MBUTTON|
        EGG_EVENT_MWHEEL|
        EGG_EVENT_NOMAPCURSOR|
      0);
      if (eggrt.hostio->video->type->show_cursor) {
        inmgr->evtmask_capable|=EGG_EVENT_HIDECURSOR;
      }
      if (eggrt.hostio->video->type->lock_cursor) {
        inmgr->evtmask_capable|=EGG_EVENT_LOCKCURSOR;
      }
    }
  }
  if (eggrt.hostio&&eggrt.hostio->inputc) {
    inmgr->evtmask_capable|=EGG_EVENT_GAMEPAD;
  }
  // Never set: TOUCH
  //TODO evdev does deal with touch devices. Get equipped to test that, and implement it.
  
  inmgr_map_keyboard(inmgr);

  return 0;
}

/* Update.
 */
 
int inmgr_update(struct inmgr *inmgr) {
  return 0;
}

/* Push to event queue.
 */
 
struct egg_event *inmgr_evtq_push(struct inmgr *inmgr,int type) {
  if ((type<0)||(type>=32)||!(inmgr->evtmask_capable&(1<<type))) return 0;
  int p=inmgr->evtp+inmgr->evtc;
  if (p>=EGGRT_EVTQ_SIZE) p-=EGGRT_EVTQ_SIZE;
  struct egg_event *event=inmgr->evtq+p;
  memset(event,0,sizeof(struct egg_event));
  event->type=type;
  if (inmgr->evtc<EGGRT_EVTQ_SIZE) inmgr->evtc++;
  else inmgr->evtp++;
  return event;
}

/* Pop from event queue.
 */
 
int inmgr_evtq_pop(struct egg_event *dst,int dsta,struct inmgr *inmgr) {
  if (!dst||(dsta<1)) return 0;
  int dstc=inmgr->evtc;
  if (dstc>dsta) dstc=dsta;
  int cpc_head=EGGRT_EVTQ_SIZE-inmgr->evtp;
  if (cpc_head>dstc) cpc_head=dstc;
  memcpy(dst,inmgr->evtq+inmgr->evtp,sizeof(struct egg_event)*cpc_head);
  if ((inmgr->evtp+=cpc_head)>=EGGRT_EVTQ_SIZE) {
    int cpc_tail=dstc-cpc_head;
    memcpy(dst+cpc_head,inmgr->evtq,sizeof(struct egg_event)*cpc_tail);
    inmgr->evtp=cpc_tail;
  }
  inmgr->evtc-=dstc;
  return dstc;
}

/* Event just enabled.
 * Do whatever needs done to the drivers, mappings, any kind of reaction.
 */
 
static void inmgr_event_enable_on(struct inmgr *inmgr,int evttype) {
  switch (evttype) {

    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT: {
        inmgr_unmap_keyboard(inmgr);
      } break;
      
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL: {
        if (!(inmgr->evtmask&((1<<EGG_EVENT_HIDECURSOR)|(1<<EGG_EVENT_LOCKCURSOR)))) {
          inmgr_show_cursor(inmgr,1);
        }
      } break;
      
    case EGG_EVENT_GAMEPAD: {
        inmgr_unmap_gamepads(inmgr);
      } break;
      
    case EGG_EVENT_HIDECURSOR: {
        inmgr_show_cursor(inmgr,0);
      } break;
      
    case EGG_EVENT_LOCKCURSOR: {
        inmgr_lock_cursor(inmgr,1);
      } break;
  }
}

/* Event just disabled.
 * Do whatever needs done to the drivers, mappings, any kind of reaction.
 */
 
static void inmgr_event_enable_off(struct inmgr *inmgr,int evttype) {
  switch (evttype) {

    case EGG_EVENT_KEY:
    case EGG_EVENT_TEXT: {
        inmgr_map_keyboard(inmgr);
      } break;
      
    case EGG_EVENT_MMOTION:
    case EGG_EVENT_MBUTTON:
    case EGG_EVENT_MWHEEL: {
        if (!(inmgr->evtmask&((1<<EGG_EVENT_MMOTION)|(1<<EGG_EVENT_MBUTTON)|(1<<EGG_EVENT_MWHEEL)))) {
          inmgr_show_cursor(inmgr,0);
        }
      } break;
      
    case EGG_EVENT_GAMEPAD: {
        inmgr_map_gamepads(inmgr);
      } break;
      
    case EGG_EVENT_HIDECURSOR: {
        if (inmgr->evtmask&((1<<EGG_EVENT_MMOTION)|(1<<EGG_EVENT_MBUTTON)|(1<<EGG_EVENT_MWHEEL))) {
          if (!(inmgr->evtmask&(1<<EGG_EVENT_LOCKCURSOR))) {
            inmgr_show_cursor(inmgr,1);
          }
        }
      } break;
      
    case EGG_EVENT_LOCKCURSOR: {
        inmgr_lock_cursor(inmgr,0);
      } break;
  }
}

/* Modify event mask.
 */
 
int inmgr_event_enable(struct inmgr *inmgr,int evttype,int enable) {
  if ((evttype<0)||(evttype>=32)) return -1;
  uint32_t bit=1<<evttype;
  if (enable) {
    if (inmgr->evtmask&bit) return 0;
    if (!(inmgr->evtmask_capable&bit)) return -1;
    inmgr->evtmask|=bit;
    inmgr_event_enable_on(inmgr,evttype);
  } else {
    if (!(inmgr->evtmask&bit)) return 0;
    inmgr->evtmask&=~bit;
    inmgr_event_enable_off(inmgr,evttype);
  }
  return 0;
}

/* Drop or reapply maps for all devices of source type.
 * (in response to an event mask change).
 */
 
void inmgr_map_keyboard(struct inmgr *inmgr) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void inmgr_unmap_keyboard(struct inmgr *inmgr) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void inmgr_map_gamepads(struct inmgr *inmgr) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void inmgr_unmap_gamepads(struct inmgr *inmgr) {
  fprintf(stderr,"TODO %s\n",__func__);
}

/* Cursor state changes.
 */
 
void inmgr_show_cursor(struct inmgr *inmgr,int show) {
  if (eggrt.hostio->video&&eggrt.hostio->video->type->show_cursor) {
    eggrt.hostio->video->type->show_cursor(eggrt.hostio->video,show);
  }
}

void inmgr_lock_cursor(struct inmgr *inmgr,int lock) {
  if (eggrt.hostio->video&&eggrt.hostio->video->type->lock_cursor) {
    eggrt.hostio->video->type->lock_cursor(eggrt.hostio->video,lock);
  }
}
