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
