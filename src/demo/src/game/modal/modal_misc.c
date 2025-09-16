#include "../demo.h"
#include "../gui/gui.h"

struct modal_misc {
  struct modal hdr;
  struct gui_list *list;
};

#define MODAL ((struct modal_misc*)modal)

/* Delete.
 */
 
static void _misc_del(struct modal *modal) {
  gui_list_del(MODAL->list);
}

/* Update.
 */
 
static void _misc_update(struct modal *modal,double elapsed,int input,int pvinput) {
  gui_list_update(MODAL->list,elapsed,input,pvinput);
}

/* Render.
 */
 
static void _misc_render(struct modal *modal) {
  gui_list_render(MODAL->list);
}

/* Activate.
 */
 
static void modal_misc_cb_activate(struct gui_list *list,int optionid) {
  struct modal *modal=gui_list_get_userdata(list);
  switch (optionid) {
    //TODO
  }
}

/* Initialize.
 */
 
static int _misc_init(struct modal *modal) {
  modal->del=_misc_del;
  modal->update=_misc_update;
  modal->render=_misc_render;
  
  if (!(MODAL->list=gui_list_new(0,0,FBW,FBH))) return -1;
  gui_list_set_userdata(MODAL->list,modal);
  gui_list_cb_activate(MODAL->list,0,modal_misc_cb_activate);
  gui_list_insert(MODAL->list,-1,0,"TODO modal_misc",-1,0);//TODO
  
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
