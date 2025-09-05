/* macos.h
 * Glue for NSApplicationMain().
 */

#ifndef MACOS_H
#define MACOS_H

struct macioc_delegate {
  int rate; // hz, for update(). <=0 we will not update
  void *userdata;
  void (*focus)(void *userdata,int focus);
  void (*quit)(void *userdata);
  int (*init)(void *userdata);
  void (*update)(void *userdata);
};

int macos_prerun_argv(int argc,char **argv);

int macos_main(
  int argc,char **argv,
  void (*cb_quit)(void *userdata),
  int (*cb_init)(void *userdata),
  void (*cb_update)(void *userdata),
  void *userdata
);

void macioc_terminate(int status);

/* Get the preferred languages per user settings, in Egg's format.
 * Exactly equivalent to eggrt_get_user_languages() in eggrt_configure.c, and that function should call us first when applicable.
 */
int macos_get_preferred_languages(int *dst,int dsta);

#endif
