#include "eggrt/eggrt_internal.h"
#include "opt/serial/serial.h"

/* Delete.
 */
 
void inmgr_device_del(struct inmgr_device *device) {
  if (!device) return;
  if (device->name) free(device->name);
  if (device->buttonv) free(device->buttonv);
  free(device);
}

/* New.
 */
 
struct inmgr_device *inmgr_device_new() {
  struct inmgr_device *device=calloc(1,sizeof(struct inmgr_device));
  if (!device) return 0;
  return device;
}

/* Sanitize and store name.
 */
 
static int inmgr_device_set_name(struct inmgr_device *device,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  // Sanitize agressively: Must be UTF-8, trim leading and trailing space, and condense inner space.
  // If the length goes beyond 64, cut it off hard.
  char tmp[64];
  int tmpc=0,srcp=0,space=1;
  while (srcp<srcc) {
    int seqlen,codepoint;
    if ((seqlen=sr_utf8_decode(&codepoint,src+srcp,srcc-srcp))<1) {
      srcp++;
      continue;
    }
    srcp+=seqlen;
    if (codepoint<=0x20) {
      if (!space) {
        if (tmpc>=sizeof(tmp)) break;
        tmp[tmpc++]=0x20;
        space=1;
      }
      continue;
    }
    space=0;
    int addc=sr_utf8_encode(tmp+tmpc,sizeof(tmp)-tmpc,codepoint);
    if (tmpc>(int)sizeof(tmp)-addc) break;
    tmpc+=addc;
  }
  if (space&&tmpc) tmpc--;
  char *nv=0;
  if (tmpc) {
    if (!(nv=malloc(tmpc+1))) return -1;
    memcpy(nv,tmp,tmpc);
    nv[tmpc]=0;
  }
  if (device->name) free(device->name);
  device->name=nv;
  device->namec=tmpc;
  return 0;
}

/* Search buttons.
 */
 
int inmgr_device_buttonv_search(struct inmgr_device *device,int btnid) {
  // During setup, it's normal to receive a whole bunch of buttons with ids in order. Optimize against that, otherwise it's worst-case.
  if ((device->buttonc<1)||(btnid>device->buttonv[device->buttonc-1].btnid)) return -device->buttonc-1;
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct inmgr_button *button=device->buttonv+ck;
         if (btnid<button->btnid) hi=ck;
    else if (btnid>button->btnid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct inmgr_button *inmgr_device_buttonv_get(struct inmgr_device *device,int btnid) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct inmgr_button *button=device->buttonv+ck;
         if (btnid<button->btnid) hi=ck;
    else if (btnid>button->btnid) lo=ck+1;
    else return button;
  }
  return 0;
}

/* Map buttons for the default keyboard.
 * These must be sorted by HID Usage, which breaks up the groups.
 * In a more sensible order:
 *
 *   w,a,s,d => dpad
 *   e,q,r => south,west,east
 *   space,comma,dot,slash => thumbs
 *
 *   arrows => dpad
 *   z,x,c,v => thumbs
 *   tab,backslash,grave,backspace => triggers
 *   enter,rightbracket,leftbracket => aux
 *
 *   kp 8,4,5,6,2 => dpad
 *   kp 7,9,1,3 => triggers
 *   kp 0,enter,plus,dot => thumbs
 *   kp slash,star,dash => aux
 *
 * Keep this the same as src/web/js/Input.js.
 */
 
