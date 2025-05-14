#include "eggrt/eggrt_internal.h"

/* Quit.
 */
 
void inmgr_quit(struct inmgr *inmgr) {
  if (inmgr->devicev) {
    while (inmgr->devicec-->0) inmgr_device_del(inmgr->devicev[inmgr->devicec]);
    free(inmgr->devicev);
  }
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
  
  if (eggrt.hostio&&eggrt.hostio->video) {
    if (eggrt.hostio->video->type->provides_input) {
      if (inmgr_device_connect(inmgr,0,0)<0) return -1;
    }
  }

  return 0;
}

/* Update.
 */
 
int inmgr_update(struct inmgr *inmgr) {
  return 0;
}

/* Device list.
 */
 
int inmgr_find_device_by_devid(struct inmgr *inmgr,int devid) {
  int i=0;
  struct inmgr_device **p=inmgr->devicev;
  for (;i<inmgr->devicec;i++,p++) {
    if ((*p)->devid!=devid) continue;
    return i;
  }
  return -1;
}
 
struct inmgr_device *inmgr_get_device_by_devid(struct inmgr *inmgr,int devid) {
  int i=0;
  struct inmgr_device **p=inmgr->devicev;
  for (;i<inmgr->devicec;i++,p++) {
    if ((*p)->devid!=devid) continue;
    return *p;
  }
  return 0;
}
