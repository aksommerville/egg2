#ifndef EGGRT_INTERNAL_H
#define EGGRT_INTERNAL_H

#include "egg/egg.h"
#include "util/res/res.h"
#include "opt/hostio/hostio.h"
#include "opt/synth/synth.h"
#include "opt/render/render.h"
#include "inmgr/inmgr.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

// eggrt.clockmode
#define EGGRT_CLOCKMODE_NORMAL   0 /* Sleep if necessary and return sanitized real time. */
#define EGGRT_CLOCKMODE_UNIFORM  1 /* Sleep if necessary but return a constant every time. */
#define EGGRT_CLOCKMODE_REDLINE  2 /* Never sleep, and return the same constant as UNIFORM. */

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
  char *store_req;
  
  int terminate;
  int status;
  int client_init_called;
  struct hostio *hostio;
  int songid,songrepeat;
  struct render *render;
  void *titlestorage,*iconstorage;
  int focus; // Current state of window manager focus. Hard-pause when blurred.
  int devid_keyboard;
  
// eggrt_rom.c:
  void *rom;
  int romc;
  struct rom_entry *resv;
  int resc,resa;
  struct {
    int fbw,fbh;
    const char *title; // default languageless
    int titlec;
    int title_strix;
    int icon_imageid;
    int playerclo,playerchi;
    const char *lang; // comma-delimited 631-1 codes
    int langc;
    const char *required;
    int requiredc;
    const char *optional;
    int optionalc;
  } metadata;
  
// eggrt_clock.c:
  int clockmode;
  int updframec;
  int clockfaultc;
  int clockclampc;
  double last_update_time;
  double starttime_real;
  double starttime_cpu;
  
// eggrt_store.c:
  struct eggrt_store_field {
    char *k,*v;
    int kc,vc;
  } *storev;
  int storec,storea;
  char *storepath; // Null if store not in play.
  int storedirty;
  int storedebounce;
  
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

void eggrt_clock_init(); // Caller sets eggrt.clockmode first.
double eggrt_clock_update(); // May sleep, and returns adjusted time for client consumption.
void eggrt_clock_report(); // Noop if insufficient data.

void eggrt_store_quit();
int eggrt_store_init();
int eggrt_store_update(); // Store gets routine updates in case it deferred saving.
int eggrt_store_get(char *v,int va,const char *k,int kc);
int eggrt_store_set(const char *k,int kc,const char *v,int vc);
int eggrt_store_key_by_index(char *k,int ka,int p);

void eggrt_language_changed();

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
void eggrt_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver);
int eggrt_cb_key(struct hostio_video *driver,int keycode,int value);
void eggrt_cb_connect(struct hostio_input *driver,int devid);
void eggrt_cb_disconnect(struct hostio_input *driver,int devid);
void eggrt_cb_button(struct hostio_input *driver,int devid,int btnid,int value);
void eggrt_cb_quit();

#endif
