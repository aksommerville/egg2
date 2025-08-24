#include "eggrt/eggrt_internal.h"
#include "opt/serial/serial.h"

/* Delete.
 */
 
void inmgr_device_del(struct inmgr_device *device) {
  if (!device) return;
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

/* Remove any buttons that aren't mapped.
 */
 
static void inmgr_device_remove_unmapped_buttons(struct inmgr_device *device) {
  int i=device->buttonc;
  while (i>0) {
    int rmc=0;
    while (i&&!device->buttonv[i-1].dstbtnid) { i--; rmc++; }
    if (rmc) {
      device->buttonc-=rmc;
      memmove(device->buttonv+i,device->buttonv+i+rmc,sizeof(struct inmgr_button)*(device->buttonc-i));
    }
    i--;
  }
}

/* Map buttons for the default keyboard.
 * These must be sorted by HID Usage, which breaks up the groups.
 * In a more sensible order:
 *
 *   w,a,s,d => dpad
 *   e,q,r,f => south,west,east,north
 *   space,comma,dot,slash => thumbs
 *
 *   arrows => dpad
 *   z,x,c,v => thumbs
 *   grave,backspace => triggers
 *   enter => aux
 *
 *   kp 8,4,5,6,2 => dpad
 *   kp 7,9 => triggers
 *   kp 0,enter,plus,dot => thumbs
 *   kp dash => aux
 *
 *   esc => quit
 *   f11 => fullscreen
 *
 * Keep this the same as src/web/js/Input.js.
 */
 
static const struct inmgr_keymap { int hidusage,dstbtnid; } inmgr_keymapv[]={
  {0x00070004,EGG_BTN_LEFT}, // a
  {0x00070006,EGG_BTN_EAST}, // c
  {0x00070007,EGG_BTN_RIGHT}, // d
  {0x00070008,EGG_BTN_SOUTH}, // e
  {0x00070009,EGG_BTN_NORTH}, // f
  {0x00070014,EGG_BTN_WEST}, // q
  {0x00070015,EGG_BTN_EAST}, // r
  {0x00070016,EGG_BTN_DOWN}, // s
  {0x00070019,EGG_BTN_EAST}, // v
  {0x0007001a,EGG_BTN_UP}, // w
  {0x0007001b,EGG_BTN_WEST}, // x
  {0x0007001d,EGG_BTN_SOUTH}, // z
  {0x00070028,EGG_BTN_AUX1}, // enter
  {0x00070029,INMGR_ACTION_QUIT}, // escape
  {0x0007002a,EGG_BTN_R1}, // backspace
  {0x0007002c,EGG_BTN_SOUTH}, // space
  {0x00070035,EGG_BTN_L1}, // grave
  {0x00070036,EGG_BTN_WEST}, // comma
  {0x00070037,EGG_BTN_EAST}, // dot
  {0x00070038,EGG_BTN_NORTH}, // slash
  {0x00070044,INMGR_ACTION_FULLSCREEN}, // f11
  {0x0007004f,EGG_BTN_RIGHT}, // right
  {0x00070050,EGG_BTN_LEFT}, // left
  {0x00070051,EGG_BTN_DOWN}, // down
  {0x00070052,EGG_BTN_UP}, // up
  {0x00070056,EGG_BTN_AUX1}, // kp dash
  {0x00070057,EGG_BTN_EAST}, // kp plus
  {0x00070058,EGG_BTN_WEST}, // kp enter
  {0x0007005a,EGG_BTN_DOWN}, // kp 2
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
  inmgr_device_remove_unmapped_buttons(device);
  return 0;
}

/* XXX TEMP Hard-coded device mappings.
 */
 
static int XXX_inmgr_device_map_xinmotek(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->btnid) {
      case 0x00010121: button->dstbtnid=EGG_BTN_SOUTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010122: button->dstbtnid=EGG_BTN_WEST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010123: button->dstbtnid=EGG_BTN_EAST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010124: button->dstbtnid=EGG_BTN_NORTH; button->srclo=1; button->srchi=INT_MAX; break;
      //case 0x00010125: button->dstbtnid=EGG_BTN_AUX2; button->srclo=1; button->srchi=INT_MAX; break; // We don't have an AUX2.
      case 0x00010126: button->dstbtnid=EGG_BTN_AUX1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001012a: button->dstbtnid=INMGR_ACTION_QUIT; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00030000: button->dstbtnid=EGG_BTN_HORZ; button->srclo=127; button->srchi=129; button->srcvalue=128; break;
      case 0x00030001: button->dstbtnid=EGG_BTN_VERT; button->srclo=127; button->srchi=129; button->srcvalue=128; break;
    }
  }
  return 0;
}

