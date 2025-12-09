#include "umenu_internal.h"

#define BLINK_PERIOD 1.000

#define LABEL_POSITION_ABOVE 1
#define LABEL_POSITION_BELOW 2
#define LABEL_POSITION_BELOWER 3

#define TIMEOUT_DEVICE 10.0
#define TIMEOUT_BUTTON 10.0

/* Enable or disable all devices in inmgr.
 */
 
static void incfg_enable_all(struct umenu *umenu,int enable) {
  int p=0;
  for (;;p++) {
    int devid=inmgr_devid_by_index(p);
    if (devid<1) return;
    inmgr_device_enable(devid,enable);
  }
}

/* Cleanup.
 */
 
void incfg_quit(struct umenu *umenu) {
  inmgr_unlisten(umenu->inlistener);
  incfg_enable_all(umenu,1);
  umenu->incfg=0;
  struct incfg_label *label=umenu->labelv;
  int i=umenu->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
  umenu->labelc=0;
  if (umenu->capv) {
    free(umenu->capv);
    umenu->capv=0;
    umenu->capc=umenu->capa=0;
  }
}

/* Add a label.
 */
 
static void incfg_add_label(struct umenu *umenu,int position,const char *src,int srcc,uint32_t tint) {

  // If we already have one in this position, replace it.
  struct incfg_label *label=0;
  struct incfg_label *q=umenu->labelv;
  int i=umenu->labelc;
  for (;i-->0;q++) {
    if (q->position==position) {
      label=q;
      break;
    }
  }
  
  // Otherwise, add a new one.
  if (!label) {
    if (umenu->labelc>=INCFG_LABEL_LIMIT) return;
    label=umenu->labelv+umenu->labelc++;
    memset(label,0,sizeof(struct incfg_label));
  }
  
  // Generate texid if we don't have one.
  if (label->texid<1) {
    if ((label->texid=render_texture_new(eggrt.render))<1) return;
  }
  
  // Determine bounds.
  // We'll use a tilesize of 8, regardless of the menu's tilesize.
  const int glyphw=3;
  const int glyphh=5;
  const int xspace=4;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { src++; srcc--; }
  label->position=position;
  label->tint=tint;
  if (srcc) label->w=srcc*glyphw+(srcc-1);
  else label->w=1;
  label->h=glyphh;
  label->x=(umenu->fbw>>1)-(label->w>>1);
  switch (position) {
    case LABEL_POSITION_ABOVE: label->y=(umenu->incy>>1)-(label->h>>1); break;
    case LABEL_POSITION_BELOW:
    case LABEL_POSITION_BELOWER: {
        int y0=umenu->incy+umenu->inch;
        int availh=umenu->fbh-y0;
        label->y=availh/3;
        if (position==LABEL_POSITION_BELOWER) label->y<<=1;
        label->y+=y0;
        label->y-=label->h>>1;
      } break;
  }
  
  // Reallocate the texture.
  if (render_texture_load_raw(eggrt.render,label->texid,label->w,label->h,label->w<<2,0,0)<0) return;
  render_texture_clear(eggrt.render,label->texid);
  
  // Render each glyph in turn.
  int dstx=0;
  for (;srcc-->0;src++,dstx+=xspace) {
    int tileid=*src;
    if ((tileid<=0x20)||(tileid>=0x7f)) continue; // We do have glyphs for 0x20 and 0x7f but they're definitely blank.
    tileid-=0x20;
    int srcx=192+(tileid&15)*glyphw;
    int srcy=144+(tileid>>4)*glyphh;
    struct egg_render_raw vtxv[]={
      {dstx,0,srcx,srcy},
      {dstx,glyphh,srcx,srcy+glyphh},
      {dstx+glyphw,0,srcx+glyphw,srcy},
      {dstx+glyphw,glyphh,srcx+glyphw,srcy+glyphh},
    };
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=label->texid,
      .srctexid=umenu->texid_tiles,
      .alpha=0xff,
    };
    render_render(eggrt.render,&un,vtxv,sizeof(vtxv));
  }
}

