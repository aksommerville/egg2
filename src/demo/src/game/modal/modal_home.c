#include "../demo.h"
#include "../gui/gui.h"

#define OPT_QUIT 1
#define OPT_VIDEO 2
#define OPT_AUDIO 3
#define OPT_INPUT 4
#define OPT_REGRESSION 5
#define OPT_STORAGE 6
#define OPT_MISC 7
#define OPT_INPUT_CONFIG 8

struct modal_home {
  struct modal hdr;
  struct gui_list *list;
};

#define MODAL ((struct modal_home*)modal)

/* Delete.
 */
 
static void _home_del(struct modal *modal) {
  gui_list_del(MODAL->list);
}

/* Update.
 */
 
static void _home_update(struct modal *modal,double elapsed,int input,int pvinput) {
  gui_list_update(MODAL->list,elapsed,input,pvinput);
}

/* Render.
 */
 
static void _home_render(struct modal *modal) {
  gui_list_render(MODAL->list);
}

/* Activate.
 */
 
static void modal_home_cb_activate(struct gui_list *list,int optionid) {
  struct modal *modal=gui_list_get_userdata(list);
  switch (optionid) {
    case OPT_QUIT: modal->defunct=1; break;
    case OPT_VIDEO: modal_new_video(); break;
    case OPT_AUDIO: modal_new_audio(); break;
    case OPT_INPUT: modal_new_input(); break;
    case OPT_REGRESSION: modal_new_regression(); break;
    case OPT_STORAGE: modal_new_storage(); break;
    case OPT_MISC: modal_new_misc(); break;
    case OPT_INPUT_CONFIG: egg_input_configure(); break;
  }
}

/* Initialize.
 */
 
static int _home_init(struct modal *modal) {
  modal->del=_home_del;
  modal->update=_home_update;
  modal->render=_home_render;
  
  if (!(MODAL->list=gui_list_new(0,0,FBW,FBH))) return -1;
  gui_list_set_userdata(MODAL->list,modal);
  gui_list_cb_activate(MODAL->list,0,modal_home_cb_activate);
  gui_list_insert(MODAL->list,-1,OPT_VIDEO,"Video",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_AUDIO,"Audio",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_INPUT,"Input",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_INPUT_CONFIG,"Input Config",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_STORAGE,"Storage",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_MISC,"Miscellaneous",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_REGRESSION,"Regression",-1,1);
  gui_list_insert(MODAL->list,-1,OPT_QUIT,"Quit",-1,1);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_home() {
  struct modal *modal=modal_new(sizeof(struct modal_home));
  if (!modal) return 0;
  if (
    (_home_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
