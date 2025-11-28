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
};

#endif