/* Which part of a button is this value, per cap?
 */
 
#define BTNPART_OFF    0 /* All modes. */
#define BTNPART_ON     1 /* Two-state. */
#define BTNPART_POS    2 /* Signed. Right or Down. */
#define BTNPART_NEG    3 /* Signed. Left or Up. */
#define BTNPART_N      4 /* Hat... */
#define BTNPART_NE     5
#define BTNPART_E      6
#define BTNPART_SE     7
#define BTNPART_S      8
#define BTNPART_SW     9
#define BTNPART_W     10
#define BTNPART_NW    11 /* ...Hat. */

static int incfg_get_part(const struct incfg_cap *cap,int value) {
  // Try to judge button parts the same way as inmgr_connect.c:inmgr_button_make_up_generic_map.
  
  // Range less than 2 is invalid and we'll always call it OFF.
  int range=cap->hi-cap->lo+1;
  if (range<2) return BTNPART_OFF;
  
  // Range of 8 can only be a hat.
  if (range==8) {
    value-=cap->lo;
    if ((value>=0)&&(value<=7)) return BTNPART_N+value;
    return BTNPART_OFF;
  }
  
  // Signed range can only be a signed axis.
  // Ditto if the cap's reported value, which we take as the resting state, lies between the endpoints.
  if (
    ((cap->lo<0)&&(cap->hi>0))||
    ((cap->lo<cap->value)&&(cap->value<cap->hi))
  ) {
    // Cut the range in thirds. In live mapping, you'd want a narrower dead zone, but we are more inclined to ignore little movements.
    int mid=(cap->lo+cap->hi)>>1;
    int cutlo=cap->lo+range/3;
    int cuthi=cap->hi-range/3;
    if (cutlo>=mid) cutlo--;
    if (cuthi<=mid) cuthi++;
    if (value<=cutlo) return BTNPART_NEG;
    if (value>=cuthi) return BTNPART_POS;
    return BTNPART_OFF;
  }
  
  // Anything else is two-state. Values greater than the minimum are ON.
  if (value>cap->lo) return BTNPART_ON;
  return BTNPART_OFF;
}

/* Search caps list.
 */
 
