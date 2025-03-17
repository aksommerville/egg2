/* eggrt_api.c
 * Egg Platform API with plain C linkage.
 */

#include "eggrt_internal.h"

/* Odds, ends.
 */
 
void egg_terminate(int status) {
  eggrt.terminate=1;
  eggrt.status=status;
}

void egg_log(const char *msg) {
  int msgc=0;
  if (msg) {
    while (msg[msgc]) msgc++;
    while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  }
  fprintf(stderr,"GAME: %s\n",msg);
}

double egg_time_real() {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0.0;
}

void egg_time_local(int *dst,int dsta) {
  fprintf(stderr,"TODO %s\n",__func__);
}

int egg_prefs_get(int k) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_prefs_set(int k,int v) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

/* Storage.
 */

int egg_rom_get(void *dst,int dsta) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_rom_get_res(void *dst,int dsta,int tid,int rid) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_store_get(char *v,int va,const char *k,int kc) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_store_set(const char *k,int kc,const char *v,int vc) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

int egg_store_key_by_index(char *k,int ka,int p) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

/* Input.
 */
 
void egg_input_configure() {
  fprintf(stderr,"TODO %s\n",__func__);
}

int egg_input_get_all(int *statev,int statea) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_input_get_one(int playerid) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_event_get(struct egg_event *dst,int dsta) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_event_enable(int evttype,int enable) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

int egg_event_is_enabled(int evttype) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_gamepad_get_name(char *dst,int dsta,int *vid,int *pid,int *version,int devid) {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

int egg_gamepad_get_button(int *btnid,int *hidusage,int *lo,int *hi,int *rest,int devid,int btnix) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

/* Audio.
 */
 
void egg_play_sound(int soundid,double trim,double pan) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_play_song(int songid,int force,int repeat) {
  fprintf(stderr,"TODO %s\n",__func__);
}

int egg_song_get_id() {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0;
}

double egg_song_get_playhead() {
  fprintf(stderr,"TODO %s\n",__func__);
  return 0.0;
}

void egg_song_set_playhead(double playhead) {
  fprintf(stderr,"TODO %s\n",__func__);
}

/* Video.
 */
 
void egg_video_get_screen_size(int *w,int *h) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_video_fb_from_screen(int *x,int *y) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_video_screen_from_fb(int *x,int *y) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_texture_del(int texid) {
  fprintf(stderr,"TODO %s\n",__func__);
}

int egg_texture_new() {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

void egg_texture_get_size(int *w,int *h,int texid) {
  fprintf(stderr,"TODO %s\n",__func__);
}

int egg_texture_load_image(int texid,int imageid) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

int egg_texture_load_raw(int texid,int w,int h,int stride,const void *src,int srcc) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

int egg_texture_get_pixels(void *dst,int dsta,int texid) {
  fprintf(stderr,"TODO %s\n",__func__);
  return -1;
}

void egg_texture_clear(int texid) {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_render(const struct egg_render_uniform *uniform,const void *vtxv,int vtxc) {
  fprintf(stderr,"TODO %s\n",__func__);
}
