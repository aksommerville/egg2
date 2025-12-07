#include "../demo.h"
#include "../gui/gui.h"

struct modal_interference {
  struct modal hdr;
};

#define MODAL ((struct modal_interference*)modal)

/* Delete.
 */
 
static void _interference_del(struct modal *modal) {
}

/* Input.
 */
 
static void _interference_input(struct modal *modal,int btnid,int value) {
}

/* Update.
 */
 
static void _interference_update(struct modal *modal,double elapsed,int input,int pvinput) {
}

/* Render.
 */

static void _interference_render(struct modal *modal) {
}

/* Initialize.
 */
 
static int _interference_init(struct modal *modal) {
  modal->del=_interference_del;
  modal->input=_interference_input;
  modal->update=_interference_update;
  modal->render=_interference_render;
  
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