static int incfg_capv_search(const struct umenu *umenu,int srcbtnid) {
  int lo=0,hi=umenu->capc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct incfg_cap *cap=umenu->capv+ck;
         if (srcbtnid<cap->srcbtnid) hi=ck;
    else if (srcbtnid>cap->srcbtnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert cap.
 */
 
static struct incfg_cap *incfg_capv_insert(struct umenu *umenu,int p,int srcbtnid,int usage,int lo,int hi,int value) {
  if ((p<0)||(p>umenu->capc)) return 0;
  if (umenu->capc>=umenu->capa) {
    int na=umenu->capa+32;
    if (na>INT_MAX/sizeof(struct incfg_cap)) return 0;
    void *nv=realloc(umenu->capv,sizeof(struct incfg_cap)*na);
    if (!nv) return 0;
    umenu->capv=nv;
    umenu->capa=na;
  }
  struct incfg_cap *cap=umenu->capv+p;
  memmove(cap+1,cap,sizeof(struct incfg_cap)*(umenu->capc-p));
  umenu->capc++;
  cap->srcbtnid=srcbtnid;
  cap->usage=usage;
  cap->lo=lo;
  cap->hi=hi;
  cap->value=value;
  cap->blackout=0;
  return cap;
}

/* After selecting device, read and store its capabilities.
 */
 
static void incfg_read_device_caps(struct umenu *umenu) {
  umenu->capc=0;
  int p=0;
  for (;;p++) {
    int usage=0,lo=0,hi=0,value=0,srcbtnid;
    if ((srcbtnid=inmgr_get_device_button(&usage,&lo,&hi,&value,umenu->incdevid,p))<1) break;
    int p=incfg_capv_search(umenu,srcbtnid);
    if (p>=0) continue; // Keep the first.
    p=-p-1;
    struct incfg_cap *cap=incfg_capv_insert(umenu,p,srcbtnid,usage,lo,hi,value);
    if (!cap) return;
  }
}

/* Finish.
 */
 
static void incfg_finish(struct umenu *umenu) {
  umenu->incfg=0;
  if (umenu->incfg_only) {
    umenu->defunct=1;
  }
  inmgr_save();
  incfg_quit(umenu);
}

/* Advance to the next button, or schedule wrap-up.
 */
 
static void incfg_advance(struct umenu *umenu) {
  struct incfg_button *button=0;
  for (;;) {
    umenu->buttonp++;
    if ((umenu->buttonp<0)||(umenu->buttonp>=umenu->buttonc)) { // ok got em all
      incfg_finish(umenu);
      return;
    }
    button=umenu->buttonv+umenu->buttonp;
    if (!button->enable) { // Also done if everything remaining is disabled. Disabled always come after enabled in the list.
      incfg_finish(umenu);
      return;
    }
    if (!button->done) break;
  }
  
  const char *name=0;
  int namec=eggrt_string_get(&name,1,button->strix);
  incfg_add_label(umenu,LABEL_POSITION_BELOW,name,namec,0xffffffff);
  umenu->incclock=TIMEOUT_BUTTON;
}

/* Mark done any button matching this mask.
 */
 
static void incfg_done_buttons(struct umenu *umenu,uint16_t mask) {
  struct incfg_button *button=umenu->buttonv;
  int i=umenu->buttonc;
  for (;i-->0;button++) {
    if (button->dstbtnid&mask) button->done=1;
  }
}

/* Raw event from inmgr.
 */
 
static void incfg_on_event(int devid,int srcbtnid,int srcvalue,int state,void *userdata) {
  struct umenu *umenu=userdata;
  //fprintf(stderr,"%s %d:0x%08x=%d [0x%04x]\n",__func__,devid,srcbtnid,srcvalue,state);
  
  /* If we're waiting for a device, the first nonzero event selects it.
   */
  if (!umenu->incdevid) {
    if (!srcvalue) return;
    int vid=0,pid=0,version=0;
    const char *name=inmgr_get_device_id(&vid,&pid,&version,devid);
    if (name&&name[0]) {
      incfg_add_label(umenu,LABEL_POSITION_ABOVE,name,-1,0xffff00ff);
    } else {
      char tmp[]={
        "0123456789abcdef"[(vid>>12)&15],
        "0123456789abcdef"[(vid>> 8)&15],
        "0123456789abcdef"[(vid>> 4)&15],
        "0123456789abcdef"[(vid    )&15],
        ':',
        "0123456789abcdef"[(pid>>12)&15],
        "0123456789abcdef"[(pid>> 8)&15],
        "0123456789abcdef"[(pid>> 4)&15],
        "0123456789abcdef"[(pid    )&15],
        ':',
        "0123456789abcdef"[(version>>12)&15],
        "0123456789abcdef"[(version>> 8)&15],
        "0123456789abcdef"[(version>> 4)&15],
        "0123456789abcdef"[(version    )&15],
      };
      incfg_add_label(umenu,LABEL_POSITION_ABOVE,tmp,sizeof(tmp),0xffff00ff);
    }
    umenu->incdevid=devid;
    incfg_read_device_caps(umenu);
    incfg_advance(umenu);
    return;
  }
  
  // Device is already selected. Ignore events from any other.
  if (devid!=umenu->incdevid) return;
  
  // If we don't have a button ready, get out. (This is an error)
  if ((umenu->buttonp<0)||(umenu->buttonp>=umenu->buttonc)) return;
  struct incfg_button *button=umenu->buttonv+umenu->buttonp;
  if (!button->enable) return;
  
  // Find the cap. Get out if none, or if the event is not significant.
  int capp=incfg_capv_search(umenu,srcbtnid);
  if (capp<0) return;
  struct incfg_cap *cap=umenu->capv+capp;
  int part=incfg_get_part(cap,srcvalue);
  if (cap->blackout) {
    if (part==BTNPART_OFF) cap->blackout=0;
    return;
  }
  
  switch (part) {
    case BTNPART_OFF: return;
    case BTNPART_ON: {
        inmgr_remap_button(umenu->incdevid,srcbtnid,button->dstbtnid,0,0);
      } break;
    case BTNPART_POS: switch (button->dstbtnid) {
        case EGG_BTN_LEFT: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_LEFT|EGG_BTN_RIGHT,"reverse",7);
            incfg_done_buttons(umenu,EGG_BTN_LEFT|EGG_BTN_RIGHT);
          } break;
        case EGG_BTN_RIGHT: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_LEFT|EGG_BTN_RIGHT,0,0);
            incfg_done_buttons(umenu,EGG_BTN_LEFT|EGG_BTN_RIGHT);
          } break;
        case EGG_BTN_UP: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_UP|EGG_BTN_DOWN,"reverse",7);
            incfg_done_buttons(umenu,EGG_BTN_UP|EGG_BTN_DOWN);
          } break;
        case EGG_BTN_DOWN: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_UP|EGG_BTN_DOWN,0,0);
            incfg_done_buttons(umenu,EGG_BTN_UP|EGG_BTN_DOWN);
          } break;
      } break;
    case BTNPART_NEG: switch (button->dstbtnid) {
        case EGG_BTN_LEFT: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_LEFT|EGG_BTN_RIGHT,0,0);
            incfg_done_buttons(umenu,EGG_BTN_LEFT|EGG_BTN_RIGHT);
          } break;
        case EGG_BTN_RIGHT: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_LEFT|EGG_BTN_RIGHT,"reverse",7);
            incfg_done_buttons(umenu,EGG_BTN_LEFT|EGG_BTN_RIGHT);
          } break;
        case EGG_BTN_UP: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_UP|EGG_BTN_DOWN,0,0);
            incfg_done_buttons(umenu,EGG_BTN_UP|EGG_BTN_DOWN);
          } break;
        case EGG_BTN_DOWN: {
            inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_UP|EGG_BTN_DOWN,"reverse",7);
            incfg_done_buttons(umenu,EGG_BTN_UP|EGG_BTN_DOWN);
          } break;
      } break;
    case BTNPART_N: case BTNPART_E: case BTNPART_S: case BTNPART_W: {
        if (!(button->dstbtnid&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN))) return;
        inmgr_remap_button(umenu->incdevid,srcbtnid,EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN,0,0);
        incfg_done_buttons(umenu,EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN);
      } break;
    default: return;
  }
  cap->blackout=1; // Must return to zero before we recognize it again. Important for analogue sticks.
  
  incfg_advance(umenu);
}

