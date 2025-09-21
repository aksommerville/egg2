/* eggrt_api.c
 * Egg Platform API with plain C linkage.
 */

#include "eggrt_internal.h"
#include "opt/image/image.h"
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
        eggrt_language_changed();
      } return 0;
  
    case EGG_PREF_MUSIC: {
        v=v?1:0;
        if (v==eggrt.music_enable) return 0;
        if (eggrt.music_enable=v) {
          synth_play_song(eggrt.synth,eggrt.songid,0,eggrt.songrepeat);
        } else {
          synth_play_song(eggrt.synth,0,0,0);
        }
      } return 0;
  
    case EGG_PREF_SOUND: {
        v=v?1:0;
        if (v==eggrt.sound_enable) return 0;
        eggrt.sound_enable=v;
        // We could ask synth to cut off sound effects immediately, but I don't think it matters.
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
  return eggrt_store_get(v,va,k,kc);
}

int egg_store_set(const char *k,int kc,const char *v,int vc) {
  return eggrt_store_set(k,kc,v,vc);
}

int egg_store_key_by_index(char *k,int ka,int p) {
  return eggrt_store_key_by_index(k,ka,p);
}

/* Input.
 */
 
void egg_input_configure() {
  fprintf(stderr,"TODO %s\n",__func__);
}

void egg_input_get_all(int *statev,int statea) {
  if (!statev||(statea<1)) return;
  if (statea>INMGR_PLAYER_LIMIT) statea=INMGR_PLAYER_LIMIT; // sanity check
  int i=statea; while (i-->0) {
    statev[i]=inmgr_get_player(i);
  }
}

int egg_input_get_one(int playerid) {
  return inmgr_get_player(playerid);
}

/* Audio.
 */
 
void egg_play_sound(int soundid,double trim,double pan) {
  if (!eggrt.sound_enable) return;
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_play_sound(eggrt.synth,soundid,trim,pan);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_play_song(int songid,int force,int repeat) {
  eggrt.songid=songid;
  eggrt.songrepeat=repeat;
  if (!eggrt.music_enable) return;
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_play_song(eggrt.synth,songid,force,repeat);
  hostio_audio_unlock(eggrt.hostio);
}

int egg_play_note(int chid,int noteid,int velocity,int durms) {
  if (!eggrt.music_enable) return 0;
  if (hostio_audio_lock(eggrt.hostio)<0) return 0;
  int holdid=synth_play_note(eggrt.synth,chid,noteid,velocity,durms);
  hostio_audio_unlock(eggrt.hostio);
  return holdid;
}

void egg_release_note(int holdid) {
  if (holdid<1) return;
  if (!eggrt.music_enable) return;
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_release_note(eggrt.synth,holdid);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_adjust_wheel(int chid,int v) {
  if (!eggrt.music_enable) return;
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_adjust_wheel(eggrt.synth,chid,v);
  hostio_audio_unlock(eggrt.hostio);
}

int egg_song_get_id() {
  return synth_get_songid(eggrt.synth);
}

double egg_song_get_playhead() {
  if (hostio_audio_lock(eggrt.hostio)<0) return 0.0;
  double p=synth_get_playhead(eggrt.synth);
  if (p<=0.0) {
    hostio_audio_unlock(eggrt.hostio);
    return 0.0;
  }
  double remaining=hostio_audio_estimate_remaining_buffer(eggrt.hostio->audio);
  hostio_audio_unlock(eggrt.hostio);
  if (remaining>=p) return 0.0;
  return p-remaining;
}

void egg_song_set_playhead(double playhead) {
  synth_set_playhead(eggrt.synth,playhead);
}

/* Video.
 */

void egg_texture_del(int texid) {
  render_texture_del(eggrt.render,texid);
}

int egg_texture_new() {
  return render_texture_new(eggrt.render);
}

void egg_texture_get_size(int *w,int *h,int texid) {
  render_texture_get_size(w,h,eggrt.render,texid);
}

int egg_texture_load_image(int texid,int imageid) {
  if (texid<1) return -1;
  int p=eggrt_rom_search(EGG_TID_image,imageid);
  if (p<0) return -1;
  const struct rom_entry *res=eggrt.resv+p;
  int w=0,h=0;
  if (image_measure(&w,&h,res->v,res->c)<0) return -1;
  if ((w<1)||(h<1)||(w>EGG_TEXTURE_SIZE_LIMIT)||(h>EGG_TEXTURE_SIZE_LIMIT)) return -1;
  int stride=w<<2;
  int pixelslen=stride*h;
  void *pixels=malloc(pixelslen);
  if (!pixels) return -1;
  int err=image_decode(pixels,pixelslen,res->v,res->c);
  if (err>=0) err=render_texture_load_raw(eggrt.render,texid,w,h,stride,pixels,pixelslen);
  free(pixels);
  return err;
}

int egg_texture_load_raw(int texid,int w,int h,int stride,const void *src,int srcc) {
  return render_texture_load_raw(eggrt.render,texid,w,h,stride,src,srcc);
}

int egg_texture_get_pixels(void *dst,int dsta,int texid) {
  return render_texture_get_pixels(dst,dsta,eggrt.render,texid);
}

void egg_texture_clear(int texid) {
  render_texture_clear(eggrt.render,texid);
}

void egg_render(const struct egg_render_uniform *uniform,const void *vtxv,int vtxc) {
  if (!uniform||!uniform->dsttexid) return;
  render_render(eggrt.render,uniform,vtxv,vtxc);
}
