#ifndef UMENU_INTERNAL_H
#define UMENU_INTERNAL_H

#include "eggrt/eggrt_internal.h"
#include "opt/image/image.h"

extern const unsigned char menubits[];
extern const int menubits_length;

#define UMENU_ACTIONID_MUSIC  1
#define UMENU_ACTIONID_SOUND  2
#define UMENU_ACTIONID_LANG   3
#define UMENU_ACTIONID_INPUT  4
#define UMENU_ACTIONID_RESUME 5
#define UMENU_ACTIONID_QUIT   6

#define UMENU_WIDGET_LIMIT 6

#define INCFG_LABEL_LIMIT 3

struct umenu {
  int fbw,fbh;
  int tilesize; // 4,8,16,32
  int texid_tiles;
  struct umenu_widget {
    int actionid;
    int x,y,w; // Bounds for highlight purposes. Height is always tilesize.
    int nl,nr,nt,nb; // actionid, neighbors in each direction.
    uint8_t tileid; // 0..32
  } widgetv[UMENU_WIDGET_LIMIT];
  int widgetc;
  int widgetp;
  int pvinput;
  int defunct;
  int *langv;
  int langc,langa;
  int incfg_only;
  
  int incfg; // Nonzero if we're in Input Config mode.
  int inlistener;
  int incx,incy,incw,inch; // Output bounds of the gamepad pic. 32x16 naturally but could be scaled up.
  int incscale;
  // All 15 buttons are defined always. Order depends on the ROM's preference, and some may be marked unused.
  struct incfg_button {
    int dstbtnid;
    int x,y,w,h;
    int enable;
    int strix;
  } buttonv[15];
  int buttonc;
  int buttonp; // <0 until a device is selected
  double incblink;
  int incdevid;
  struct incfg_label {
    int texid;
    int x,y,w,h;
    uint32_t tint;
    int position;
  } labelv[INCFG_LABEL_LIMIT];
  int labelc;
};

void umenu_fill_rect(int x,int y,int w,int h,uint32_t rgba);
void umenu_blit_tile(struct umenu *umenu,int dstx,int dsty,uint8_t tileid);

void incfg_quit(struct umenu *umenu);
void incfg_begin(struct umenu *umenu);
void incfg_update(struct umenu *umenu,double elapsed);
void incfg_render(struct umenu *umenu);

#endif
