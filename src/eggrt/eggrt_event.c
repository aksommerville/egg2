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
 
static void eggrt_quantize_pcm(int16_t *dst,const float *l,const float *r,int framec,int dstchanc) {
  #define QSAMPLE(src) (((src)<=-1.0f)?-32768:((src)>=1.0f)?32767:(int)((src)*32767.0f))
  if (dstchanc==1) { // mono...
    for (;framec-->0;l++,dst++) {
      *dst=QSAMPLE(*l);
    }
  } else if ((dstchanc==2)&&r) { // regular stereo...
    for (;framec-->0;l++,r++,dst+=2) {
      dst[0]=QSAMPLE(*l);
      dst[1]=QSAMPLE(*r);
    }
  } else { // generic...
    int extrac=dstchanc-2;
    for (;framec-->0;l++) {
      *dst++=QSAMPLE(*l);
      if (dstchanc>=2) {
        if (r) *dst++=QSAMPLE(*r);
        else *dst++=0;
        int i=extrac; while (i-->0) *dst++=0;
      }
      if (r) r++;
    }
  }
  #undef QSAMPLE
}
 
void eggrt_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  int framec=c/driver->chanc;
  float *bufl=synth_get_buffer(0),*bufr=0;
  if (!bufl) {
    memset(v,0,c);
    return;
  }
  if (driver->chanc>=2) bufr=synth_get_buffer(1);
  while (framec>0) {
    int updc=(framec>eggrt.audio_buffer)?eggrt.audio_buffer:framec;
    synth_update(updc);
    eggrt_quantize_pcm(v,bufl,bufr,updc,driver->chanc);
    v+=updc*driver->chanc;
    framec-=updc;
  }
}

/* Key from system keyboard.
 */
 
int eggrt_cb_key(struct hostio_video *driver,int keycode,int value) {
  inmgr_event(eggrt.devid_keyboard,keycode,value);
  return 1;
}

/* System pointer.
 */
 
void eggrt_cb_mmotion(struct hostio_video *driver,int x,int y) {
  render_coords_fb_from_win(eggrt.render,&x,&y);
  // Allow OOB but only by 1 pixel. We can't know how reliable OOB mouse reporting is across platforms, so keep it tight.
  if (x<0) x=-1; else if (x>=eggrt.metadata.fbw) x=eggrt.metadata.fbw;
  if (y<0) y=-1; else if (y>=eggrt.metadata.fbh) y=eggrt.metadata.fbh;
  eggrt.mousex=x;
  eggrt.mousey=y;
}

void eggrt_cb_mbutton(struct hostio_video *driver,int btnid,int value) {
  switch (btnid) {
    case 1: inmgr_artificial_event(0,EGG_BTN_SOUTH,value); break; // Left
    case 2: inmgr_artificial_event(0,EGG_BTN_WEST,value); break; // Right
    case 3: inmgr_artificial_event(0,EGG_BTN_EAST,value); break; // Middle
  }
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
  // We don't actually quit from here. Maybe we should. But both QUIT and MENU buttons from inmgr map to this, the Universal Menu.
  if (eggrt.umenu) {
    umenu_del(eggrt.umenu);
    eggrt.umenu=0;
  } else {
    eggrt.umenu=umenu_new(0);
  }
}
