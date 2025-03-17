/* eggrt_client.c
 * Manages calls out to the client game.
 * For now, these are just plain C calls, we get linked against the game natively.
 */

#include "eggrt_internal.h"

int eggrt_call_client_quit(int status) {
  if (!eggrt.client_init_called) return 0;
  egg_client_quit(status);
  return 0;
}

int eggrt_call_client_init() {
  eggrt.client_init_called=1;
  return egg_client_init();
}

int eggrt_call_client_update(double elapsed) {
  egg_client_update(elapsed);
  return 0;
}

int eggrt_call_client_render() {
  egg_client_render();
  return 0;
}
