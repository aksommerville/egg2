#include "../demo.h"
#include "../gui/gui.h"

/* Including player zero.
 */
#define PLAYERC 9

#define CHEATCODE_TIME 0.500

struct modal_input {
  struct modal hdr;
  struct gui_term *term;
  int brow;
  int pvinput[PLAYERC];
  double cheatcode_start_time;
  int cheatcode_count;
  int cheatcode_btnid;
};

#define MODAL ((struct modal_input*)modal)

/* Delete.
 */
 
static void _input_del(struct modal *modal) {
  gui_term_del(MODAL->term);
}

/* Append event to term.
 * We also update the cheatcode, since it's a convenient place to.
 */
 
static const char *modal_input_btnid_repr(int btnid) {
  switch (btnid) {
    #define _(tag) case EGG_BTN_##tag: return #tag;
    _(LEFT) _(RIGHT) _(UP) _(DOWN)
    _(SOUTH) _(WEST) _(EAST) _(NORTH)
    _(L1) _(R1) _(L2) _(R2)
    _(AUX1) _(AUX2) _(AUX3)
    _(CD)
    #undef _
  }
  return "?";
}
 
static void modal_input_log_event(struct modal *modal,int playerid,int btnid,int value) {
  gui_term_scroll(MODAL->term,0,-1);
  gui_term_writef(MODAL->term,1,MODAL->brow,"%d.%04x=%d (%s)",playerid,btnid,value,modal_input_btnid_repr(btnid));
  
  if (value&&!playerid) {
    double now=egg_time_real();
    if ((now-MODAL->cheatcode_start_time>=CHEATCODE_TIME)||(btnid!=MODAL->cheatcode_btnid)) {
      MODAL->cheatcode_count=0;
      MODAL->cheatcode_start_time=now;
      MODAL->cheatcode_btnid=btnid;
    }
    if (++(MODAL->cheatcode_count)>=3) {
      modal->defunct=1;
    }
  }
}

/* Update.
 */
 
static void _input_update(struct modal *modal,double elapsed,int input,int pvinput) {

  int inputv[PLAYERC];
  egg_input_get_all(inputv,PLAYERC);
  int *pv=MODAL->pvinput,*nx=inputv;
  int i=PLAYERC,playerid=0;
  for (;i-->0;pv++,nx++,playerid++) {
    if (*pv==*nx) continue;
    int btnid=0x8000;
    for (;btnid;btnid>>=1) {
      if (((*pv)&btnid)&&!((*nx)&btnid)) modal_input_log_event(modal,playerid,btnid,0);
      else if (!((*pv)&btnid)&&((*nx)&(btnid))) modal_input_log_event(modal,playerid,btnid,1);
    }
    *pv=*nx;
  }

  gui_term_update(MODAL->term,elapsed);
}

/* Render.
 */
 
static void _input_render(struct modal *modal) {
  if (MODAL->cheatcode_count>=2) {
    if (egg_time_real()-MODAL->cheatcode_start_time<CHEATCODE_TIME) {
      graf_fill_rect(&g.graf,0,0,FBW,FBH,0xff000080);
    }
  }
  gui_term_render(MODAL->term);
  int colw=FBW/PLAYERC;
  int x=(FBW>>1)-((colw*PLAYERC)>>1);
  x+=colw>>1;
  int playerid=0;
  const int *state=MODAL->pvinput;
  graf_set_image(&g.graf,RID_image_input);
  for (;playerid<PLAYERC;playerid++,x+=colw,state++) {
    graf_tile(&g.graf,x,FBH-20,0x00,0);
    if (*state) {
      int btnid=1,srccol=0,srcrow=1;
      for (;btnid<=0x8000;btnid<<=1) {
        if ((*state)&btnid) {
          graf_tile(&g.graf,x,FBH-20,(srcrow<<4)|srccol,0);
        }
        if (++srccol>=4) { srccol=0; srcrow++; }
      }
    }
    graf_tile(&g.graf,x,FBH-52,0x01+playerid,0);
  }
}

/* Initialize.
 */
 
static int _input_init(struct modal *modal) {
  modal->del=_input_del;
  modal->update=_input_update;
  modal->render=_input_render;
  modal->suppress_exit=1;
  
  if (!(MODAL->term=gui_term_new(0,0,FBW,FBH-56))) return -1;
  gui_term_set_background(MODAL->term,0x00000000);
  int colc;
  gui_term_get_size(&colc,&MODAL->brow,MODAL->term);
  MODAL->brow-=1;
  
  gui_term_write(MODAL->term,1,MODAL->brow,"Any key x3 to exit.",-1);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_input() {
  struct modal *modal=modal_new(sizeof(struct modal_input));
  if (!modal) return 0;
  if (
    (_input_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
