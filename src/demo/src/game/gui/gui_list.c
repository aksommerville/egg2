#include "../demo.h"
#include "gui.h"

struct gui_list {
  int x,y,w,h;
  void *userdata;
  void (*cb_activate)(struct gui_list *list,int optionid);
  void (*cb_adjust)(struct gui_list *list,int optionid,int dx);
  void (*cb_focus)(struct gui_list *list,int optionid,int focus);
  int optionp;
  struct option {
    int optionid;
    char *text;
    int textc;
    int enable;
    int texid,texw,texh;
    void (*cb_activate)(struct gui_list *list,int optionid);
    void (*cb_adjust)(struct gui_list *list,int optionid,int dx);
    void (*cb_focus)(struct gui_list *list,int optionid,int focus);
  } *optionv;
  int optionc,optiona;
};

/* Delete.
 */
 
static void option_cleanup(struct option *option) {
  if (option->text) free(option->text);
  if (option->texid) egg_texture_del(option->texid);
}

void gui_list_del(struct gui_list *list) {
  if (!list) return;
  if (list->optionv) {
    while (list->optionc-->0) option_cleanup(list->optionv+list->optionc);
    free(list->optionv);
  }
  free(list);
}

/* New.
 */

struct gui_list *gui_list_new(int x,int y,int w,int h) {
  if ((w<1)||(h<1)) return 0;
  struct gui_list *list=calloc(1,sizeof(struct gui_list));
  if (!list) return 0;
  list->x=x;
  list->y=y;
  list->w=w;
  list->h=h;
  list->optionp=-1;
  return list;
}

/* Trivial accessors.
 */
 
void *gui_list_get_userdata(const struct gui_list *list) {
  if (!list) return 0;
  return list->userdata;
}

void gui_list_set_userdata(struct gui_list *list,void *userdata) {
  if (!list) return;
  list->userdata=userdata;
}

void gui_list_cb_activate(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid)) {
  if (!list) return;
  list->cb_activate=cb;
}

void gui_list_cb_adjust(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid,int dx)) {
  if (!list) return;
  list->cb_adjust=cb;
}

void gui_list_cb_focus(struct gui_list *list,int optionid,void (*cb)(struct gui_list *list,int optionid,int focus)) {
  if (!list) return;
  list->cb_focus=cb;
}

int gui_list_get_selection(const struct gui_list *list) {
  if (!list) return 0;
  if ((list->optionp<0)||(list->optionp>=list->optionc)) return 0;
  return list->optionv[list->optionp].optionid;
}

/* Replace text and texture.
 */
 
static int gui_list_replace_text(struct gui_list *list,struct option *option,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (option->text) free(option->text);
  option->text=nv;
  option->textc=srcc;
  int err=font_render_to_texture(option->texid,g.font,src,srcc,list->w,list->h,0xffffffff);
  if (err<0) return -1;
  option->texid=err;
  egg_texture_get_size(&option->texw,&option->texh,option->texid);
  return 0;
}

/* Next unused optionid.
 * We won't fill gaps, it's just the highest extant id plus one.
 */
 
static int gui_list_next_id(const struct gui_list *list) {
  if (!list) return -1;
  if (list->optionc<1) return 1;
  int max=0,i=list->optionc;
  struct option *option=list->optionv;
  for (;i-->0;option++) {
    if (option->optionid>=max) max=option->optionid;
  }
  if (max>=INT_MAX) return -1;
  return max+1;
}

/* Insert, public.
 */

int gui_list_insert(struct gui_list *list,int p,int optionid,const char *text,int textc,int enable) {
  if (!optionid) optionid=gui_list_next_id(list);
  if (optionid<0) return -1;
  if (gui_list_index_by_optionid(list,optionid)>=0) return -1;
  if (p==-1) p=list->optionc;
  if ((p<0)||(p>list->optionc)) return -1;
  if (list->optionc>=list->optiona) {
    int na=list->optiona+16;
    if (na>INT_MAX/sizeof(struct option)) return -1;
    void *nv=realloc(list->optionv,sizeof(struct option)*na);
    if (!nv) return -1;
    list->optionv=nv;
    list->optiona=na;
  }
  struct option *option=list->optionv+p;
  memmove(option+1,option,sizeof(struct option)*(list->optionc-p));
  list->optionc++;
  memset(option,0,sizeof(struct option));
  option->optionid=optionid;
  option->enable=enable;
  if (gui_list_replace_text(list,option,text,textc)<0) {
    option_cleanup(option);
    list->optionc--;
    memmove(option,option+1,sizeof(struct option)*(list->optionc-p));
    return -1;
  }
  if (list->optionp>=p) list->optionp++;
  return optionid;
}