static const struct inmgr_keymap { int hidusage,dstbtnid; } inmgr_keymapv[]={
  {0x00070004,EGG_BTN_LEFT}, // a
  {0x00070006,EGG_BTN_EAST}, // c
  {0x00070007,EGG_BTN_RIGHT}, // d
  {0x00070008,EGG_BTN_SOUTH}, // e
  {0x00070014,EGG_BTN_WEST}, // q
  {0x00070015,EGG_BTN_EAST}, // r
  {0x00070016,EGG_BTN_DOWN}, // s
  {0x00070019,EGG_BTN_EAST}, // v
  {0x0007001a,EGG_BTN_UP}, // w
  {0x0007001b,EGG_BTN_WEST}, // x
  {0x0007001d,EGG_BTN_SOUTH}, // z
  {0x00070028,EGG_BTN_AUX1}, // enter
  {0x00070029,INMGR_ACTION_QUIT}, // escape
  {0x0007002a,EGG_BTN_R2}, // backspace
  {0x0007002b,EGG_BTN_L1}, // tab
  {0x0007002c,EGG_BTN_SOUTH}, // space
  {0x0007002f,EGG_BTN_AUX3}, // left bracket
  {0x00070030,EGG_BTN_AUX2}, // right bracket
  {0x00070031,EGG_BTN_R1}, // backslash
  {0x00070035,EGG_BTN_L2}, // grave
  {0x00070036,EGG_BTN_WEST}, // comma
  {0x00070037,EGG_BTN_EAST}, // dot
  {0x00070038,EGG_BTN_NORTH}, // slash
  {0x00070044,INMGR_ACTION_FULLSCREEN}, // f11
  {0x0007004f,EGG_BTN_RIGHT}, // right
  {0x00070050,EGG_BTN_LEFT}, // left
  {0x00070051,EGG_BTN_DOWN}, // down
  {0x00070052,EGG_BTN_UP}, // up
  {0x00070054,EGG_BTN_AUX1}, // kp slash
  {0x00070055,EGG_BTN_AUX2}, // kp star
  {0x00070056,EGG_BTN_AUX3}, // kp dash
  {0x00070057,EGG_BTN_EAST}, // kp plus
  {0x00070058,EGG_BTN_WEST}, // kp enter
  {0x00070059,EGG_BTN_L2}, // kp 1
  {0x0007005a,EGG_BTN_DOWN}, // kp 2
  {0x0007005b,EGG_BTN_R2}, // kp 3
  {0x0007005c,EGG_BTN_LEFT}, // kp 4
  {0x0007005d,EGG_BTN_DOWN}, // kp 5
  {0x0007005e,EGG_BTN_RIGHT}, // kp 6
  {0x0007005f,EGG_BTN_L1}, // kp 7
  {0x00070060,EGG_BTN_UP}, // kp 8
  {0x00070061,EGG_BTN_R1}, // kp 9
  {0x00070062,EGG_BTN_SOUTH}, // kp 0
  {0x00070063,EGG_BTN_NORTH}, // kp dot
};
 
static int inmgr_device_map_default_keyboard(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    button->srcvalue=button->lo;
    const struct inmgr_keymap *keymap=0;
    int lo=0,hi=sizeof(inmgr_keymapv)/sizeof(struct inmgr_keymap);
    while (lo<hi) {
      int ck=(lo+hi)>>1;
      const struct inmgr_keymap *q=inmgr_keymapv+ck;
           if (button->hidusage<q->hidusage) hi=ck;
      else if (button->hidusage>q->hidusage) lo=ck+1;
      else { keymap=q; break; }
    }
    if (!keymap) continue;
    button->dstbtnid=keymap->dstbtnid;
  }
  return 0;
}

/* Map device buttons, after acquiring the full set.
 */
 