/* Initialize one button.
 */
 
static void incfg_button_init(struct umenu *umenu,struct incfg_button *button,int btnid,int strix) {
  button->dstbtnid=btnid;
  button->strix=strix;
  button->enable=1;
  // Hard-coded positions of each button in the source image.
  switch (btnid) {
    #define _(btntag,srcx,srcy,srcw,srch) case EGG_BTN_##btntag: { \
        button->x=umenu->incx+srcx*umenu->incscale; \
        button->y=umenu->incy+srcy*umenu->incscale; \
        button->w=srcw*umenu->incscale; \
        button->h=srch*umenu->incscale; \
      } break;
    _(LEFT,7,10,2,2)
    _(RIGHT,11,10,2,2)
    _(UP,9,8,2,2)
    _(DOWN,9,12,2,2)
    _(SOUTH,22,12,2,2)
    _(WEST,20,10,2,2)
    _(EAST,24,10,2,2)
    _(NORTH,22,8,2,2)
    _(AUX1,17,12,2,2)
    _(AUX2,14,12,2,2)
    _(AUX3,15,8,3,2)
    _(L1,7,4,2,2)
    _(R1,24,4,2,2)
    _(L2,10,5,2,1)
    _(R2,21,5,2,1)
    #undef _
  }
}

/* Populate buttonv.
 */
 
