/* modal_audio_Playback.c
 * Play two songs concurrently, each with its own trim, pan, and playhead controls.
 * On top of that, play sound effects on demand. Again, with configurable trim and pan.
 */
 
#include "../demo.h"
#include "../gui/gui.h"

#define SONGPLAY_COUNT 3

#define PART_FIRST 1
#define PART_RID 1
#define PART_REPEAT 2
#define PART_TRIM 3
#define PART_PAN 4
#define PART_PLAYHEAD 5
#define PART_LAST 5

struct songplay {
  int x,y,w,h; // Position in framebuffer.
  int songid; // If zero, we're the sounds player, not a song player. Matches our index in the list.
  int rid;
  int repeat;
  float trim,pan;
  float playhead;
};

struct modal_playback {
  struct modal hdr;
  int *song_ridv,*sound_ridv;
  int song_ridc,sound_ridc;
  struct songplay songplayv[SONGPLAY_COUNT];
  int songplayp; // Which songplay is focussed?
  int songplaypart; // Which part of the focussed songplay is focussed?
};

#define MODAL ((struct modal_playback*)modal)

/* Delete.
 */
 
static void _playback_del(struct modal *modal) {
  if (MODAL->song_ridv) free(MODAL->song_ridv);
  if (MODAL->sound_ridv) free(MODAL->sound_ridv);
  // Silence both songs. Don't care whether we think something is playing or not.
  egg_play_song(1,0,0,0.0f,0.0f);
  egg_play_song(2,0,0,0.0f,0.0f);
}

/* Begin play.
 */
 
static void playback_play(struct modal *modal,struct songplay *songplay) {
  if (songplay->songid) {
    egg_play_song(songplay->songid,songplay->rid,songplay->repeat,songplay->trim,songplay->pan);
  } else {
    egg_play_sound(songplay->rid,songplay->trim,songplay->pan);
  }
}

/* Adjust rid.
 */
 
static void playback_adjust_rid(struct modal *modal,struct songplay *songplay,int d) {
  const int *ridv=0;
  int ridc=0;
  if (songplay->songid) {
    ridv=MODAL->song_ridv;
    ridc=MODAL->song_ridc;
  } else {
    ridv=MODAL->sound_ridv;
    ridc=MODAL->sound_ridc;
  }
  if (ridc<1) return;
  int ridp=-1;
  int i=0; for (;i<ridc;i++) {
    if (ridv[i]==songplay->rid) {
      ridp=i;
      break;
    }
  }
  ridp+=d;
  if (ridp<0) ridp=ridc-1;
  else if (ridp>=ridc) ridp=0;
  songplay->rid=ridv[ridp];
}

/* Adjust trim or pan.
 */

static void playback_adjust_trim(struct modal *modal,struct songplay *songplay,int d) {
  if (!songplay->songid) return;
  if (d<0) {
    songplay->trim-=0.100f;
    if (songplay->trim<0.0f) songplay->trim=0.0f;
  } else {
    songplay->trim+=0.100f;
    if (songplay->trim>1.0f) songplay->trim=1.0f;
  }
  egg_song_set(songplay->songid,0xff,EGG_SONG_PROP_TRIM,songplay->trim);
}

static void playback_adjust_pan(struct modal *modal,struct songplay *songplay,int d) {
  if (!songplay->songid) return;
  if (d<0) {
    songplay->pan-=0.100f;
    if (songplay->pan<-1.0f) songplay->pan=-1.0f;
  } else {
    songplay->pan+=0.100f;
    if (songplay->pan>1.0f) songplay->pan=1.0f;
  }
  egg_song_set(songplay->songid,0xff,EGG_SONG_PROP_PAN,songplay->pan);
}

/* Adjust playhead.
 */
 
static void playback_adjust_playhead(struct modal *modal,struct songplay *songplay,int d) {
  if (!songplay->songid) return;
  float step=(d<0)?-5.000:5.000;
  songplay->playhead+=step;
  egg_song_set(songplay->songid,0xff,EGG_SONG_PROP_PLAYHEAD,songplay->playhead);
}

/* Activate selected row.
 */
 
static void playback_activate(struct modal *modal) {
  if ((MODAL->songplayp<0)||(MODAL->songplayp>=SONGPLAY_COUNT)) return;
  struct songplay *songplay=MODAL->songplayv+MODAL->songplayp;
  // We could allow SOUTH to do different things per part, but I think "play" is always the right choice.
  playback_play(modal,songplay);
}

/* Adjust value of selected row.
 */
 
static void playback_adjust(struct modal *modal,int d) {
  if ((MODAL->songplayp<0)||(MODAL->songplayp>=SONGPLAY_COUNT)) return;
  struct songplay *songplay=MODAL->songplayv+MODAL->songplayp;
  switch (MODAL->songplaypart) {
    case PART_RID: playback_adjust_rid(modal,songplay,d); break;
    case PART_REPEAT: songplay->repeat^=1; break;
    case PART_TRIM: playback_adjust_trim(modal,songplay,d); break;
    case PART_PAN: playback_adjust_pan(modal,songplay,d); break;
    case PART_PLAYHEAD: playback_adjust_playhead(modal,songplay,d); break;
  }
}

/* Move cursor.
 */
 
