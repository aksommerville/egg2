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
  inmgr_quit();
  eggrt_store_quit();
  
  hostio_audio_play(eggrt.hostio,0);
  hostio_del(eggrt.hostio);
  eggrt.hostio=0;
  
  synth_del(eggrt.synth);
  
  eggrt_rom_quit();
  
  if (eggrt.titlestorage) free(eggrt.titlestorage);
  if (eggrt.iconstorage) free(eggrt.iconstorage);
  if (eggrt.video_driver) free(eggrt.video_driver);
  if (eggrt.video_device) free(eggrt.video_device);
  if (eggrt.audio_driver) free(eggrt.audio_driver);
  if (eggrt.audio_device) free(eggrt.audio_device);
  if (eggrt.input_driver) free(eggrt.input_driver);
  if (eggrt.store_req) free(eggrt.store_req);
  memset(&eggrt,0,sizeof(eggrt));
}

/* React to a change of language.
 * - Window title.
 */
 
void eggrt_language_changed() {
  /**
  char name[2];
  EGG_STRING_FROM_LANG(name,eggrt.lang)
  fprintf(stderr,"%s: %.2s\n",__func__,name);
  /**/
  if ((eggrt.metadata.title_strix<1)||(eggrt.metadata.title_strix>1024)) return;
  if (!eggrt.hostio->video||!eggrt.hostio->video->type->set_title) return;
  int resp=eggrt_rom_search(EGG_TID_strings,(eggrt.lang<<6)|1);
  if (resp<0) return;
  struct strings_reader reader;
  if (strings_reader_init(&reader,eggrt.resv[resp].v,eggrt.resv[resp].c)<0) return;
  struct strings_entry entry;
  while (strings_reader_next(&entry,&reader)>0) {
    if (entry.index==eggrt.metadata.title_strix) {
      char *v=malloc(entry.c+1);
      if (!v) return;
      memcpy(v,entry.v,entry.c);
      v[entry.c]=0;
      eggrt.hostio->video->type->set_title(eggrt.hostio->video,v);
      free(v);
      return;
    }
  }
}

/* Fill in (title,icon*,fbw,fbh) per ROM.
 * Fails if we don't end up with valid (fbw,fbh).
 * Title and icon are purely optional.
 */
 
static int eggrt_populate_video_setup(struct hostio_video_setup *setup) {
  setup->fbw=eggrt.metadata.fbw;
  setup->fbh=eggrt.metadata.fbh;
  
  /* We can only do the default title from here, since we haven't picked a language yet.
   */
  if (eggrt.metadata.titlec) {
    if (eggrt.titlestorage=malloc(eggrt.metadata.titlec+1)) {
      memcpy(eggrt.titlestorage,eggrt.metadata.title,eggrt.metadata.titlec);
      ((char*)eggrt.titlestorage)[eggrt.metadata.titlec]=0;
      setup->title=eggrt.titlestorage;
    }
  }
  if (eggrt.metadata.icon_imageid>0) {
    int p=eggrt_rom_search(EGG_TID_image,eggrt.metadata.icon_imageid);
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

/* Deliver songs and sounds to the new synthesizer.
 */
 
static int eggrt_load_synth_resources() {
  const struct rom_entry *res=eggrt.resv;
  int i=eggrt.resc,err;
  for (;i-->0;res++) {
    if (res->tid==EGG_TID_song) {
      if ((err=synth_install_song(eggrt.synth,res->rid,res->v,res->c))<0) return err;
    } else if (res->tid==EGG_TID_sound) {
      if ((err=synth_install_sound(eggrt.synth,res->rid,res->v,res->c))<0) return err;
    }
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
  int err=eggrt_load_synth_resources();
  if (err<0) return err;
  return 0;
}

static int eggrt_init_input() {
  struct hostio_input_setup setup={0};
  if (hostio_init_input(eggrt.hostio,eggrt.input_driver,&setup)<0) {
    fprintf(stderr,"%s: Error initializing input drivers.\n",eggrt.exename);
    return -2;
  }
  int err=inmgr_init();
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Error initializing input manager.\n",eggrt.exename);
    return -2;
  }
  inmgr_set_signal(INMGR_BTN_QUIT,eggrt_cb_quit);
  if (eggrt.hostio->video&&eggrt.hostio->video->type->provides_input) {
    eggrt.devid_keyboard=hostio_input_devid_next();
    inmgr_connect_keyboard(eggrt.devid_keyboard);
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
  
  eggrt.focus=1;
  
  // ROM must initialize before drivers, drivers before prefs, and prefs before client.
  if ((err=eggrt_rom_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to acquire game ROM.\n",eggrt.exename);
    return -2;
  }
  
  // Store must be after ROM, otherwise anywhere is good.
  if ((err=eggrt_store_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to initialize persistence.\n",eggrt.exename);
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
  if (eggrt.terminate) return 0;
  
  // If we're hard-paused, get out.
  if (!eggrt.focus) return 0;
  
  // Update client.
  if ((err=eggrt_call_client_update(elapsed))<0) return err;
  if ((err=eggrt_store_update())<0) return err;
  if (eggrt.terminate) return 0;
  
  // Render.
  if ((err=eggrt.hostio->video->type->gx_begin(eggrt.hostio->video))<0) return err;
  render_begin(eggrt.render);
  if ((err=eggrt_call_client_render())<0) return err;
  render_commit(eggrt.render);
  if ((err=eggrt.hostio->video->type->gx_end(eggrt.hostio->video))<0) return err;
  
  return 0;
}
