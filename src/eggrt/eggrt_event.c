#include "eggrt_internal.h"

/* Close window.
 */
 
void eggrt_cb_close(struct hostio_video *driver) {
  eggrt.terminate=1;
}

/* Window gains or loses focus.
 */
 
void eggrt_cb_focus(struct hostio_video *driver,int focus) {
  if (focus) {
    if (eggrt.focus) return;
    eggrt.focus=1;
  } else {
    if (!eggrt.focus) return;
    eggrt.focus=0;
  }
  // Should we notify inmgr?
}

/* Window resized.
 */
 
void eggrt_cb_resize(struct hostio_video *driver,int w,int h) {
  render_set_size(eggrt.render,w,h);
}

/* Audio out.
 */
 
void eggrt_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  synth_updatei(v,c,eggrt.synth);
}

/* Key from system keyboard.
 */
 
int eggrt_cb_key(struct hostio_video *driver,int keycode,int value) {
  inmgr_event(eggrt.devid_keyboard,keycode,value);
  return 1;
}

/* Input events.
 */
 
static int eggrt_cb_incap(int btnid,int hidusage,int lo,int hi,int value,void *userdata) {
  inmgr_connect_more(*(int*)userdata,btnid,hidusage,lo,hi,value);
  return 0;
}

void eggrt_cb_connect(struct hostio_input *driver,int devid) {
  int vid=0,pid=0,version=0;
  const char *name=0;
  if (driver->type->get_ids) {
    name=driver->type->get_ids(&vid,&pid,&version,driver,devid);
  }
  inmgr_connect_begin(devid,vid,pid,version,name,-1);
  if (driver->type->for_each_button) {
    driver->type->for_each_button(driver,devid,eggrt_cb_incap,&devid);
  }
  inmgr_connect_end(devid);
}

void eggrt_cb_disconnect(struct hostio_input *driver,int devid) {
  inmgr_disconnect(devid);
}

void eggrt_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  inmgr_event(devid,btnid,value);
}

/* Signals via inmgr.
 */
 
void eggrt_cb_quit() {
  eggrt.terminate=1;
}