static int inmgr_device_map(struct inmgr *inmgr,struct inmgr_device *device) {
  fprintf(stderr,"%s %04x:%04x:%04x '%.*s' buttonc=%d...\n",__func__,device->vid,device->pid,device->version,device->namec,device->name,device->buttonc);
  
  //TODO Search templates. Apply if we find one. Otherwise synthesize a new template, save it, then apply it.
  // The logic below should be adapted for that synthesize case.
  
  /* Classify each button and select limits.
   * Use (srcvalue) temporarily to record the decision.
   */
  #define CLS_KEY 1 /* KEY are BTN, but we'll take a different special path for keyboards. */
  #define CLS_BTN 2
  #define CLS_AXIS 3
  #define CLS_HAT 4
  int keyc=0,btnc=0,axisc=0,hatc=0;
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    if ((button->hidusage&0xffff0000)==0x00070000) { // Page 7: Keyboard.
      button->srcvalue=CLS_KEY;
      keyc++;
      button->srclo=button->lo+1;
      button->srchi=INT_MAX;
    } else if ((button->rest>button->lo)&&(button->rest<button->hi)) { // Intermediate resting value: Axis.
      button->srcvalue=CLS_AXIS;
      axisc++;
      int mid=(button->lo+button->hi)>>1;
      int midlo=(button->lo+mid)>>1;
      int midhi=(button->hi+mid)>>1;
      if (midlo>=mid) midlo=mid-1;
      if (midhi<=mid) midhi=mid+1;
      button->srclo=midlo;
      button->srchi=midhi;
    } else if (button->hi-button->lo==7) { // Declared range of exactly 8 inclusive: Hat.
      button->srcvalue=CLS_HAT;
      hatc++;
      button->srclo=button->lo;
      button->srchi=button->hi;
    } else { // Two-state button or one-way axis.
      button->srcvalue=CLS_BTN;
      btnc++;
      button->srclo=button->lo+1;
      button->srchi=INT_MAX;
    }
  }
  
  /* If it has at least 60 KEY, assume it's a standard keyboard and use a fixed default mapping.
   * This will not assign every button, and that's fine.
   */
  if (keyc>=60) {
    return inmgr_device_map_default_keyboard(inmgr,device);
  }
  
  /* Don't map if there's fewer than 3 buttons. (SOUTH,WEST,AUX1)
   * And if we don't have 2 axes or 1 hat, make that 7 buttons: Those devices need 4 buttons for the dpad.
   */
  if (hatc||(axisc>=2)) {
    if (keyc+btnc<3) return 0;
  } else {
    if (keyc+btnc<7) return 0;
  }
  
  /* Map every button.
   * Track how many we've assigned to each of the 16 standard buttons, and assign to those evenly if (hidusage) doesn't tell us.
   */
  int count_by_btnix[16]={0};
  for (button=device->buttonv,i=device->buttonc;i-->0;button++) {
    switch (button->srcvalue) {
    
      case CLS_KEY: case CLS_BTN: {
          int candidatev[16]={
            0,1,2,3, // SOUTH,WEST,EAST,NORTH
            9,8, // AUX1,AUX2
            4,5,6,7, // L1,R1,L2,R2
            10, // AUX3
          };
          int candidatec=11;
          if (!hatc&&(axisc<2)) {
            // If we don't have any axes or hats, the first four buttons map to a dpad.
            memmove(candidatev+4,candidatev,sizeof(int)*candidatec);
            candidatec+=4;
            candidatev[0]=12; // UP
            candidatev[1]=13; // DOWN
            candidatev[2]=14; // LEFT
            candidatev[3]=15; // RIGHT
          }
          int bestscore=999,bestix=0;
          int ii=0; for (;ii<candidatec;ii++) {
            int btnix=candidatev[ii];
            if (count_by_btnix[btnix]>=bestscore) continue;
            bestix=btnix;
            bestscore=count_by_btnix[btnix];
            button->dstbtnid=1<<btnix;
          }
          count_by_btnix[bestix]++;
        } break;
      
      case CLS_AXIS: {
          switch (button->hidusage) {
            case 0x00010030:
            case 0x00010033:
            case 0x00050021:
            case 0x00050023:
            case 0x00050024:
            case 0x00050027: {
                button->dstbtnid=EGG_BTN_HORZ;
                count_by_btnix[14]++; // LEFT
                count_by_btnix[15]++; // RIGHT
              } break;
            case 0x00010031:
            case 0x00010034:
            case 0x00050022:
            case 0x00050025:
            case 0x00050026:
            case 0x00050028: {
                button->dstbtnid=EGG_BTN_VERT;
                count_by_btnix[12]++; // UP
                count_by_btnix[13]++; // DOWN
              } break;
            default: {
                if (count_by_btnix[14]<=count_by_btnix[12]) {
                  button->dstbtnid=EGG_BTN_HORZ;
                  count_by_btnix[14]++;
                  count_by_btnix[15]++;
                } else {
                  button->dstbtnid=EGG_BTN_VERT;
                  count_by_btnix[12]++;
                  count_by_btnix[13]++;
                }
              }
          }
        } break;

      case CLS_HAT: {
          button->dstbtnid=EGG_BTN_DPAD;
          count_by_btnix[12]++; // UP
          count_by_btnix[13]++; // DOWN
          count_by_btnix[14]++; // LEFT
          count_by_btnix[15]++; // RIGHT
        } break;
    }
    button->srcvalue=button->rest;
    if (button->dstbtnid<0x10000) device->dstmask|=button->dstbtnid;
  }
  
  #undef CLS_KEY
  #undef CLS_BTN
  #undef CLS_AXIS
  #undef CLS_HAT
  return 0;
}

