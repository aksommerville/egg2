#include "eggrt_internal.h"
#include "opt/serial/serial.h"
#include "opt/image/image.h"
#include <unistd.h>

struct eggrt eggrt={0};

/* Quit.
 */
 
void eggrt_quit(int status) {
  
  eggrt_call_client_quit(status);
  
  if (!status) eggrt_clock_report();
  
  render_del(eggrt.render);
  inmgr_quit(&eggrt.inmgr);
  
  hostio_audio_play(eggrt.hostio,0);
  hostio_del(eggrt.hostio);
  eggrt.hostio=0;
  
  synth_del(eggrt.synth);
  
  eggrt_rom_quit();
  
  if (eggrt.titlestorage) {
    free(eggrt.titlestorage);
    eggrt.titlestorage=0;
  }
  if (eggrt.iconstorage) {
    free(eggrt.iconstorage);
    eggrt.iconstorage=0;
  }
}

/* Fill in (title,icon*,fbw,fbh) per ROM.
 * Fails if we don't end up with valid (fbw,fbh).
 * Title and icon are purely optional.
 * Also, since we're in there anyway, capture the metadata's player count, language, and feature flags.
 */
 
static int eggrt_eval_fb(int *w,int *h,const char *src,int srcc) {
  *w=*h=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*w)*=10;
    (*w)+=src[srcp++]-'0';
  }
  if ((srcp>=srcc)||(src[srcp++]!='x')) return -1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*h)*=10;
    (*h)+=src[srcp++]-'0';
  }
  if (srcp<srcc) return *w=*h=-1;
  return 0;
}

static int eggrt_eval_players(int *lo,int *hi,const char *src,int srcc) {
  *lo=*hi=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*lo)*=10;
    (*lo)+=src[srcp++]-'0';
  }
  if (srcp>=srcc) {
    *hi=*lo;
    return 0;
  }
  if ((srcp>srcc-2)||memcmp(src+srcp,"..",2)) return -1;
  srcp+=2;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*hi)*=10;
    (*hi)+=src[srcp++]-'0';
  }
  if (srcp<srcc) return *lo=*hi=-1;
  if (!*hi) *hi=8;
  return 0;
}
 
static int eggrt_populate_video_setup(struct hostio_video_setup *setup) {
  setup->fbw=640; // Default framebuffer size, per our spec.
  setup->fbh=360;
  const char *titlesrc=0;
  int titlesrcc=0;
  int iconimageid=0;
  
  int p=eggrt_rom_search(EGG_TID_metadata,1);
  if (p>=0) {
    struct metadata_reader reader;
    if (metadata_reader_init(&reader,eggrt.resv[p].v,eggrt.resv[p].c)>=0) {
      struct metadata_entry entry;
      while (metadata_reader_next(&entry,&reader)>0) {
      
        if ((entry.kc==2)&&!memcmp(entry.k,"fb",2)) {
          eggrt_eval_fb(&setup->fbw,&setup->fbh,entry.v,entry.vc);
          
        } else if ((entry.kc==5)&&!memcmp(entry.k,"title",5)) {
          titlesrc=entry.v;
          titlesrcc=entry.vc;
          
        } else if ((entry.kc==6)&&!memcmp(entry.k,"title$",6)) {
          sr_int_eval(&eggrt.titlestrix,entry.v,entry.vc);
          
        } else if ((entry.kc==9)&&!memcmp(entry.k,"iconImage",9)) {
          sr_int_eval(&iconimageid,entry.v,entry.vc);
          
        } else if ((entry.kc==7)&&!memcmp(entry.k,"players",7)) {
          eggrt_eval_players(&eggrt.inmgr.playerclo,&eggrt.inmgr.playerchi,entry.v,entry.vc);
          
        } else if ((entry.kc==4)&&!memcmp(entry.k,"lang",4)) {
          eggrt.romlang=entry.v;
          eggrt.romlangc=entry.vc;
          
        } else if ((entry.kc==8)&&!memcmp(entry.k,"required",8)) { //TODO We're dutifully recording these... Not sure when we ought to validate.
          eggrt.romrequired=entry.v;
          eggrt.romrequiredc=entry.vc;
          
        } else if ((entry.kc==8)&&!memcmp(entry.k,"optional",8)) {
          eggrt.romoptional=entry.v;
          eggrt.romoptionalc=entry.vc;
      
        }
      }
    }
  }
  
  /* We can only do the default title from here, since we haven't picked a language yet.
   */
  if (titlesrcc) {
    if (eggrt.titlestorage=malloc(titlesrcc+1)) {
      memcpy(eggrt.titlestorage,titlesrc,titlesrcc);
      ((char*)eggrt.titlestorage)[titlesrcc]=0;
      setup->title=eggrt.titlestorage;
    }
  }
  if (iconimageid>0) {
    int p=eggrt_rom_search(EGG_TID_image,iconimageid);
    if (p>=0) {
      const struct rom_entry *res=eggrt.resv+p;
      int w=0,h=0;
      if (image_measure(&w,&h,res->v,res->c)>=0) {
        int len=w*h*4;
        if (eggrt.iconstorage=malloc(len)) {
          if (image_decode(eggrt.iconstorage,len,res->v,res->c)==len) {
            setup->iconrgba=eggrt.iconstorage;
            setup->iconw=w;
            setup->iconh=h;
          }
        }
      }
    }
  }

  if (
    (setup->fbw<1)||(setup->fbw>EGG_TEXTURE_SIZE_LIMIT)||
    (setup->fbh<1)||(setup->fbh>EGG_TEXTURE_SIZE_LIMIT)
  ) {
    fprintf(stderr,"%s: Invalid framebuffer dimensions %dx%d.\n",eggrt.exename,setup->fbw,setup->fbh);
    return -2;
  }
  return 0;
}