static void playback_move(struct modal *modal,int d) {
  int panic=100;
  while (panic-->0) {
    // Step by one part, and step songplay when we breach an edge.
    MODAL->songplaypart+=d;
    if (MODAL->songplaypart<PART_FIRST) {
      MODAL->songplaypart=PART_LAST;
      if (--(MODAL->songplayp)<0) MODAL->songplayp=SONGPLAY_COUNT-1;
    } else if (MODAL->songplaypart>PART_LAST) {
      MODAL->songplaypart=PART_FIRST;
      if (++(MODAL->songplayp)>=SONGPLAY_COUNT) MODAL->songplayp=0;
    }
    // If that yields an interactable part, we're done. REPEAT and PLAYHEAD only exist for songs.
    struct songplay *songplay=MODAL->songplayv+MODAL->songplayp;
    switch (MODAL->songplaypart) {
      case PART_RID: return;
      case PART_REPEAT: if (songplay->songid) return; break;
      case PART_TRIM: return;
      case PART_PAN: return;
      case PART_PLAYHEAD: if (songplay->songid) return; break;
    }
  }
}

/* Input.
 */
 
static void _playback_input(struct modal *modal,int btnid,int value) {
  if (value) switch (btnid) {
    case EGG_BTN_SOUTH: playback_activate(modal); break;
    case EGG_BTN_LEFT: playback_adjust(modal,-1); break;
    case EGG_BTN_RIGHT: playback_adjust(modal,1); break;
    case EGG_BTN_UP: playback_move(modal,-1); break;
    case EGG_BTN_DOWN: playback_move(modal,1); break;
  }
}

/* Update.
 */
 
static void _playback_update(struct modal *modal,double elapsed,int input,int pvinput) {
}

/* Render.
 */
 
static void songplay_render(struct modal *modal,struct songplay *songplay) {
  // Render the 5 parts in the order they're declared above (PART_*), for convenience.
  int y=songplay->y,part=1;
  #define TEXTROW(fmt,...) { \
    if ((MODAL->songplayp==songplay->songid)&&(MODAL->songplaypart==part)) { \
      graf_set_tint(&g.graf,0); \
      graf_fill_rect(&g.graf,songplay->x,y,songplay->w,8,0x3040c0ff); \
    } \
    char tmp[32]; \
    int tmpc=snprintf(tmp,sizeof(tmp),fmt,##__VA_ARGS__); \
    if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0; \
    gui_render_string(songplay->x,y,tmp,tmpc,0xffffffff); \
    y+=8; \
    part++; \
  }
  if (songplay->songid) {
    TEXTROW("Song %d: %d",songplay->songid,songplay->rid)
  } else {
    TEXTROW("Sound: %d",songplay->rid)
  }
  if (songplay->songid) {
    TEXTROW("%s",songplay->repeat?"Repeat":"Once")
  } else {
    part++;
  }
  TEXTROW("Trim: %.03f",songplay->trim)
  TEXTROW("Pan: %+.03f",songplay->pan)
  if (songplay->songid) {
    songplay->playhead=egg_song_get_playhead(songplay->songid);
    TEXTROW("Playhead: %.03f",songplay->playhead)
  } else {
    part++;
  }
  #undef TEXTROW
}

static void _playback_render(struct modal *modal) {
  struct songplay *songplay=MODAL->songplayv;
  int i=SONGPLAY_COUNT;
  for (;i-->0;songplay++) {
    songplay_render(modal,songplay);
  }
  graf_set_tint(&g.graf,0);
}

/* Initialize (song_ridv,sound_ridv) with all known resource IDs.
 */
 
static int playback_acquire_rids_1(void *dstpp,int tid) {
  int pa=demo_resv_search(tid,1);
  if (pa<0) pa=-pa-1;
  int pz=demo_resv_search(tid+1,0);
  if (pz<0) pz=-pz-1;
  int c=1+pz-pa; // "1+" because we'll add a zero at the beginning.
  if (c<=0) return 0;
  int *v=malloc(sizeof(int)*c);
  if (!v) return -1;
  const struct rom_entry *res=g.resv+pa;
  int *dst=v;
  int i=c;
  *dst++=0; i--; // First rid is zero.
  for (;i-->0;res++,dst++) *dst=res->rid;
  *(void**)dstpp=v;
  return c;
}
 
static int playback_acquire_rids(struct modal *modal) {
  if ((MODAL->song_ridc=playback_acquire_rids_1(&MODAL->song_ridv,EGG_TID_song))<0) return -1;
  if ((MODAL->sound_ridc=playback_acquire_rids_1(&MODAL->sound_ridv,EGG_TID_sound))<0) return -1;
  return 0;
}

/* Initialize.
 */
 
static int _playback_init(struct modal *modal) {
  modal->del=_playback_del;
  modal->input=_playback_input;
  modal->update=_playback_update;
  modal->render=_playback_render;
  
  if (playback_acquire_rids(modal)<0) return -1;
  
  int rowh=FBH/SONGPLAY_COUNT;
  struct songplay *songplay=MODAL->songplayv;
  int songid=0,y=0;
  for (;songid<SONGPLAY_COUNT;songid++,songplay++,y+=rowh) {
    songplay->x=0;
    songplay->y=y;
    songplay->w=FBW;
    songplay->h=rowh;
    songplay->songid=songid;
    songplay->rid=0;
    songplay->repeat=1;
    songplay->trim=1.0f;
    songplay->pan=0.0f;
    songplay->playhead=0.0f;
  }
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_audio_Playback() {
  struct modal *modal=modal_new(sizeof(struct modal_playback));
  if (!modal) return 0;
  if (
    (_playback_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