/* Remove, public.
 */
 
int gui_list_remove(struct gui_list *list,int optionid) {
  int p=gui_list_index_by_optionid(list,optionid);
  if (p<0) return -1;
  struct option *option=list->optionv+p;
  option_cleanup(option);
  list->optionc--;
  memmove(option,option+1,sizeof(struct option)*(list->optionc-p));
  if (list->optionp>p) list->optionp--;
  else if (list->optionp==p) list->optionp=-1;
  return 0;
}

/* Replace, public.
 */
 
int gui_list_replace(struct gui_list *list,int optionid,const char *text,int textc,int enable) {
  int p=gui_list_index_by_optionid(list,optionid);
  if (p<0) return -1;
  struct option *option=list->optionv+p;
  if (text||textc) {
    if (gui_list_replace_text(list,option,text,textc)<0) return -1;
  }
  option->enable=enable;
  return 0;
}

/* Index to/from optionid.
 */
 
int gui_list_optionid_by_index(const struct gui_list *list,int p) {
  if (!list) return 0;
  if ((p<0)||(p>=list->optionc)) return 0;
  return list->optionv[p].optionid;
}

int gui_list_index_by_optionid(const struct gui_list *list,int optionid) {
  if (!list) return -1;
  int p=0;
  const struct option *option=list->optionv;
  for (;p<list->optionc;p++,option++) {
    if (option->optionid==optionid) return p;
  }
  return -1;
}

/* Get text for option.
 */
 
int gui_list_text_by_optionid(void *dstpp,const struct gui_list *list,int optionid) {
  int p=gui_list_index_by_optionid(list,optionid);
  if (p<0) return 0;
  const struct option *option=list->optionv+p;
  *(void**)dstpp=option->text;
  return option->textc;
}

/* Events.
 */
 
static void gui_list_move(struct gui_list *list,int d) {
  if (list->optionc<1) return;
  egg_play_sound(RID_sound_uimotion,0.5,0.0);
  int panic=list->optionc;
  int np=list->optionp+d;
  while (panic-->0) {
    if (np<0) np=list->optionc-1;
    else if (np>=list->optionc) np=0;
    if (list->optionv[np].enable) break;
    np+=d;
  }
  if (np==list->optionp) return;
  struct option *option=list->optionv+np;
  if (!option->enable) {
    list->optionp=-1;
    return;
  }
  if ((list->optionp>=0)&&(list->optionp<list->optionc)) {
    struct option *pvoption=list->optionv+list->optionp;
    if (pvoption->enable) {
      if (pvoption->cb_focus) pvoption->cb_focus(list,pvoption->optionid,0);
      else if (list->cb_focus) list->cb_focus(list,pvoption->optionid,0);
    }
  }
  list->optionp=np;
  if (option->cb_focus) option->cb_focus(list,option->optionid,1);
  else if (list->cb_focus) list->cb_focus(list,option->optionid,1);
}
 
static void gui_list_adjust(struct gui_list *list,int d) {
  if ((list->optionp<0)||(list->optionp>=list->optionc)) return;
  struct option *option=list->optionv+list->optionp;
  if (!option->enable) return;
  if (option->cb_adjust) {
    egg_play_sound(RID_sound_uiadjust,0.5,(d<0)?-0.5:0.5);
    option->cb_adjust(list,option->optionid,d);
  } else if (list->cb_adjust) {
    egg_play_sound(RID_sound_uiadjust,0.5,(d<0)?-0.5:0.5);
    list->cb_adjust(list,option->optionid,d);
  }
}
 