static void incfg_populate_buttonv(struct umenu *umenu) {
  
  // ROM may indicate which buttons are in use, what order to prompt for them, and what to call them.
  const char *plan=eggrt.metadata.incfgMask;
  int planc=eggrt.metadata.incfgMaskc;
  int strix=eggrt.metadata.incfgNames;
  if (planc<1) {
    plan="dswen123lrLR";
    planc=12;
  }
  
  uint16_t gotbits=0;
  memset(umenu->buttonv,0,sizeof(umenu->buttonv));
  struct incfg_button *button=umenu->buttonv;
  umenu->buttonc=0;
  umenu->buttonp=-1;
  umenu->incdevid=0;
  while ((planc>0)&&(umenu->buttonc<15)) {
    char opcode=*plan;
    plan++;
    planc--;
    int btnid=0;
    switch (opcode) {
      case 'd': { // 'd' is special; we add four buttons.
          if (umenu->buttonc>11) break;
          if (gotbits&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) break;
          incfg_button_init(umenu,button+0,EGG_BTN_LEFT,strix);
          incfg_button_init(umenu,button+1,EGG_BTN_RIGHT,strix);
          incfg_button_init(umenu,button+2,EGG_BTN_UP,strix);
          incfg_button_init(umenu,button+3,EGG_BTN_DOWN,strix);
          gotbits|=(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN);
          button+=4;
          umenu->buttonc+=4;
          if (strix>0) strix++;
        } break;
      // All the rest are single buttons.
      case 's': btnid=EGG_BTN_SOUTH; break;
      case 'w': btnid=EGG_BTN_WEST; break;
      case 'e': btnid=EGG_BTN_EAST; break;
      case 'n': btnid=EGG_BTN_NORTH; break;
      case '1': btnid=EGG_BTN_AUX1; break;
      case '2': btnid=EGG_BTN_AUX2; break;
      case '3': btnid=EGG_BTN_AUX3; break;
      case 'l': btnid=EGG_BTN_L1; break;
      case 'r': btnid=EGG_BTN_R1; break;
      case 'L': btnid=EGG_BTN_L2; break;
      case 'R': btnid=EGG_BTN_R2; break;
    }
    if (!btnid) continue;
    if (gotbits&btnid) continue;
    incfg_button_init(umenu,button,btnid,strix);
    gotbits|=btnid;
    button++;
    umenu->buttonc++;
    if (strix>0) strix++;
  }
  
  // Any button we haven't visited yet gets initialized but disabled.
  uint16_t btnid=1;
  for (;btnid<0x8000;btnid<<=1) {
    if (gotbits&btnid) continue;
    if (umenu->buttonc>=15) break;
    incfg_button_init(umenu,button,btnid,0);
    button->enable=0;
    button++;
    umenu->buttonc++;
  }
}

/* Begin input config.
 */
 
void incfg_begin(struct umenu *umenu) {
  if (umenu->incfg) return;
  umenu->incfg=1;
  umenu->inlistener=inmgr_listen(incfg_on_event,umenu);
  incfg_enable_all(umenu,0); // A bit heavy-handed. If a device was deliberately disabled before, we'll accidentally reenable it on quit.
  umenu->incclock=TIMEOUT_DEVICE;
  
  while (umenu->labelc>0) {
    umenu->labelc--;
    egg_texture_del(umenu->labelv[umenu->labelc].texid);
  }
  
  /* Select scale for the main display.
   * Natural size is 32x16, and we'll pretty much always print it bigger.
   * Not more than half of either framebuffer axis.
   */
  int xscale=(umenu->fbw>>1)/32;
  int yscale=(umenu->fbh>>1)/16;
  umenu->incscale=(xscale<yscale)?xscale:yscale;
  if (umenu->incscale<1) umenu->incscale=1;
  umenu->incw=32*umenu->incscale;
  umenu->inch=16*umenu->incscale;
  umenu->incx=(umenu->fbw>>1)-(umenu->incw>>1);
  umenu->incy=(umenu->fbh>>1)-(umenu->inch>>1);
  
  incfg_populate_buttonv(umenu);
}

