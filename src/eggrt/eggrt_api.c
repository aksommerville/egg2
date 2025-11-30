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
  fprintf(stderr,"GAME: %.*s\n",msgc,msg);
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
    case EGG_PREF_MUSIC: return eggrt.music_level;
    case EGG_PREF_SOUND: return eggrt.sound_level;
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
        eggrt_call_client_notify(k,v);
      } return 0;
  
    case EGG_PREF_MUSIC: {
        if (v<0) v=0; else if (v>99) v=99;
        if (v==eggrt.music_level) return 0;
        eggrt.music_level=v;
        eggrt_call_client_notify(k,v);
      } return 0;
  
    case EGG_PREF_SOUND: {
        if (v<0) v=0; else if (v>99) v=99;
        if (v==eggrt.sound_level) return 0;
        eggrt.sound_level=v;
        eggrt_call_client_notify(k,v);
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
  if (eggrt.umenu) return;
  eggrt.umenu=umenu_new(1);
}

void egg_input_get_all(int *statev,int statea) {
  if (!statev||(statea<1)) return;
  if (statea>INMGR_PLAYER_LIMIT) statea=INMGR_PLAYER_LIMIT; // sanity check
  switch (eggrt.input_mode) {
    case EGG_INPUT_MODE_GAMEPAD: {
        int i=statea; while (i-->0) {
          statev[i]=inmgr_get_player(i);
        }
      } break;
    case EGG_INPUT_MODE_MOUSE: {
        int i=statea; while (i-->0) {
          statev[i]=inmgr_get_player(i);
        }
        statev[0]&=(EGG_BTN_SOUTH|EGG_BTN_WEST|EGG_BTN_EAST);
      } break;
    default: {
        memset(statev,0,sizeof(int)*statea);
      }
  }
}

int egg_input_get_one(int playerid) {
  switch (eggrt.input_mode) {
    case EGG_INPUT_MODE_GAMEPAD: return inmgr_get_player(playerid);
    case EGG_INPUT_MODE_MOUSE: {
        if (playerid) return inmgr_get_player(playerid);
        return inmgr_get_player(0)&(EGG_BTN_SOUTH|EGG_BTN_WEST|EGG_BTN_EAST);
      }
    default: return 0;
  }
}

void egg_input_set_mode(int mode) {
  if (mode==eggrt.input_mode) return;
  switch (mode) {
    case EGG_INPUT_MODE_GAMEPAD: {
        eggrt.input_mode=EGG_INPUT_MODE_GAMEPAD;
        uint16_t btnid=0x4000;
        for (;btnid;btnid>>=1) inmgr_artificial_event(0,btnid,0);
      } break;
    case EGG_INPUT_MODE_MOUSE: {
        eggrt.input_mode=EGG_INPUT_MODE_MOUSE;
        uint16_t btnid=0x4000;
        for (;btnid;btnid>>=1) inmgr_artificial_event(0,btnid,0);
      } break;
  }
}

int egg_input_get_mouse(int *x,int *y) {
  switch (eggrt.input_mode) {
    case EGG_INPUT_MODE_MOUSE: {
        if (x) *x=eggrt.mousex;
        if (y) *y=eggrt.mousey;
      } return 1;
  }
  return 0;
}

/* Audio.
 */
 
void egg_play_sound(int soundid,float trim,float pan) {
  //if (!eggrt.sound_enable) return;//XXX
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_play_sound(soundid,trim,pan);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_play_song(int songid,int rid,int repeat,float trim,float pan) {
  eggrt.songid=rid;//XXX eggrt needs to track multiple songs. (or none, do we need to track at all anymore?)
  eggrt.songrepeat=repeat;
  //if (!eggrt.music_enable) return;//XXX
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_play_song(songid,rid,repeat,trim,pan);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_song_set(int songid,int chid,int prop,float v) {
  // EGG_SONG_PROP_* and SYNTH_PROP_* are the same thing.
  // But don't pass them thru blindly; any properties not defined by Egg Platform API must be discarded.
  switch (prop) {
    case EGG_SONG_PROP_PLAYHEAD:
    case EGG_SONG_PROP_TRIM:
    case EGG_SONG_PROP_PAN:
      break;
    default: return;
  }
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_set(songid,chid,prop,v);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_song_event_note_on(int songid,int chid,int noteid,int velocity) {
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_event_note_on(songid,chid,noteid,velocity);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_song_event_note_off(int songid,int chid,int noteid) {
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_event_note_off(songid,chid,noteid,0x40);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_song_event_note_once(int songid,int chid,int noteid,int velocity,int durms) {
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_event_note_once(songid,chid,noteid,velocity,durms);
  hostio_audio_unlock(eggrt.hostio);
}

void egg_song_event_wheel(int songid,int chid,int v) {
  float vf;
  if (v<=-8192) vf=-1.0f;
  else if (v>=8192) vf=1.0f;
  else vf=(float)v/8192.0f;
  if (hostio_audio_lock(eggrt.hostio)<0) return;
  synth_set(songid,chid,SYNTH_PROP_WHEEL,vf);
  hostio_audio_unlock(eggrt.hostio);
}

float egg_song_get_playhead(int songid) {
  if (hostio_audio_lock(eggrt.hostio)<0) return 0.0;
  double p=synth_get(1,0xff,SYNTH_PROP_PLAYHEAD);
  if (p<=0.0) {
    hostio_audio_unlock(eggrt.hostio);
    return 0.0;
  }
  double remaining=hostio_audio_estimate_remaining_buffer(eggrt.hostio->audio);
  hostio_audio_unlock(eggrt.hostio);
  if (remaining>=p) return 0.0;
  return p-remaining;
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