static int XXX_inmgr_device_map_elcheapo(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->btnid) {
      case 0x00030010: button->dstbtnid=EGG_BTN_HORZ; button->srclo=-1; button->srchi=1; break;
      case 0x00030011: button->dstbtnid=EGG_BTN_VERT; button->srclo=-1; button->srchi=1; break;
      case 0x00010122: button->dstbtnid=EGG_BTN_SOUTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010123: button->dstbtnid=EGG_BTN_WEST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010121: button->dstbtnid=EGG_BTN_EAST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010120: button->dstbtnid=EGG_BTN_NORTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010124: button->dstbtnid=EGG_BTN_L1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010125: button->dstbtnid=EGG_BTN_R1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010129: button->dstbtnid=EGG_BTN_AUX1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001012b: button->dstbtnid=INMGR_ACTION_QUIT; button->srclo=1; button->srchi=INT_MAX; break;
    }
  }
  return 0;
}

static int XXX_inmgr_device_map_zelda(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->btnid) {
      case 0x00030010: button->dstbtnid=EGG_BTN_HORZ; button->srclo=-1; button->srchi=1; break;
      case 0x00030011: button->dstbtnid=EGG_BTN_VERT; button->srclo=-1; button->srchi=1; break;
      case 0x00010131: button->dstbtnid=EGG_BTN_SOUTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010130: button->dstbtnid=EGG_BTN_WEST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010132: button->dstbtnid=EGG_BTN_EAST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010133: button->dstbtnid=EGG_BTN_NORTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010134: button->dstbtnid=EGG_BTN_L1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010135: button->dstbtnid=EGG_BTN_R1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010139: button->dstbtnid=EGG_BTN_AUX1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001013b: button->dstbtnid=INMGR_ACTION_QUIT; button->srclo=1; button->srchi=INT_MAX; break;
    }
  }
  return 0;
}

static int XXX_inmgr_device_map_snes8bd(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->btnid) {
      case 0x00030000: button->dstbtnid=EGG_BTN_HORZ; button->srclo=-100; button->srchi=100; break; // Range 32kish, sometimes nonzero resting value.
      case 0x00030001: button->dstbtnid=EGG_BTN_VERT; button->srclo=-100; button->srchi=100; break; // ....why on earth does it do that???
      case 0x00010130: button->dstbtnid=EGG_BTN_SOUTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010133: button->dstbtnid=EGG_BTN_WEST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010131: button->dstbtnid=EGG_BTN_EAST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010134: button->dstbtnid=EGG_BTN_NORTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010136: button->dstbtnid=EGG_BTN_L1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010137: button->dstbtnid=EGG_BTN_R1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001013b: button->dstbtnid=EGG_BTN_AUX1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001013a: button->dstbtnid=INMGR_ACTION_QUIT; button->srclo=1; button->srchi=INT_MAX; break; // Select (there is no AUX3 or plunger)
    }
  }
  return 0;
}

static int XXX_inmgr_device_map_evercade(struct inmgr *inmgr,struct inmgr_device *device) {
  struct inmgr_button *button=device->buttonv;
  int i=device->buttonc;
  for (;i-->0;button++) {
    switch (button->btnid) {
      case 0x00030010: button->dstbtnid=EGG_BTN_HORZ; button->srclo=-1; button->srchi=1; break;
      case 0x00030011: button->dstbtnid=EGG_BTN_VERT; button->srclo=-1; button->srchi=1; break;
      case 0x00010130: button->dstbtnid=EGG_BTN_SOUTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010133: button->dstbtnid=EGG_BTN_WEST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010131: button->dstbtnid=EGG_BTN_EAST; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010134: button->dstbtnid=EGG_BTN_NORTH; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010136: button->dstbtnid=EGG_BTN_L1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x00010137: button->dstbtnid=EGG_BTN_R1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001013b: button->dstbtnid=EGG_BTN_AUX1; button->srclo=1; button->srchi=INT_MAX; break;
      case 0x0001013c: button->dstbtnid=INMGR_ACTION_QUIT; button->srclo=1; button->srchi=INT_MAX; break;
    }
  }
  return 0;
}

/* Map device buttons, after acquiring the full set.
 */
 
