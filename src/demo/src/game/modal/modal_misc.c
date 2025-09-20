#include "../demo.h"
#include "../gui/gui.h"

#define MISC_ROW_LOCAL 1
#define MISC_ROW_REAL 2
#define MISC_ROW_LOG 3
#define MISC_ROW_LANG 4
#define MISC_ROW_MUSIC 5
#define MISC_ROW_SOUND 6

#define MISC_ROW_FIRST 3
#define MISC_ROW_LAST 6

#define LANG_LIMIT 128 /* 183 of the 676 possible ISO 639-1 codes are assigned. If a game contains more than 10, that's impressive. 1 and 2 are typical. */

struct modal_misc {
  struct modal hdr;
  struct gui_term *term;
  int focusp; // term row
  uint16_t langv[LANG_LIMIT];
  int langc;
  char langstore[3];
};

#define MODAL ((struct modal_misc*)modal)

/* Delete.
 */
 
static void _misc_del(struct modal *modal) {
  gui_term_del(MODAL->term);
}

/* Move focus.
 */
 
static void modal_misc_move(struct modal *modal,int d) {
  MODAL->focusp+=d;
  if (MODAL->focusp<MISC_ROW_FIRST) MODAL->focusp=MISC_ROW_LAST;
  else if (MODAL->focusp>MISC_ROW_LAST) MODAL->focusp=MISC_ROW_FIRST;
  egg_play_sound(RID_sound_uimotion,0.5,0.0);
}

/* Next or previous language from list.
 */
 
static int modal_misc_next_lang(const struct modal *modal,int lang,int d) {
  if (MODAL->langc<2) return lang;
  int i=0;
  for (;i<MODAL->langc;i++) {
    if (MODAL->langv[i]==lang) {
      int np=i+d;
      if (np<0) np=MODAL->langc-1;
      else if (np>=MODAL->langc) np=0;
      return MODAL->langv[np];
    }
  }
  return MODAL->langv[0]; // Not (lang). Return something that does exist.
}

/* Nul-terminated string for language code.
 */
 
static const char *modal_lang_repr(struct modal *modal,int lang) {
  EGG_STRING_FROM_LANG(MODAL->langstore,lang);
  if (
    (MODAL->langstore[0]<'a')||(MODAL->langstore[0]>'z')||
    (MODAL->langstore[1]<'a')||(MODAL->langstore[1]>'z')
  ) MODAL->langstore[0]=MODAL->langstore[1]='?';
  MODAL->langstore[2]=0;
  return MODAL->langstore;
}

/* Adjust focussed row.
 */
 
static void modal_misc_adjust_log(struct modal *modal,int d) {
  char msg[]="Log(?)";
  msg[4]=(d<0)?'L':(d>0)?'R':'-';
  egg_log(msg);
}

static void modal_misc_adjust_lang(struct modal *modal,int d) {
  int lang=egg_prefs_get(EGG_PREF_LANG);
  lang=modal_misc_next_lang(modal,lang,d);
  egg_prefs_set(EGG_PREF_LANG,lang);
  gui_term_writef(MODAL->term,1,MISC_ROW_LANG,"Prefs(LANG): %s",modal_lang_repr(modal,egg_prefs_get(EGG_PREF_LANG)));
}

static void modal_misc_adjust_music(struct modal *modal,int d) {
  egg_prefs_set(EGG_PREF_MUSIC,egg_prefs_get(EGG_PREF_MUSIC)?0:1);
  gui_term_writef(MODAL->term,1,MISC_ROW_MUSIC,"Prefs(MUSIC): %d",egg_prefs_get(EGG_PREF_MUSIC));
}
 
static void modal_misc_adjust_sound(struct modal *modal,int d) {
  egg_prefs_set(EGG_PREF_SOUND,egg_prefs_get(EGG_PREF_SOUND)?0:1);
  gui_term_writef(MODAL->term,1,MISC_ROW_SOUND,"Prefs(SOUND): %d",egg_prefs_get(EGG_PREF_SOUND));
}
 
static void modal_misc_adjust(struct modal *modal,int d) {
  egg_play_sound(RID_sound_uiadjust,0.5,(d<0)?-0.5:0.5);
  switch (MODAL->focusp) {
    case MISC_ROW_LOG: modal_misc_adjust_log(modal,d); break;
    case MISC_ROW_LANG: modal_misc_adjust_lang(modal,d); break;
    case MISC_ROW_MUSIC: modal_misc_adjust_music(modal,d); break;
    case MISC_ROW_SOUND: modal_misc_adjust_sound(modal,d); break;
  }
}

/* Activate.
 */
 
