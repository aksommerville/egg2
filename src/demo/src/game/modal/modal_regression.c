/* modal_regression.c
 * Tests exposing specific bugs I've found, mostly used during troubleshooting, but also keep around to notice if they come back.
 * We operate like Video and Audio -- no need to change anything here. Add your declaration in modal.h.
 */

#include "../demo.h"
#include "../gui/gui.h"

struct modal_regression {
  struct modal hdr;
  struct gui_list *list;
};

#define MODAL ((struct modal_regression*)modal)

/* Delete.
 */
 
static void _regression_del(struct modal *modal) {
  gui_list_del(MODAL->list);
}

/* Update.
 */
 
static void _regression_update(struct modal *modal,double elapsed,int input,int pvinput) {
  gui_list_update(MODAL->list,elapsed,input,pvinput);
}

/* Render.
 */
 
static void _regression_render(struct modal *modal) {
  gui_list_render(MODAL->list);
}

/* Activate.
 */
 
static void modal_regression_cb_activate(struct gui_list *list,int optionid) {
  struct modal *modal=gui_list_get_userdata(list);
  switch (optionid) {
    #define _(tag) case REGRESSION_TEST_##tag: modal_new_regression_##tag(); break;
    REGRESSION_FOR_EACH_TEST
    #undef _
  }
}

/* Initialize.
 */
 
static int _regression_init(struct modal *modal) {
  modal->del=_regression_del;
  modal->update=_regression_update;
  modal->render=_regression_render;
  
  if (!(MODAL->list=gui_list_new(0,0,FBW,FBH))) return -1;
  gui_list_set_userdata(MODAL->list,modal);
  gui_list_cb_activate(MODAL->list,0,modal_regression_cb_activate);
  #define _(tag) gui_list_insert(MODAL->list,-1,0,#tag,-1,REGRESSION_TEST_##tag);
  REGRESSION_FOR_EACH_TEST
  #undef _
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_regression() {
  struct modal *modal=modal_new(sizeof(struct modal_regression));
  if (!modal) return 0;
  if (
    (_regression_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
