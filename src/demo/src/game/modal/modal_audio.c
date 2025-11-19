#include "../demo.h"
#include "../gui/gui.h"

#define PLAYHEAD_ALERT_TIME 0.500
#define HOLD_LIMIT 2 /* How many note buttons do we have. */

struct modal_audio {
  struct modal hdr;
  struct gui_term *term;
  int focusp; // term row 1..5
  double pvplayhead; // For tracking unexpected playhead motion.
  double playhead_alert; // Counts down while alerting.
  
  int songidp;
  int *songidv;
  int songidc;
  
  int soundidp;
  int *soundidv;
  int soundidc;
  
  int force,repeat;
  double playhead; // The UI field value, in seconds. No upper bound.
  double trim; // 0..1
  double pan; // -1..1
  
  int dispsongid; // The one we think the platform is playing.
  int holdv[HOLD_LIMIT]; // holdid for notes playing.
  int wheeltarget;
  int wheelcurrent;
};

#define MODAL ((struct modal_audio*)modal)

/* Delete.
 */
 
static void _audio_del(struct modal *modal) {
  gui_term_del(MODAL->term);
  if (MODAL->songidv) free(MODAL->songidv);
  if (MODAL->soundidv) free(MODAL->soundidv);
}

/* Move focus.
 */
 
static void modal_audio_move(struct modal *modal,int d) {
  MODAL->focusp+=d;
  if (MODAL->focusp<1) MODAL->focusp=7;
  else if (MODAL->focusp>7) MODAL->focusp=1;
  egg_play_sound(RID_sound_uimotion,0.5,0.0);
}

/* Adjust focussed row.
 */
 
static void modal_audio_adjust_songid(struct modal *modal,int d) {
  if (MODAL->songidc<1) return;
  MODAL->songidp+=d;
  if (MODAL->songidp<0) MODAL->songidp=MODAL->songidc-1;
  else if (MODAL->songidp>=MODAL->songidc) MODAL->songidp=0;
  gui_term_writef(MODAL->term,12,1,"%d     ",MODAL->songidv[MODAL->songidp]);
}

static void modal_audio_adjust_force(struct modal *modal) {
  MODAL->force=MODAL->force?0:1;
  gui_term_write(MODAL->term,8,2,MODAL->force?"true ":"false",5);
}

static void modal_audio_adjust_repeat(struct modal *modal) {
  MODAL->repeat=MODAL->repeat?0:1;
  gui_term_write(MODAL->term,9,3,MODAL->repeat?"true ":"false",5);
}
 
static void modal_audio_adjust_playhead(struct modal *modal,int d) {
  const double step=5.000;
  MODAL->playhead+=d*step;
  if (MODAL->playhead<0.0) MODAL->playhead=0.0;
  int sec=(int)MODAL->playhead;
  int min=sec/60;
  sec%=60;
  gui_term_writef(MODAL->term,15,4,"%d:%02d        ",min,sec);
}
 
static void modal_audio_adjust_soundid(struct modal *modal,int d) {
  if (MODAL->soundidc<1) return;
  MODAL->soundidp+=d;
  if (MODAL->soundidp<0) MODAL->soundidp=MODAL->soundidc-1;
  else if (MODAL->soundidp>=MODAL->soundidc) MODAL->soundidp=0;
  gui_term_writef(MODAL->term,13,5,"%d     ",MODAL->soundidv[MODAL->soundidp]);
}
 
static void modal_audio_adjust_trim(struct modal *modal,int d) {
  const double step=0.0625;
  MODAL->trim+=d*step;
  if (MODAL->trim<0.0) MODAL->trim=0.0;
  else if (MODAL->trim>1.0) MODAL->trim=1.0;
  int dispv=(int)(MODAL->trim*100.0);
  if (dispv>100) dispv=100;
  gui_term_writef(MODAL->term,7,6,"%d   ",dispv);
}
 
static void modal_audio_adjust_pan(struct modal *modal,int d) {
  const double step=0.0625;
  MODAL->pan+=d*step;
  if (MODAL->pan<-1.0) MODAL->pan=-1.0;
  else if (MODAL->pan>1.0) MODAL->pan=1.0;
  int dispv=(int)(MODAL->pan*100.0);
  if (dispv>100) dispv=100;
  else if (dispv<-100) dispv=-100;
  gui_term_writef(MODAL->term,6,7,"%d    ",dispv);
}
 
static void modal_audio_adjust(struct modal *modal,int d) {
  egg_play_sound(RID_sound_uiadjust,0.5,(d<0)?-0.5:0.5);
  switch (MODAL->focusp) {
    case 1: modal_audio_adjust_songid(modal,d); break;
    case 2: modal_audio_adjust_force(modal); break;
    case 3: modal_audio_adjust_repeat(modal); break;
    case 4: modal_audio_adjust_playhead(modal,d); break;
    case 5: modal_audio_adjust_soundid(modal,d); break;
    case 6: modal_audio_adjust_trim(modal,d); break;
    case 7: modal_audio_adjust_pan(modal,d); break;
  }
}