/* Update.
 */
 
void incfg_update(struct umenu *umenu,double elapsed) {
  if ((umenu->incblink-=elapsed)<0.0) {
    umenu->incblink+=BLINK_PERIOD;
  }
  if ((umenu->incclock-=elapsed)<0.0) {
    if (umenu->incdevid) {
      incfg_advance(umenu);
    } else {
      incfg_finish(umenu);
      return;
    }
  }
}

/* Render.
 */
 
void incfg_render(struct umenu *umenu) {

  // Draw the gamepad decal.
  {
    const int srcx=128,srcy=176;
    struct egg_render_raw vtxv[]={
      {umenu->incx,umenu->incy,srcx,srcy},
      {umenu->incx,umenu->incy+umenu->inch,srcx,srcy+16},
      {umenu->incx+umenu->incw,umenu->incy,srcx+32,srcy},
      {umenu->incx+umenu->incw,umenu->incy+umenu->inch,srcx+32,srcy+16},
    };
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=1,
      .srctexid=umenu->texid_tiles,
      .alpha=0xff,
      .tint=0x808080ff,
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }

  // Draw each button.
  {
    struct incfg_button *button=umenu->buttonv;
    int i=0;
    for (;i<umenu->buttonc;i++,button++) {
      if (!button->enable) {
        umenu_fill_rect(button->x,button->y,button->w,button->h,0x707070ff);
      } else if (i==umenu->buttonp) { // In progress.
        umenu_fill_rect(button->x,button->y,button->w,button->h,(umenu->incblink<BLINK_PERIOD*0.5)?0xffff00ff:0x800000ff);
      } else if (i>umenu->buttonp) { // Pending.
        umenu_fill_rect(button->x,button->y,button->w,button->h,0xc0c0c0ff);
      } else { // Complete.
        umenu_fill_rect(button->x,button->y,button->w,button->h,0x008000ff);
      }
    }
  }
  
  // Labels.
  {
    struct incfg_label *label=umenu->labelv;
    int i=umenu->labelc;
    for (;i-->0;label++) {
      struct egg_render_raw vtxv[]={
        {label->x,label->y,0,0},
        {label->x,label->y+label->h,0,label->h},
        {label->x+label->w,label->y,label->w,0},
        {label->x+label->w,label->y+label->h,label->w,label->h},
      };
      struct egg_render_uniform un={
        .mode=EGG_RENDER_TRIANGLE_STRIP,
        .dsttexid=1,
        .srctexid=label->texid,
        .alpha=0xff,
        .tint=label->tint,
      };
      egg_render(&un,vtxv,sizeof(vtxv));
    }
  }
  
  // If our clock is under 5, show it (ceiling; single digit).
  if (umenu->incclock<5.0) {
    const int glyphw=3;
    const int glyphh=5;
    int digit=(int)(umenu->incclock+1.0);
    if (digit<1) digit=1; else if (digit>5) digit=5;
    int dstx=(umenu->fbw>>1)-1;
    int dsty=umenu->fbh-15;
    int srcx=192+digit*glyphw;
    int srcy=149;
    struct egg_render_raw vtxv[]={
      {dstx,dsty,srcx,srcy},
      {dstx,dsty+glyphh,srcx,srcy+glyphh},
      {dstx+glyphw,dsty,srcx+glyphw,srcy},
      {dstx+glyphw,dsty+glyphh,srcx+glyphw,srcy+glyphh},
    };
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=1,
      .srctexid=umenu->texid_tiles,
      .alpha=0xff,
      .tint=0xff0000ff,
    };
    render_render(eggrt.render,&un,vtxv,sizeof(vtxv));
  }
}
