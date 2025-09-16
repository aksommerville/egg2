#include "../demo.h"

struct modal_home {
  struct modal hdr;
};

#define MODAL ((struct modal_home*)modal)

/* Delete.
 */
 
static void _home_del(struct modal *modal) {
}

/* Input.
 */
 
static void _home_input(struct modal *modal,int btnid,int value) {
  fprintf(stderr,"%s 0x%04x=%d\n",__func__,btnid,value);
}

/* Update.
 */
 
static void _home_update(struct modal *modal,double elapsed,int input,int pvinput) {
}

/* Render.
 */
 
static void _home_render(struct modal *modal) {
}

/* Initialize.
 */
 
static int _home_init(struct modal *modal) {
  modal->del=_home_del;
  modal->input=_home_input;
  modal->update=_home_update;
  modal->render=_home_render;
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