/* Activate.
 */
 
static void modal_audio_start_song(struct modal *modal) {
  if ((MODAL->songidp<0)||(MODAL->songidp>=MODAL->songidc)) return;
  int rid=MODAL->songidv[MODAL->songidp];
  egg_play_song(1,rid,MODAL->repeat,1.0f,0.0f);
  // Do not update the now-playing fields; we poll for them aggressively during update.
}
 
static void modal_audio_commit_playhead(struct modal *modal) {
  egg_song_set(1,0xff,EGG_SONG_PROP_PLAYHEAD,MODAL->playhead);
}
 
static void modal_audio_start_sound(struct modal *modal) {
  if ((MODAL->soundidp<0)||(MODAL->soundidp>=MODAL->soundidc)) return;
  int soundid=MODAL->soundidv[MODAL->soundidp];
  egg_play_sound(soundid,MODAL->trim,MODAL->pan);
}
 
static void modal_audio_activate(struct modal *modal) {
  egg_play_sound(RID_sound_uiactivate,0.5,0.0);
  switch (MODAL->focusp) {
    case 1: modal_audio_start_song(modal); break;
    case 2: modal_audio_adjust_force(modal); break;
    case 3: modal_audio_adjust_repeat(modal); break;
    case 4: modal_audio_commit_playhead(modal); break;
    case 5: modal_audio_start_sound(modal); break;
    case 6: modal_audio_start_sound(modal); break;
    case 7: modal_audio_start_sound(modal); break;
  }
}

/* Play or release a manual note.
 */
 
static void modal_audio_note(struct modal *modal,int holdp,int value) {
  #if 0 /*TODO*/
  if ((holdp<0)||(holdp>=HOLD_LIMIT)) return;
  if (value) {
    int noteid=0x38+holdp*4;
    MODAL->holdv[holdp]=egg_play_note(0,noteid,0x40,2000);
  } else if (MODAL->holdv[holdp]>0) {
    egg_release_note(MODAL->holdv[holdp]);
    MODAL->holdv[holdp]=0;
  }
  #endif
}

/* Input.
 */
 
static void _audio_input(struct modal *modal,int btnid,int value) {
  if (value) switch (btnid) {
    case EGG_BTN_UP: modal_audio_move(modal,-1); break;
    case EGG_BTN_DOWN: modal_audio_move(modal,1); break;
    case EGG_BTN_LEFT: modal_audio_adjust(modal,-1); break;
    case EGG_BTN_RIGHT: modal_audio_adjust(modal,1); break;
    case EGG_BTN_SOUTH: modal_audio_activate(modal); break;
    case EGG_BTN_WEST: egg_play_song(1,0,0,0.0f,0.0f); break; // Not perfect, but try to stop the music when we dismiss.
  }
  switch (btnid) { // regardless of value...
    // EAST and NORTH to play notes manually.
    case EGG_BTN_EAST: modal_audio_note(modal,0,value); break;
    case EGG_BTN_NORTH: modal_audio_note(modal,1,value); break;
    // L1 and R1 to adjust the wheel -- we poll for those.
  }
}

/* Update.
 */
 
static void _audio_update(struct modal *modal,double elapsed,int input,int pvinput) {
  if ((MODAL->playhead_alert-=elapsed)<=0.0) MODAL->playhead_alert=0.0;
  #if 0 /*TODO update demo for new audio api */
  int songid=egg_song_get_id();
  if (songid==MODAL->dispsongid) {
    if (songid) { // Something still playing; update the playhead display.
      double sf=egg_song_get_playhead();
      
      // The playhead should always increase frame by frame.
      // Except when a song changes or repeats or whatever.
      // But during normal play, if we get the same or lesser playhead, it warrants attention.
      if ((sf<=MODAL->pvplayhead)&&(sf>0.0)) {
        fprintf(stderr,"Playhead: %.06f => %.06f\n",MODAL->pvplayhead,sf);
        MODAL->playhead_alert=PLAYHEAD_ALERT_TIME;
      }
      MODAL->pvplayhead=sf;
      
      int ms=(int)(sf*1000.0);
      if (ms<0) ms=0;
      int sec=ms/1000; ms%=1000;
      int min=sec/60; sec%=60;
      if (min>99) { min=sec=99; ms=999; }
      gui_term_writef(MODAL->term,11,10,"%d:%02d.%03d",min,sec,ms);
    }
  } else {
    MODAL->dispsongid=songid;
    gui_term_writef(MODAL->term,15,9,"%d     ",songid);
    gui_term_write(MODAL->term,11,10,"          ",-1); // Blank the playhead output; we'll get it next frame.
  }
  #endif
  
  // Adjust the wheel.
  #if 0 /*TODO*/
  switch (input&(EGG_BTN_L1|EGG_BTN_R1)) {
    case EGG_BTN_L1: {
        if ((MODAL->wheeltarget-=10)<-512) MODAL->wheeltarget=-512;
      } break;
    case EGG_BTN_R1: {
        if ((MODAL->wheeltarget+=10)>511) MODAL->wheeltarget=511;
      } break;
    default: {
        if (MODAL->wheeltarget>0) {
          if ((MODAL->wheeltarget-=10)<0) MODAL->wheeltarget=0;
        } else if (MODAL->wheeltarget<0) {
          if ((MODAL->wheeltarget+=10)>0) MODAL->wheeltarget=0;
        }
      }
  }
  if (MODAL->wheelcurrent<MODAL->wheeltarget) {
    MODAL->wheelcurrent+=1;
    egg_adjust_wheel(0,MODAL->wheelcurrent);
  } else if (MODAL->wheelcurrent>MODAL->wheeltarget) {
    MODAL->wheelcurrent-=1;
    egg_adjust_wheel(0,MODAL->wheelcurrent);
  }
  #endif
  
  gui_term_update(MODAL->term,elapsed);
}

