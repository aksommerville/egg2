/* eggrt_api.c
 * Egg Platform API with plain C linkage.
 */

#include "eggrt_internal.h"
#include <sys/time.h>
#include <time.h>

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
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

void egg_time_local(int *dst,int dsta) {
  if (!dst||(dsta<1)) return;
  time_t now=time(0);
  struct tm tm={0};
  localtime_r(&now,&tm);
  dst[0]=1900+tm.tm_year; if (dsta<2) return;
  dst[1]=1+tm.tm_mon; if (dsta<3) return;
  dst[2]=tm.tm_mday; if (dsta<4) return;
  dst[3]=tm.tm_hour; if (dsta<5) return;
  dst[4]=tm.tm_min; if (dsta<6) return;
  dst[5]=tm.tm_sec; if (dsta<7) return;
  struct timeval tv={0};
  gettimeofday(&tv,0);
  dst[6]=tv.tv_usec/1000;
}

int egg_prefs_get(int k) {
  switch (k) {
    case EGG_PREF_LANG: return eggrt.lang;
    case EGG_PREF_MUSIC: return eggrt.music_enable;
    case EGG_PREF_SOUND: return eggrt.sound_enable;
  }
  return 0;
}

int egg_prefs_set(int k,int v) {
  switch (k) {
  
    case EGG_PREF_LANG: {
        if (v==eggrt.lang) return 0;
        char name[3]={0};
        EGG_STRING_FROM_LANG(name,v)
        if (!EGG_STRING_IS_LANG(name)) return -1;
        fprintf(stderr,"%s: Changing language to '%.2s'\n",eggrt.exename,name);
        eggrt.lang=v;
        //TODO Update window title etc.
      } return 0;
  
    case EGG_PREF_MUSIC: {
        v=v?1:0;
        if (v==eggrt.music_enable) return 0;
        if (eggrt.music_enable=v) {
        } else {
          //TODO Stop music.
        }
      } return 0;
  
    case EGG_PREF_SOUND: {
        v=v?1:0;
        if (v==eggrt.sound_enable) return 0;
        if (eggrt.sound_enable=v) {
        } else {
          //TODO Stop sounds? Or just let them run out?
        }
      } return 0;
  }
  return -1;
}

/* Storage.
 */

int egg_rom_get(void *dst,int dsta) {
  if (!dst||(dsta<0)) dsta=0;
  int cpc=eggrt.romc;
  if (cpc>dsta) cpc=dsta;
  memcpy(dst,eggrt.rom,cpc);
  return eggrt.romc;
}

int egg_rom_get_res(void *dst,int dsta,int tid,int rid) {
  if ((tid<1)||(rid<1)) return 0;
  if (!dst||(dsta<0)) dsta=0;
  int p=eggrt_rom_search(tid,rid);
  if (p<0) return 0;
  const struct rom_entry *res=eggrt.resv+p;
  int cpc=res->c;
  if (cpc>dsta) cpc=dsta;
  memcpy(dst,res->v,cpc);
  return res->c;
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
  if (!eggrt.sound_enable) return;
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
  *w=320;//XXX
  *h=180;
  fprintf(stderr,"TODO %s texid=%d, reporting %dx%d\n",__func__,texid,*w,*h);
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