static void gui_list_activate(struct gui_list *list) {
  if ((list->optionp<0)||(list->optionp>=list->optionc)) return;
  struct option *option=list->optionv+list->optionp;
  if (!option->enable) return;
  if (option->cb_activate) {
    egg_play_sound(RID_sound_uiactivate,0.5,0.0);
    option->cb_activate(list,option->optionid);
  } else if (list->cb_activate) {
    egg_play_sound(RID_sound_uiactivate,0.5,0.0);
    list->cb_activate(list,option->optionid);
  }
}

/* Update.
 */

void gui_list_update(struct gui_list *list,double elapsed,int input,int pvinput) {
  if (!list) return;
  if (input!=pvinput) {
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) gui_list_move(list,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) gui_list_move(list,1);
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) gui_list_adjust(list,-1);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) gui_list_adjust(list,1);
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) gui_list_activate(list);
  }
  // If we want an animated focus, this is the place to effect it. I think not.
}

/* Render.
 */
 
void gui_list_render(struct gui_list *list) {
  if (!list) return;
  
  /* Take the full height of all options.
   * They butt against each other snugly. The first renders at y==1.
   * Also measure (habove), the height above the focussed option.
   */
  int contenth=0,habove=0;
  int i=0;
  const struct option *option=list->optionv;
  for (;i<list->optionc;i++,option++) {
    contenth+=option->texh;
    if (i<list->optionp) habove+=option->texh;
  }
  
  /* If the content height is less than our box, the labels are stationary, at the top.
   * If nothing is focussed, same.
   * Otherwise, put the focussed label in the center and then clamp top and bottom.
   */
  int y0=1;
  if ((contenth<list->h)||(list->optionp<0)||(list->optionp>=list->optionc)) {
    // Cool, keep it at 1.
  } else {
    option=list->optionv+list->optionp;
    y0=(list->h>>1)-(option->texh>>1)-habove;
    if (y0>0) y0=0; // At the top of the list, stick to the top edge.
    else if (y0+contenth<list->h) y0=list->h-contenth; // At the bottom of the list, stick to the bottom edge.
  }
  y0+=list->y;
  int x=list->x+1;
  
  /* First draw the highlight. It's a bar as wide as the whole list, slightly taller than the focussed option.
   */
  if ((list->optionp>=0)&&(list->optionp<list->optionc)) {
    option=list->optionv+list->optionp;
    // Convention is for fonts to occupy their top row but not bottom. So cheat the highlight bar up but not down.
    int hy=y0+habove-1;
    int hh=option->texh+1;
    if (hy<list->y) {
      int trim=list->y-hy;
      hy+=trim;
      hh-=trim;
    }
    if (hy+hh>list->y+list->h) hh=list->y+list->h-hy;
    if (hh>0) {
      graf_set_input(&g.graf,0);
      graf_fill_rect(&g.graf,list->x,hy,list->w,hh,0x002080ff);
    }
  }
  
  /* In the very likely case that we cover the entire framebuffer, don't bother clipping.
   */
  if ((list->x==0)&&(list->y==0)&&(list->w==FBW)&&(list->h==FBH)) {
    int y=y0;
    for (i=0,option=list->optionv;i<list->optionc;i++,option++) {
      graf_set_input(&g.graf,option->texid);
      if (!option->enable) graf_set_alpha(&g.graf,0x80);
      graf_decal(&g.graf,x,y,0,0,option->texw,option->texh);
      graf_set_alpha(&g.graf,0xff);
      y+=option->texh;
    }
  
  /* If we occupy only a portion of the framebuffer, clip carefully.
   */
  } else {
    int y=y0;
    for (i=0,option=list->optionv;i<list->optionc;i++,option++) {
      int dstw=option->texw,dsth=option->texh;
      int dsty=y;
      if (x+dstw>list->x+list->w) dstw=list->x+list->w-x;
      if (y<list->y) {
        int trim=list->y-y;
        if ((dsth-=trim)<=0) { y+=option->texh; continue; }
        dsty+=trim;
      }
      if (dsty+dsth>list->y+list->h) {
        if ((dsth=list->y+list->h-dsty)<=0) { y+=option->texh; continue; }
      }
      graf_set_input(&g.graf,option->texid);
      if (!option->enable) graf_set_alpha(&g.graf,0x80);
      graf_decal(&g.graf,x,dsty,0,dsty-y,dstw,dsth);
      graf_set_alpha(&g.graf,0xff);
      y+=option->texh;
    }
  }
}