/* Init drivers.
 */
 
static int eggrt_init_video() {
  int err;
  struct hostio_video_setup setup={
    .fullscreen=eggrt.fullscreen,
    .device=eggrt.video_device,
  };
  if ((err=eggrt_populate_video_setup(&setup))<0) return err;
  if (hostio_init_video(eggrt.hostio,eggrt.video_driver,&setup)<0) {
    fprintf(stderr,"%s: Failed to initialize any video driver.\n",eggrt.exename);
    return -2;
  }
  if (!eggrt.hostio->video->type->gx_begin||!eggrt.hostio->video->type->gx_end) {
    fprintf(stderr,"%s: Video driver '%s' does not appear to support OpenGL.\n",eggrt.exename,eggrt.hostio->video->type->name);
    return -2;
  }
  if (!(eggrt.render=render_new())) return -1;
  render_set_size(eggrt.render,eggrt.hostio->video->w,eggrt.hostio->video->h);
  if (render_set_framebuffer_size(eggrt.render,setup.fbw,setup.fbh)<0) return -1;
  return 0;
}

static int eggrt_init_audio() {
  struct hostio_audio_setup setup={
    .rate=eggrt.audio_rate,
    .chanc=eggrt.audio_chanc,
    .buffer_size=eggrt.audio_buffer,
    .device=eggrt.audio_device,
  };
  if (hostio_init_audio(eggrt.hostio,eggrt.audio_driver,&setup)<0) {
    fprintf(stderr,"%s: Failed to initialize any audio driver.\n",eggrt.exename);
    return -2;
  }
  if (!(eggrt.synth=synth_new(eggrt.hostio->audio->rate,eggrt.hostio->audio->chanc))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer. rate=%d chanc=%d\n",eggrt.exename,eggrt.hostio->audio->rate,eggrt.hostio->audio->chanc);
    return -2;
  }
  return 0;
}

static int eggrt_init_input() {
  struct hostio_input_setup setup={0};
  if (hostio_init_input(eggrt.hostio,eggrt.input_driver,&setup)<0) {
    fprintf(stderr,"%s: Error initializing input drivers.\n",eggrt.exename);
    return -2;
  }
  int err=inmgr_init(&eggrt.inmgr);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Error initializing input manager.\n",eggrt.exename);
    return -2;
  }
  return 0;
}
 
static int eggrt_init_drivers() {
  int err;
  struct hostio_video_delegate vdelegate={
    .cb_close=eggrt_cb_close,
    .cb_focus=eggrt_cb_focus,
    .cb_resize=eggrt_cb_resize,
    .cb_key=eggrt_cb_key,
    .cb_text=eggrt_cb_text,
    .cb_mmotion=eggrt_cb_mmotion,
    .cb_mbutton=eggrt_cb_mbutton,
    .cb_mwheel=eggrt_cb_mwheel,
  };
  struct hostio_audio_delegate adelegate={
    .cb_pcm_out=eggrt_cb_pcm_out,
  };
  struct hostio_input_delegate idelegate={
    .cb_connect=eggrt_cb_connect,
    .cb_disconnect=eggrt_cb_disconnect,
    .cb_button=eggrt_cb_button,
  };
  if (!(eggrt.hostio=hostio_new(&vdelegate,&adelegate,&idelegate))) return -1;
  // Video must initialize before input.
  if ((err=eggrt_init_video())<0) return err;
  if ((err=eggrt_init_input())<0) return err;
  if ((err=eggrt_init_audio())<0) return err;
  hostio_log_driver_names(eggrt.hostio);
  return 0;
}

/* Init.
 */
 
int eggrt_init() {
  int err;
  
  // ROM must initialize before drivers, drivers before prefs, and prefs before client.
  if ((err=eggrt_rom_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to acquire game ROM.\n",eggrt.exename);
    return -2;
  }
  
  // Initialize drivers.
  if ((err=eggrt_init_drivers())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing platform drivers.\n",eggrt.exename);
    return -2;
  }
  
  // Initial preferences.
  if ((err=eggrt_prefs_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error initializing preferences.\n",eggrt.exename);
    return -2;
  }
  
  // Initialize client.
  if ((err=eggrt_call_client_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Error %d from egg_client_init.\n",eggrt.exename,err);
    return -2;
  }
  
  // Start clock and audio.
  hostio_audio_play(eggrt.hostio,1);
  eggrt.clockmode=EGGRT_CLOCKMODE_NORMAL;
  eggrt_clock_init();
  
  return 0;
}

/* Update.
 */
 
int eggrt_update() {
  int err;

  // Tick clock.
  double elapsed=eggrt_clock_update();
  
  // Update drivers.
  if ((err=hostio_update(eggrt.hostio))<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating platform drivers.\n",eggrt.exename);
    return -2;
  }
  if ((err=inmgr_update(&eggrt.inmgr))<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating input manager.\n",eggrt.exename);
    return -2;
  }
  
  // Update client.
  if ((err=eggrt_call_client_update(elapsed))<0) return err;
  
  // Render.
  if ((err=eggrt.hostio->video->type->gx_begin(eggrt.hostio->video))<0) return err;
  render_begin(eggrt.render);
  if ((err=eggrt_call_client_render())<0) return err;
  render_commit(eggrt.render);
  if ((err=eggrt.hostio->video->type->gx_end(eggrt.hostio->video))<0) return err;
  
  return 0;
}