static void modal_misc_activate(struct modal *modal) {
  egg_play_sound(RID_sound_uiactivate,0.5,0.0);
  switch (MODAL->focusp) {
    case MISC_ROW_LOG: modal_misc_adjust_log(modal,0); break;
    case MISC_ROW_LANG: modal_misc_adjust_lang(modal,1); break;
    case MISC_ROW_MUSIC: modal_misc_adjust_music(modal,1); break;
    case MISC_ROW_SOUND: modal_misc_adjust_sound(modal,1); break;
  }
}

/* Input.
 */
 
static void _misc_input(struct modal *modal,int btnid,int value) {
  if (value) switch (btnid) {
    case EGG_BTN_UP: modal_misc_move(modal,-1); break;
    case EGG_BTN_DOWN: modal_misc_move(modal,1); break;
    case EGG_BTN_LEFT: modal_misc_adjust(modal,-1); break;
    case EGG_BTN_RIGHT: modal_misc_adjust(modal,1); break;
    case EGG_BTN_SOUTH: modal_misc_activate(modal); break;
  }
}

/* Update.
 */
 
static void _misc_update(struct modal *modal,double elapsed,int input,int pvinput) {
  {
    double sf=egg_time_real();
    gui_term_writef(MODAL->term,1,MISC_ROW_REAL,"Real time: %.03f",sf);
  }
  {
    int vv[7]={0};
    egg_time_local(vv,7);
    gui_term_writef(MODAL->term,1,MISC_ROW_LOCAL,"Local time: %04d-%02d-%02dT%02d:%02d:%02d.%03d",vv[0],vv[1],vv[2],vv[3],vv[4],vv[5],vv[6]);
  }
  gui_term_update(MODAL->term,elapsed);
}

/* Render.
 */
 
static void _misc_render(struct modal *modal) {
  { // Highlight focussed row.
    int x,y,w,h;
    gui_term_get_bounds(&x,&y,&w,&h,MODAL->term,0,MODAL->focusp,100,1);
    graf_fill_rect(&g.graf,x,y,w,h,0x102060ff);
  }
  gui_term_render(MODAL->term);
}

/* Compose the list of available languages.
 */
 
static int modal_misc_add_language(struct modal *modal,int lang) {
  if ((lang<1)||(lang>0xffff)) return -1;
  int lo=0,hi=MODAL->langc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=MODAL->langv[ck];
         if (lang<q) hi=ck;
    else if (lang>q) lo=ck+1;
    else return 0;
  }
  if (MODAL->langc>=LANG_LIMIT) return -1;
  memmove(MODAL->langv+lo+1,MODAL->langv+lo,sizeof(uint16_t)*(MODAL->langc-lo));
  MODAL->langc++;
  MODAL->langv[lo]=lang;
  return 0;
}
 
static void modal_misc_gather_languages(struct modal *modal) {

  // Check rid of all strings resources; add every language to our list.
  int resp=demo_resv_search(EGG_TID_strings,0);
  if (resp<0) resp=-resp-1;
  const struct rom_entry *res=g.resv+resp;
  int pv=0;
  while ((resp<g.resc)&&(res->tid==EGG_TID_strings)) {
    int lang=res->rid>>6;
    if (lang!=pv) {
      modal_misc_add_language(modal,lang);
      pv=lang;
    }
    resp++;
    res++;
  }
  
  // Also include the current language, and finally "en" just to be super sure it isn't empty.
  modal_misc_add_language(modal,egg_prefs_get(EGG_PREF_LANG));
  modal_misc_add_language(modal,EGG_LANG_FROM_STRING("en"));
}

/* Initialize.
 */
 
static int _misc_init(struct modal *modal) {
  modal->del=_misc_del;
  modal->input=_misc_input;
  modal->update=_misc_update;
  modal->render=_misc_render;
  
  MODAL->focusp=MISC_ROW_FIRST;
  
  modal_misc_gather_languages(modal);
  if (MODAL->langc<1) return -1;
  
  if (!(MODAL->term=gui_term_new(0,0,FBW,FBH))) return -1;
  gui_term_set_background(MODAL->term,0);
  gui_term_write(MODAL->term,1,MISC_ROW_LOCAL,"Local time: ",-1);
  gui_term_write(MODAL->term,1,MISC_ROW_REAL,"Real time: ",-1);
  gui_term_write(MODAL->term,1,MISC_ROW_LOG,"Log",-1);
  gui_term_writef(MODAL->term,1,MISC_ROW_LANG,"Prefs(LANG): %s",modal_lang_repr(modal,egg_prefs_get(EGG_PREF_LANG)));
  gui_term_writef(MODAL->term,1,MISC_ROW_MUSIC,"Prefs(MUSIC): %d",egg_prefs_get(EGG_PREF_MUSIC));
  gui_term_writef(MODAL->term,1,MISC_ROW_SOUND,"Prefs(SOUND): %d",egg_prefs_get(EGG_PREF_SOUND));
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_misc() {
  struct modal *modal=modal_new(sizeof(struct modal_misc));
  if (!modal) return 0;
  if (
    (_misc_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