/* Render.
 */
 
static void _audio_render(struct modal *modal) {
  { // Highlight focussed row.
    int x,y,w,h;
    gui_term_get_bounds(&x,&y,&w,&h,MODAL->term,0,MODAL->focusp,100,1);
    graf_fill_rect(&g.graf,x,y,w,h,0x102060ff);
  }
  gui_term_render(MODAL->term);
  
  // If the playhead did something unexpected, show a quick "hey!".
  if (MODAL->playhead_alert>0.0) {
    int x,y,w,h;
    gui_term_get_bounds(&x,&y,&w,&h,MODAL->term,21,10,1,1);
    x+=w>>1;
    y+=h>>1;
    int alpha=(int)((MODAL->playhead_alert*255.0)/PLAYHEAD_ALERT_TIME);
    if (alpha<1) alpha=1; else if (alpha>0xff) alpha=0xff;
    graf_set_image(&g.graf,RID_image_tiles);
    graf_set_alpha(&g.graf,alpha);
    graf_tile(&g.graf,x,y,0x22,0);
  }
}

/* Populate (songidv) and (soundidv).
 * Don't let either be empty; add a zero at the start of each.
 * For songs, zero is meaningful. Sounds, not so much, but it's an administrative convenience for us.
 */
 
static int modal_audio_acquire_resource_ids(struct modal *modal) {
  {
    int pa=demo_resv_search(EGG_TID_song,0);
    if (pa<0) pa=-pa-1;
    int pz=demo_resv_search(EGG_TID_song,0x10000);
    if (pz<0) pz=-pz-1;
    int a=1+pz-pa;
    if (!(MODAL->songidv=malloc(sizeof(int)*a))) return -1;
    int *dst=MODAL->songidv;
    *dst++=0;
    const struct rom_entry *res=g.resv+pa;
    int i=pz-pa;
    for (;i-->0;dst++,res++) *dst=res->rid;
    MODAL->songidc=a;
  }
  {
    int pa=demo_resv_search(EGG_TID_sound,0);
    if (pa<0) pa=-pa-1;
    int pz=demo_resv_search(EGG_TID_sound,0x10000);
    if (pz<0) pz=-pz-1;
    int a=1+pz-pa;
    if (!(MODAL->soundidv=malloc(sizeof(int)*a))) return -1;
    int *dst=MODAL->soundidv;
    *dst++=0;
    const struct rom_entry *res=g.resv+pa;
    int i=pz-pa;
    for (;i-->0;dst++,res++) *dst=res->rid;
    MODAL->soundidc=a;
  }
  return 0;
}

/* Initialize.
 */
 
static int _audio_init(struct modal *modal) {
  modal->del=_audio_del;
  modal->input=_audio_input;
  modal->update=_audio_update;
  modal->render=_audio_render;
  
  MODAL->focusp=1;
  MODAL->force=1;
  MODAL->repeat=1;
  MODAL->trim=1.0;
  MODAL->pan=0.0;
  
  if (modal_audio_acquire_resource_ids(modal)<0) return -1;
  
  if (!(MODAL->term=gui_term_new(0,0,FBW,FBH))) return -1;
  gui_term_set_background(MODAL->term,0);
  gui_term_write(MODAL->term,1,1,"Play song: 0",-1);
  gui_term_write(MODAL->term,1,2,"Force: true",-1);
  gui_term_write(MODAL->term,1,3,"Repeat: true",-1);
  gui_term_write(MODAL->term,1,4,"Set playhead: 0:00",-1);
  gui_term_write(MODAL->term,1,5,"Play sound: 0",-1);
  gui_term_write(MODAL->term,1,6,"Trim: 100",-1);
  gui_term_write(MODAL->term,1,7,"Pan: 0",-1);
  gui_term_write(MODAL->term,1,9,"Current song: 0",-1);
  gui_term_write(MODAL->term,1,10,"Playhead: 0:00.000",-1);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_audio() {
  struct modal *modal=modal_new(sizeof(struct modal_audio));
  if (!modal) return 0;
  if (
    (_audio_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
