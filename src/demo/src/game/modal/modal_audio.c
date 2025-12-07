/* Just a simple menu.
 * You shouldn't need to edit this ever.
 * Add things to modal.h:AUDIO_TEST_* instead.
 */

#include "../demo.h"
#include "../gui/gui.h"

struct modal_audio {
  struct modal hdr;
  struct gui_list *list;
};

#define MODAL ((struct modal_audio*)modal)

/* Delete.
 */
 
static void _audio_del(struct modal *modal) {
  gui_list_del(MODAL->list);
}

/* Activate.
 */
 
static void _audio_cb_activate(struct gui_list *list,int optionid) {
  struct modal *modal=gui_list_get_userdata(list);
  switch (optionid) {
    #define _(tag) case AUDIO_TEST_##tag: modal_new_audio_##tag(); break;
    AUDIO_FOR_EACH_TEST
    #undef _
  }
}

/* Principal hooks, forwarded to list.
 */
 
static void _audio_update(struct modal *modal,double elapsed,int input,int pvinput) {
  gui_list_update(MODAL->list,elapsed,input,pvinput);
}

static void _audio_render(struct modal *modal) {
  gui_list_render(MODAL->list);
}

/* Initialize.
 */
 
static int _audio_init(struct modal *modal) {
  modal->del=_audio_del;
  modal->update=_audio_update;
  modal->render=_audio_render;
  
  if (!(MODAL->list=gui_list_new(0,0,FBW,FBH))) return -1;
  gui_list_set_userdata(MODAL->list,modal);
  gui_list_cb_activate(MODAL->list,0,_audio_cb_activate);
  
  #define _(tag) gui_list_insert(MODAL->list,-1,0,#tag,-1,1);
  AUDIO_FOR_EACH_TEST
  #undef _
  
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