static int inmgr_device_map(
  struct inmgr *inmgr,
  struct inmgr_device *device,
  int vid,int pid,int version,const char *name
) {
  //fprintf(stderr,"%s %04x:%04x:%04x '%s' buttonc=%d...\n",__func__,vid,pid,version,name,device->buttonc);
  
  /* XXX Highly temporary. Hard-coded mapping for my devices.
   * Hacking this in quick before GDEX 2025. Get it working proper later.
   */
  switch ((vid<<16)|pid) {
    case 0x16c005e1: return XXX_inmgr_device_map_xinmotek(inmgr,device);
    case 0x0e8f0003: return XXX_inmgr_device_map_elcheapo(inmgr,device);
    case 0x20d6a711: return XXX_inmgr_device_map_zelda(inmgr,device);
    case 0x045e028e: switch (version) { // Everybody calls themselves fucking "Xbox 360 Controller", it drives me nuts.
        case 0x0114: return XXX_inmgr_device_map_snes8bd(inmgr,device);
        case 0x0105: return XXX_inmgr_device_map_evercade(inmgr,device);
      } break;
  }
  
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
            4,5,6,7, // SOUTH,WEST,EAST,NORTH
            10, // AUX1
            8,9, // L1,R1
          };
          int candidatec=7;
          if (!hatc&&(axisc<2)) {
            // If we don't have any axes or hats, the first four buttons map to a dpad.
            memmove(candidatev+4,candidatev,sizeof(int)*candidatec);
            candidatec+=4;
            candidatev[0]=0; // LEFT
            candidatev[1]=1; // RIGHT
            candidatev[2]=2; // UP
            candidatev[3]=3; // DOWN
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
                count_by_btnix[0]++; // LEFT
                count_by_btnix[1]++; // RIGHT
              } break;
            case 0x00010031:
            case 0x00010034:
            case 0x00050022:
            case 0x00050025:
            case 0x00050026:
            case 0x00050028: {
                button->dstbtnid=EGG_BTN_VERT;
                count_by_btnix[2]++; // UP
                count_by_btnix[3]++; // DOWN
              } break;
            default: {
                if (count_by_btnix[0]<=count_by_btnix[2]) {
                  button->dstbtnid=EGG_BTN_HORZ;
                  count_by_btnix[0]++;
                  count_by_btnix[1]++;
                } else {
                  button->dstbtnid=EGG_BTN_VERT;
                  count_by_btnix[2]++;
                  count_by_btnix[3]++;
                }
              }
          }
        } break;

      case CLS_HAT: {
          button->dstbtnid=EGG_BTN_DPAD;
          count_by_btnix[0]++; // LEFT
          count_by_btnix[1]++; // RIGHT
          count_by_btnix[2]++; // UP
          count_by_btnix[3]++; // DOWN
        } break;
    }
    button->srcvalue=button->rest;
    if (button->dstbtnid<0x10000) device->dstmask|=button->dstbtnid;
  }
  
  #undef CLS_KEY
  #undef CLS_BTN
  #undef CLS_AXIS
  #undef CLS_HAT
  inmgr_device_remove_unmapped_buttons(device);
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
  return device->driver->type->for_each_button(device->driver,device->devid,inmgr_device_acquire_buttons_cb,device);
}

/* Get config from hostio and apply maps.
 */

int inmgr_device_query_config(struct inmgr *inmgr,struct inmgr_device *device) {
  if (!device->driver) return -1;
  
  int vid=0,pid=0,version=0;
  const char *name=0;
  if (device->driver->type->get_ids) {
    name=device->driver->type->get_ids(&vid,&pid,&version,device->driver,device->devid);
  }
  
  if (device->driver->type->for_each_button) {
    if (inmgr_device_acquire_buttons(inmgr,device)<0) return -1;
  }
  
  return inmgr_device_map(inmgr,device,vid,pid,version,name);
}

/* Synthesize config for keyboard and apply maps.
 */

static const struct inmgr_keyrange { uint8_t lo,hi; } inmgr_keyrangev[]={
  {0x04,0x67}, // All the common keys, plus a few oddballs.
  // 0x68..0xdd most are defined by the spec, but they are all unusual.
  {0xe0,0xe7}, // Modifiers.
};
 
int inmgr_device_init_keyboard(struct inmgr *inmgr,struct inmgr_device *device) {
  
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
  
  return inmgr_device_map(inmgr,device,0,0,0,"System Keyboard");
}

/* Map to player if we haven't.
 */
 
void inmgr_device_require_player(struct inmgr *inmgr,struct inmgr_device *device) {
  if (device->playerid) return;
  device->playerid=1;//TODO device to player mapping
}
