/* eggstra_api.c
 * Stubs for the Egg Platform API, since opt units might call into it.
 */

#include "eggstra.h"
#include "egg/egg.h"

void egg_terminate(int status) {
}

void egg_log(const char *msg) {
}

double egg_time_real() {
  return 0.0;
}

void egg_time_local(int *dst,int dsta) {
}

int egg_prefs_get(int k) {
  return 0;
}

int egg_prefs_set(int k,int v) {
  return -1;
}

int egg_rom_get(void *dst,int dsta) {
  return 0;
}

int egg_rom_get_res(void *dst,int dsta,int tid,int rid) {
  return 0;
}

int egg_store_get(char *v,int va,const char *k,int kc) {
  return -1;
}

int egg_store_set(const char *k,int kc,const char *v,int vc) {
  return -1;
}

int egg_store_key_by_index(char *k,int ka,int p) {
  return 0;
}

void egg_input_configure() {
}

void egg_input_get_all(int *statev,int statea) {
}

int egg_input_get_one(int playerid) {
  return 0;
}

void egg_input_set_mode(int mode) {
}

int egg_input_get_mouse(int *x,int *y) {
  return 0;
}
 
void egg_play_sound(int soundid,float trim,float pan) {
}

void egg_play_song(int songid,int rid,int repeat,float trim,float pan) {
}

void egg_song_set(int songid,int chid,int prop,float v) {
}

void egg_song_event_note_on(int songid,int chid,int noteid,int velocity) {
}

void egg_song_event_note_off(int songid,int chid,int noteid) {
}

void egg_song_event_note_once(int songid,int chid,int noteid,int velocity,int durms) {
}

void egg_song_event_wheel(int songid,int chid,int v) {
}

float egg_song_get_playhead(int songid) {
  return 0.0f;
}

void egg_texture_del(int texid) {
}

int egg_texture_new() {
  return 0;
}

void egg_texture_get_size(int *w,int *h,int texid) {
}

int egg_texture_load_image(int texid,int imageid) {
  return -1;
}

int egg_texture_load_raw(int texid,int w,int h,int stride,const void *src,int srcc) {
  return -1;
}

int egg_texture_get_pixels(void *dst,int dsta,int texid) {
  return -1;
}

void egg_texture_clear(int texid) {
}

void egg_render(const struct egg_render_uniform *uniform,const void *vtxv,int vtxc) {
}
