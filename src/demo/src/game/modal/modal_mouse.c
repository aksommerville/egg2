#include "../demo.h"
#include "../gui/gui.h"

#define CLICK_LIMIT 16
#define CLICK_TTL 1.000

struct modal_mouse {
  struct modal hdr;
  struct click {
    int x,y;
    double ttl;
  } clickv[CLICK_LIMIT];
  int clickc;
};

#define MODAL ((struct modal_mouse*)modal)

/* Delete.
 */
 
static void _mouse_del(struct modal *modal) {
}

/* Click.
 */
 
static void mouse_on_click(struct modal *modal) {
  if (MODAL->clickc>=CLICK_LIMIT) return;
  int x,y;
  if (!egg_input_get_mouse(&x,&y)) return;
  struct click *click=MODAL->clickv+MODAL->clickc++;
  click->x=x;
  click->y=y;
  click->ttl=CLICK_TTL;
}

/* Update.
 */
 
static void _mouse_update(struct modal *modal,double elapsed,int input,int pvinput) {

  int i=MODAL->clickc;
  struct click *click=MODAL->clickv+i-1;
  for (;i-->0;click--) {
    if ((click->ttl-=elapsed)<=0.0) {
      MODAL->clickc--;
      memmove(click,click+1,sizeof(struct click)*(MODAL->clickc-i));
    }
  }

  if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) {
    modal->defunct=1;
    egg_input_set_mode(EGG_INPUT_MODE_GAMEPAD);
  }
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    mouse_on_click(modal);
  }
}

/* Render.
 */
 
static void _mouse_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x008000ff);
  
  struct click *click=MODAL->clickv;
  int i=MODAL->clickc;
  for (;i-->0;click++) {
    int alpha=(click->ttl*255.0)/CLICK_TTL;
    if (alpha<1) alpha=1; else if (alpha>0xff) alpha=0xff;
    graf_fill_rect(&g.graf,click->x-2,click->y-2,5,5,0xffffff00|alpha);
  }
  
  int x=0,y=0;
  if (egg_input_get_mouse(&x,&y)) {
    graf_fill_rect(&g.graf,x-1,y-1,3,3,0xffff00ff);
  }
}

/* Initialize.
 */
 
static int _mouse_init(struct modal *modal) {
  modal->del=_mouse_del;
  modal->update=_mouse_update;
  modal->render=_mouse_render;
  modal->suppress_exit=1;
  
  egg_input_set_mode(EGG_INPUT_MODE_MOUSE);
  return 0;
}

/* New.
 */
 
struct modal *modal_new_mouse() {
  struct modal *modal=modal_new(sizeof(struct modal_mouse));
  if (!modal) return 0;
  if (
    (_mouse_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
