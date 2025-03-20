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
  if (statea>EGGRT_PLAYER_LIMIT) statea=EGGRT_PLAYER_LIMIT; // sanity check
  if (statea<=eggrt.inmgr.playerc) {
    memcpy(statev,eggrt.inmgr.playerv,sizeof(int)*statea);
  } else {
    memcpy(statev,eggrt.inmgr.playerv,sizeof(int)*eggrt.inmgr.playerc);
    memset(statev+eggrt.inmgr.playerc,0,sizeof(int)*(statea-eggrt.inmgr.playerc));
  }
}

int egg_input_get_one(int playerid) {
  if ((playerid<0)||(playerid>=eggrt.inmgr.playerc)) return 0;
  return eggrt.inmgr.playerv[playerid];
}

int egg_event_get(struct egg_event *dst,int dsta) {
  return inmgr_evtq_pop(dst,dsta,&eggrt.inmgr);
}

int egg_event_enable(int evttype,int enable) {
  return inmgr_event_enable(&eggrt.inmgr,evttype,enable);
}

int egg_event_is_enabled(int evttype) {
  if ((evttype<0)||(evttype>=32)) return 0;
  return (eggrt.inmgr.evtmask&(1<<evttype))?1:0;
}

int egg_gamepad_get_name(char *dst,int dsta,int *vid,int *pid,int *version,int devid) {
  if (!dst||(dsta<0)) dsta=0;
  int i=0;
  for (;i<eggrt.hostio->inputc;i++) {
    struct hostio_input *driver=eggrt.hostio->inputv[i];
    if (!driver->type->get_ids) continue;
    const char *name=driver->type->get_ids(vid,pid,version,driver,devid);
    if (!name) continue;
    int namec=0;
    while (name[namec]) namec++;
    if (namec<=dsta) {
      memcpy(dst,name,namec);
      if (namec<dsta) dst[namec]=0;
    } else {
      memcpy(dst,name,dsta);
    }
    return namec;
  }
  if (vid) *vid=0;
  if (pid) *pid=0;
  if (version) *version=0;
  if (dsta) dst[0]=0;
  return 0;
}

int egg_gamepad_get_button(int *btnid,int *hidusage,int *lo,int *hi,int *rest,int devid,int btnix) {
  fprintf(stderr,"TODO %s\n",__func__);
  //TODO We need inmgr to cache the entire capability report for each device. Drivers expose an iterator, but Platform API exposes an index accessor.
  // (and it is unreasonable for drivers to work on index, or wasm apps to provide a callback).
  return -1;
}

/* Audio.
 */
 
void egg_play_sound(int soundid,double trim,double pan) {
  if (!eggrt.sound_enable) return;
  synth_play_sound(eggrt.synth,soundid,trim,pan);
}

void egg_play_song(int songid,int force,int repeat) {
  eggrt.songid=songid;
  eggrt.songrepeat=repeat;
  if (!eggrt.music_enable) return;
  synth_play_song(eggrt.synth,songid,force,repeat);
}

int egg_song_get_id() {
  return synth_get_song_id(eggrt.synth);
}

double egg_song_get_playhead() {
  double p=synth_get_playhead(eggrt.synth);
  if (p<=0.0) return 0.0;
  double remaining=hostio_audio_estimate_remaining_buffer(eggrt.hostio->audio);
  if (remaining>=p) return 0.0;
  return p-remaining;
}

void egg_song_set_playhead(double playhead) {
  synth_set_playhead(eggrt.synth,playhead);
}

/* Video.
 */
 
void egg_video_get_screen_size(int *w,int *h) {
  if (w) *w=eggrt.hostio->video->w;
  if (h) *h=eggrt.hostio->video->h;
}

void egg_video_fb_from_screen(int *x,int *y) {
  render_coords_fb_from_win(eggrt.render,x,y);
}

void egg_video_screen_from_fb(int *x,int *y) {
  render_coords_win_from_fb(eggrt.render,x,y);
}

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
  render_render(eggrt.render,uniform,vtxv,vtxc);
}
