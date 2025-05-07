#include "eggrt/eggrt_internal.h"

int inmgr_key(struct hostio_video *driver,int keycode,int value) {
  struct egg_event *event;
  if (event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_KEY)) {
    // Raw KEY event, cool.
    event->key.keycode=keycode;
    event->key.value=value;
    
  } else if (eggrt.inmgr.evtmask&(1<<EGG_EVENT_TEXT)) {
    // TEXT enabled: Return 0 to request WM mapping, and do not map to a player.
    return 0;
    
  } else if ((value==0)||(value==1)) {
    // State zero or one, map to players.
    inmgr_mappable_event(&eggrt.inmgr,0,keycode,value);
  }
  // Request text mapping from WM if TEXT enabled (duplicate clause, in case KEY is also enabled).
  return (eggrt.inmgr.evtmask&(1<<EGG_EVENT_TEXT))?0:1;
}

void inmgr_text(struct hostio_video *driver,int codepoint) {
  struct egg_event *event;
  if (event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_TEXT)) {
    event->text.codepoint=codepoint;
  }
}

void inmgr_mmotion(struct hostio_video *driver,int x,int y) {
  eggrt.inmgr.mousex=x;
  eggrt.inmgr.mousey=y;
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_MMOTION);
  if (!event) return;
  if (eggrt.inmgr.evtmask&(1<<EGG_EVENT_NOMAPCURSOR)) {
    // Report exactly as the window manager did; should be relative to the window.
  } else {
    // Transform into framebuffer space.
    render_coords_fb_from_win(eggrt.render,&x,&y);
  }
  event->mmotion.x=x;
  event->mmotion.y=y;
}

void inmgr_mbutton(struct hostio_video *driver,int btnid,int value) {
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_MBUTTON);
  if (!event) return;
  event->mbutton.x=eggrt.inmgr.mousex;
  event->mbutton.y=eggrt.inmgr.mousey;
  if (!(eggrt.inmgr.evtmask&(1<<EGG_EVENT_NOMAPCURSOR))) {
    render_coords_fb_from_win(eggrt.render,&event->mbutton.x,&event->mbutton.y);
  }
  event->mbutton.btnid=btnid;
  event->mbutton.value=value;
}

void inmgr_mwheel(struct hostio_video *driver,int dx,int dy) {
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_MWHEEL);
  if (!event) return;
  event->mwheel.x=eggrt.inmgr.mousex;
  event->mwheel.y=eggrt.inmgr.mousey;
  if (!(eggrt.inmgr.evtmask&(1<<EGG_EVENT_NOMAPCURSOR))) {
    render_coords_fb_from_win(eggrt.render,&event->mwheel.x,&event->mwheel.y);
  }
  event->mwheel.dx=dx;
  event->mwheel.dy=dy;
}

void inmgr_connect(struct hostio_input *driver,int devid) {
  //TODO Internal gamepad bookkeeping.
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_GAMEPAD);
  if (event) {
    event->gamepad.devid=devid;
    event->gamepad.btnid=0;
    event->gamepad.value=1;
  }
}

void inmgr_disconnect(struct hostio_input *driver,int devid) {
  //TODO Tear down internal gamepad bookkeeping
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_GAMEPAD);
  if (event) {
    event->gamepad.devid=devid;
    event->gamepad.btnid=0;
    event->gamepad.value=0;
  }
}

void inmgr_button(struct hostio_input *driver,int devid,int btnid,int value) {
  if (!devid||!btnid) return; // (devid) and (btnid) zero are reserved, for keyboards and connection events.
  struct egg_event *event=inmgr_evtq_push(&eggrt.inmgr,EGG_EVENT_GAMEPAD);
  if (event) {
    // Client is taking raw gamepads.
    event->gamepad.devid=devid;
    event->gamepad.btnid=btnid;
    event->gamepad.value=value;
  } else {
    // The recommended case, map them ourselves.
    inmgr_mappable_event(&eggrt.inmgr,devid,btnid,value);
  }
}
