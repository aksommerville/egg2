/* modal.h
 * Generic definition of a modal, where all the work of this demo will happen.
 */
 
#ifndef MODAL_H
#define MODAL_H

struct modal {
  const void *id; // Point to its own constructor, just as a reference.
  int defunct; // Set nonzero and main will kill you later; the normal way to destroy a modal.
  int suppress_exit; // Normally main pops the top modal when user presses B. Nonzero if you have your own suppression mechanism.
  int opaque; // Nonzero if you guarantee to fill the framebuffer every time; main will skip its background fill.
  void (*del)(struct modal *modal);
  void (*input)(struct modal *modal,int btnid,int value); // Modals may opt to receive individual events from main.
  void (*update)(struct modal *modal,double elapsed,int input,int pvinput);
  void (*render)(struct modal *modal); // Main fills the framebuffer first.
};

/* Only other modal facilities should call these.
 */
void modal_del(struct modal *modal);
struct modal *modal_new(int len);

/* Hands off ownership to the global stack.
 */
int modal_push(struct modal *modal);

int modal_drop_defunct();

int modal_is_resident(const struct modal *modal);

/* Below are public constructors that all return WEAK.
 *****************************************************************************/
 
struct modal *modal_new_home();

#endif