/* Insert button. Valid insertion point required.
 */
 
static struct inmgr_button *inmgr_device_buttonv_insert(struct inmgr_device *device,int p,int btnid) {
  if ((p<0)||(p>device->buttonc)) return 0;
  if (p&&(btnid<=device->buttonv[p-1].btnid)) return 0;
  if ((p<device->buttonc)&&(btnid>=device->buttonv[p].btnid)) return 0;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+32;
    if (na>INT_MAX/sizeof(struct inmgr_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct inmgr_button)*na);
    if (!nv) return 0;
    device->buttonv=nv;
    device->buttona=na;
  }
  struct inmgr_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct inmgr_button)*(device->buttonc-p));
  device->buttonc++;
  memset(button,0,sizeof(struct inmgr_button));
  button->btnid=btnid;
  return button;
}

/* Acquire buttons for device with a driver that supports that.
 */
 
static int inmgr_device_acquire_buttons_cb(int btnid,int hidusage,int lo,int hi,int rest,void *userdata) {
  struct inmgr_device *device=userdata;
  //fprintf(stderr,"  0x%08x 0x%08x %d..%d =%d\n",btnid,hidusage,lo,hi,rest);
  if (lo>=hi) return 0;
  int p=inmgr_device_buttonv_search(device,btnid);
  if (p>=0) return 0; // Already have it? First report wins.
  p=-p-1;
  struct inmgr_button *button=inmgr_device_buttonv_insert(device,p,btnid);
  if (!button) return -1;
  button->hidusage=hidusage;
  button->lo=lo;
  button->hi=hi;
  button->rest=rest;
  return 0;
}
 
static int inmgr_device_acquire_buttons(struct inmgr *inmgr,struct inmgr_device *device) {
  //fprintf(stderr,"%s %04x:%04x:%04x '%.*s'...\n",__func__,device->vid,device->pid,device->version,device->namec,device->name);
  return device->driver->type->for_each_button(device->driver,device->devid,inmgr_device_acquire_buttons_cb,device);
}

/* Get config from hostio and apply maps.
 */

int inmgr_device_query_config(struct inmgr *inmgr,struct inmgr_device *device) {
  if (!device->driver) return -1;
  
  if (device->driver->type->get_ids) {
    const char *name=device->driver->type->get_ids(&device->vid,&device->pid,&device->version,device->driver,device->devid);
    if (inmgr_device_set_name(device,name,-1)<0) return -1;
  }
  
  if (device->driver->type->for_each_button) {
    if (inmgr_device_acquire_buttons(inmgr,device)<0) return -1;
  }
  
  return inmgr_device_map(inmgr,device);
}

/* Synthesize config for keyboard and apply maps.
 */

static const struct inmgr_keyrange { uint8_t lo,hi; } inmgr_keyrangev[]={
  {0x04,0x67}, // All the common keys, plus a few oddballs.
  // 0x68..0xdd most are defined by the spec, but they are all unusual.
  {0xe0,0xe7}, // Modifiers.
};
 
int inmgr_device_init_keyboard(struct inmgr *inmgr,struct inmgr_device *device) {
  
  // Hard-coded IDs.
  device->vid=device->pid=device->version=0;
  if (inmgr_device_set_name(device,"System keyboard",15)<0) return -1;
  
  // Add button records for the entire set of reasonably-expected keys.
  const struct inmgr_keyrange *keyrange=inmgr_keyrangev;
  int i=sizeof(inmgr_keyrangev)/sizeof(struct inmgr_keyrange);
  for (;i-->0;keyrange++) {
    int usage=keyrange->lo;
    for (;usage<=keyrange->hi;usage++) {
      int btnid=0x00070000|usage;
      struct inmgr_button *button=inmgr_device_buttonv_insert(device,device->buttonc,btnid);
      if (!button) return -1;
      button->hidusage=btnid;
      button->hi=2; // Allow that drivers may use '2' for repeat events.
    }
  }
  
  return inmgr_device_map(inmgr,device);
}

/* Map to player if we haven't.
 */
 
void inmgr_device_require_player(struct inmgr *inmgr,struct inmgr_device *device) {
  if (device->playerid) return;
  device->playerid=1;//TODO device to player mapping
}
