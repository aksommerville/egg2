#ifndef EGGRT_INTERNAL_H
#define EGGRT_INTERNAL_H

#include "egg/egg.h"
#include "opt/res/res.h"
#include "opt/hostio/hostio.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

extern struct eggrt {

// eggrt_configure():
  const char *exename;
  char *video_driver;
  int fullscreen;
  char *video_device;
  char *audio_driver;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  char *audio_device;
  char *input_driver;
  
  int terminate;
  int status;
  int client_init_called;
  struct hostio *hostio;
  
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

void eggrt_cb_close(struct hostio_video *driver);
void eggrt_cb_focus(struct hostio_video *driver,int focus);
void eggrt_cb_resize(struct hostio_video *driver,int w,int h);
int eggrt_cb_key(struct hostio_video *driver,int keycode,int value);
void eggrt_cb_text(struct hostio_video *driver,int codepoint);
void eggrt_cb_mmotion(struct hostio_video *driver,int x,int y);
void eggrt_cb_mbutton(struct hostio_video *driver,int btnid,int value);
void eggrt_cb_mwheel(struct hostio_video *driver,int dx,int dy);
void eggrt_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver);
void eggrt_cb_connect(struct hostio_input *driver,int devid);
void eggrt_cb_disconnect(struct hostio_input *driver,int devid);
void eggrt_cb_button(struct hostio_input *driver,int devid,int btnid,int value);

#endif
