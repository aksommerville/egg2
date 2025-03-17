#ifndef EGGRT_INTERNAL_H
#define EGGRT_INTERNAL_H

#include "egg/egg.h"
#include "opt/res/res.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct eggrt {

// eggrt_configure():
  const char *exename;
  
  int terminate;
  int status;
  int client_init_called;
  
// eggrt_rom.c:
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  
// Preferences exposed via Platform API:
  int lang;
  int music_enable;
  int sound_enable;
  
} eggrt;

void eggrt_print_help(const char *topic,int topicc);
int eggrt_configure(int argc,char **argv);

void eggrt_quit(int status);
int eggrt_init();
int eggrt_update();

void eggrt_rom_quit();
int eggrt_rom_init();
int eggrt_rom_search(int tid,int rid);

int eggrt_prefs_init();

/* Don't call the egg_client_* functions directly.
 * Technically today you could. But eventually I expect to include a Wasm runtime.
 * It's ok to call quit from here no matter what. Wrapper will prevent it going to the client if you hadn't called init.
 */
int eggrt_call_client_quit(int status);
int eggrt_call_client_init();
int eggrt_call_client_update(double elapsed);
int eggrt_call_client_render();

#endif
