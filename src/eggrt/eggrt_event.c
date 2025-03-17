#include "eggrt_internal.h"

/* Close window.
 */
 
void eggrt_cb_close(struct hostio_video *driver) {
  eggrt.terminate=1;
}

/* Window gains or loses focus.
 */
 
void eggrt_cb_focus(struct hostio_video *driver,int focus) {
  fprintf(stderr,"%s %d\n",__func__,focus);
}

/* Window resized.
 */
 
void eggrt_cb_resize(struct hostio_video *driver,int w,int h) {
  render_set_size(eggrt.render,w,h);
}

/* Keyboard.
 */
 
int eggrt_cb_key(struct hostio_video *driver,int keycode,int value) {
  fprintf(stderr,"%s 0x%08x=%d\n",__func__,keycode,value);
  return 0;
}

void eggrt_cb_text(struct hostio_video *driver,int codepoint) {
  fprintf(stderr,"%s U+%x\n",__func__,codepoint);
}

/* Mouse.
 */
 
void eggrt_cb_mmotion(struct hostio_video *driver,int x,int y) {
  fprintf(stderr,"%s %d,%d\n",__func__,x,y);
}

void eggrt_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
  fprintf(stderr,"%s %d=%d\n",__func__,btnid,value);
}

void eggrt_cb_mwheel(struct hostio_video *driver,int dx,int dy) {
  fprintf(stderr,"%s %+d,%+d\n",__func__,dx,dy);
}

/* Audio out.
 */
 
void eggrt_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  synth_updatei(v,c,eggrt.synth);
}

/* Gamepad.
 */
 
void eggrt_cb_connect(struct hostio_input *driver,int devid) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

void eggrt_cb_disconnect(struct hostio_input *driver,int devid) {
  fprintf(stderr,"%s %d\n",__func__,devid);
}

void eggrt_cb_button(struct hostio_input *driver,int devid,int btnid,int value) {
  fprintf(stderr,"%s %d.%d=%d\n",__func__,devid,btnid,value);
}
