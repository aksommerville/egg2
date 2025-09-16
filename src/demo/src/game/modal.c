#include "demo.h"

/* Delete.
 */
 
void modal_del(struct modal *modal) {
  if (!modal) return;
  if (modal->del) modal->del(modal);
  free(modal);
}

/* New.
 */
 
struct modal *modal_new(int len) {
  if (len<(int)sizeof(struct modal)) return 0;
  struct modal *modal=calloc(1,len);
  if (!modal) return 0;
  return modal;
}

/* Push to global stack.
 */
 
int modal_push(struct modal *modal) {
  if (!modal||modal->defunct) return -1;
  if (g.modalc>=g.modala) {
    int na=g.modala+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(g.modalv,sizeof(void*)*na);
    if (!nv) return -1;
    g.modalv=nv;
    g.modala=na;
  }
  g.modalv[g.modalc++]=modal;
  return 0;
}

/* Drop all defunct modals from the global stack.
 */

int modal_drop_defunct() {
  int i=g.modalc,rmc=0;;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (!modal->defunct) continue;
    g.modalc--;
    memmove(g.modalv+i,g.modalv+i+1,sizeof(void*)*(g.modalc-i));
    modal_del(modal);
    rmc++;
  }
  return rmc;
}

/* Test modal.
 */
 
int modal_is_resident(const struct modal *modal) {
  int i=g.modalc;
  while (i-->0) if (g.modalv[i]==modal) return 1;
  return 0;
}
