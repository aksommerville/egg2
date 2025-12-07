#include "../demo.h"
#include "../gui/gui.h"

#define GROUNDY ((FBH*3)/4)
#define DANGER_RANGE (FBW/5)

// Top levels for the Danger and Yoshi tracks.
#define DANGER_PEAK 0.333f
#define YOSHI_PEAK 0.400f

// Notes for the ocarina. The song is in G.
#define NOTEID_DOWN  0x43
#define NOTEID_LEFT  0x47
#define NOTEID_RIGHT 0x4a
#define NOTEID_UP    0x4f

struct modal_interference {
  struct modal hdr;
  int manx,camelx,firex;
  int framec;
  int mounted;
  int ocarining;
  uint8_t manxform;
  uint8_t camelxform;
  int texid;
  int pvdx;
};

#define MODAL ((struct modal_interference*)modal)

/* Delete.
 */
 
static void _interference_del(struct modal *modal) {
  egg_play_song(1,0,0,0.0f,0.0f);
  egg_texture_del(MODAL->texid);
}

/* Input.
 */
 
static void _interference_input(struct modal *modal,int btnid,int value) {
}

/* Mount or dismount the camel.
 */
 
static void interference_attempt_mount(struct modal *modal) {
  int dx=MODAL->camelx-MODAL->manx;
  if ((dx<-12)||(dx>12)) return;
  MODAL->mounted=1;
  MODAL->manx=MODAL->camelx;
  MODAL->manxform=MODAL->camelxform;
  egg_song_set(1,3,EGG_SONG_PROP_TRIM,YOSHI_PEAK);
}

static void interference_dismount(struct modal *modal) {
  MODAL->mounted=0;
  egg_song_set(1,3,EGG_SONG_PROP_TRIM,0.0f);
}

/* Compare man to fire and adjust danger track accordingly.
 */
 
static void interference_assess_danger(struct modal *modal) {
  int dx=MODAL->firex-MODAL->manx;
  if (dx<0) dx=-dx;
  if (dx>=DANGER_RANGE) {
    if (MODAL->pvdx<DANGER_RANGE) egg_song_set(1,2,EGG_SONG_PROP_TRIM,0.0f);
    MODAL->pvdx=dx;
    return;
  }
  if (MODAL->pvdx==dx) return;
  MODAL->pvdx=dx;
  float trim=1.0f-(float)dx/(float)DANGER_RANGE;
  trim*=DANGER_PEAK;
  egg_song_set(1,2,EGG_SONG_PROP_TRIM,trim);
}

/* Update.
 */
 
static void _interference_update(struct modal *modal,double elapsed,int input,int pvinput) {

  /* If ocarining, play a note or release.
   */
  if (MODAL->ocarining) {
    if (!(input&EGG_BTN_SOUTH)) {
      MODAL->ocarining=0;
      egg_song_event_note_off(1,0,NOTEID_DOWN);
      egg_song_event_note_off(1,0,NOTEID_LEFT);
      egg_song_event_note_off(1,0,NOTEID_RIGHT);
      egg_song_event_note_off(1,0,NOTEID_UP);
    } else {
      #define BLOW(tag) \
        if ((input&EGG_BTN_##tag)&&!(pvinput&EGG_BTN_##tag)) egg_song_event_note_on(1,0,NOTEID_##tag,0x40); \
        else if (!(input&EGG_BTN_##tag)&&(pvinput&EGG_BTN_##tag)) egg_song_event_note_off(1,0,NOTEID_##tag);
      BLOW(DOWN)
      BLOW(LEFT)
      BLOW(RIGHT)
      BLOW(UP) // kaboom!
      #undef BLOW
    }
    
  /* If mounted, move both man and camel, or dismount.
   */
  } else if (MODAL->mounted) {
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) {
      interference_dismount(modal);
    } else {
      if (input&EGG_BTN_SOUTH) {
        MODAL->ocarining=1;
      }
      int speed=1;
      switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
        case EGG_BTN_LEFT: MODAL->manxform=MODAL->camelxform=EGG_XFORM_XREV; MODAL->manx-=speed; MODAL->camelx-=speed; break;
        case EGG_BTN_RIGHT: MODAL->manxform=MODAL->camelxform=0; MODAL->manx+=speed; MODAL->camelx+=speed; break;
      }
    }
    
  /* Neither ocarining nor camelling. Let the man walk, or enter either state.
   */
  } else {
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) {
      interference_attempt_mount(modal);
    } else if (input&EGG_BTN_SOUTH) {
      MODAL->ocarining=1;
    } else {
      int speed=1;
      switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
        case EGG_BTN_LEFT: MODAL->manxform=EGG_XFORM_XREV; MODAL->manx-=speed; break;
        case EGG_BTN_RIGHT: MODAL->manxform=0; MODAL->manx+=speed; break;
      }
    }
  }
  
  interference_assess_danger(modal);
}

/* Render.
 */

static void _interference_render(struct modal *modal) {
  MODAL->framec++;
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,0xa0c0e0ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x008020ff);
  
  graf_set_input(&g.graf,MODAL->texid);
  graf_tile(&g.graf,MODAL->camelx,GROUNDY-8,0x24,MODAL->camelxform);
  graf_tile(&g.graf,MODAL->firex,GROUNDY-8,0x25,(MODAL->framec&0x10)?EGG_XFORM_XREV:0);
  uint8_t mantileid=MODAL->ocarining?0x23:0x21;
  if (MODAL->mounted) {
    graf_tile(&g.graf,MODAL->manx,GROUNDY-20,mantileid,MODAL->manxform);
  } else {
    graf_tile(&g.graf,MODAL->manx,GROUNDY-8,mantileid,MODAL->manxform);
  }
}

/* Initialize.
 */
 
static int _interference_init(struct modal *modal) {
  modal->del=_interference_del;
  modal->input=_interference_input;
  modal->update=_interference_update;
  modal->render=_interference_render;
  modal->opaque=1;
  
  MODAL->manx=FBW>>1;
  MODAL->camelx=FBW>>2;
  MODAL->firex=(FBW*3)>>2;
  
  if (egg_texture_load_image(MODAL->texid=egg_texture_new(),RID_image_tiles)<0) return -1;
  
  egg_play_song(1,RID_song_multitrack_demo,1,1.0f,0.0f);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_audio_Interference() {
  struct modal *modal=modal_new(sizeof(struct modal_interference));
  if (!modal) return 0;
  if (
    (_interference_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
