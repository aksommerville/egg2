/* 2025-12-07
 * Graf has been observed to flush its vertex cache in the middle of composite geometry.
 * eg it caches the first point of a line, then flushes, then caches the second point.
 * Obviously that line isn't getting rendered!
 * When this works, you'll see 5 rows of perfect straight vertical lines.
 * When broken, there are 3 or 4 obvious hiccups in the pattern.
 */

#include "../demo.h"
#include "../gui/gui.h"

struct modal_test {
  struct modal hdr;
};

#define MODAL ((struct modal_test*)modal)

/* Render.
 */
 
static void _test_render(struct modal *modal) {
  int y=1;
  for (;y<FBH;y+=40) {
    int x=1;
    for (;x<FBW;x+=2) {
      graf_line(&g.graf,x,y,0xffffffff,x,y+38,0xffffffff);
    }
  }
}

/* Initialize.
 */
 
static int _test_init(struct modal *modal) {
  modal->render=_test_render;
  // All we need to do is render.
  return 0;
}

/* New.
 */
 
struct modal *modal_new_regression_20251207_graf_sequence_break() {
  struct modal *modal=modal_new(sizeof(struct modal_test));
  if (!modal) return 0;
  if (
    (_test_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
