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
  
  umenu_del(eggrt.umenu);
  render_del(eggrt.render);
  inmgr_quit();
  eggrt_store_quit();
  
  hostio_audio_play(eggrt.hostio,0);
  hostio_del(eggrt.hostio);
  eggrt.hostio=0;
  
  synth_quit();
  
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
 
static int eggrt_slice_rom(void *dstpp) {
  // Alas we really can't use rom_reader for this. We need to definitely know positions in the encoded ROM.
  if (eggrt.rom&&(eggrt.romc>=4)&&!memcmp(eggrt.rom,"\0ERM",4)) {
    const uint8_t *src=eggrt.rom;
    int srcc=eggrt.romc,srcp=4,tid=1;
    int startp=0,stopp=srcc;
    while (srcp<srcc) {
      int cmdp=srcp;
      uint8_t lead=src[srcp++];
      if (!lead) break;
      switch (lead&0xc0) {
        case 0x00: { // TID
            int next_tid=tid+lead;
            if (!startp) {
              if (next_tid==5) {
                startp=srcp; // Start reading right after the advancement to tid 5.
              } else if (next_tid==6) {
                srcp=srcc; // No songs. We'd have to generate a new rom starting with a (tid+1). Not worth the trouble.
              }
            } else if (next_tid>7) {
              stopp=cmdp;
              srcp=srcc;
            }
            tid=next_tid;
          } break;
        case 0x40: srcp++; break; // RID (don't care)
        case 0x80: { // RES
            if (srcp>srcc-2) { srcp=srcc; break; }
            int len=(lead&0x3f)<<16;
            len|=src[srcp++]<<8;
            len|=src[srcp++];
            len++;
            srcp+=len;
          } break;
        case 0xc0: startp=0; srcp=srcc; break; // Illegal, default.
      }
    }
    if (startp) { // Got something.
      *(const void**)dstpp=src+startp;
      return stopp-startp;
    }
    return 0; // Invalid or no songs in a valid rom, return empty.
  }
  // Anything goes wrong, hand them all of whatever we have.
  *(const void**)dstpp=eggrt.rom;
  return eggrt.romc;
}
 
static int eggrt_load_synth_resources() {
  const void *src=0;
  int srcc=eggrt_slice_rom(&src);
  void *dst=synth_get_rom(srcc);
  if (!dst) return -1;
  memcpy(dst,src,srcc);
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
  render_set_scale(eggrt.render,eggrt.hostio->video->scale);
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
  if (eggrt.audio_buffer<=0) eggrt.audio_buffer=1024;
  if (synth_init(eggrt.hostio->audio->rate,eggrt.hostio->audio->chanc,eggrt.audio_buffer)<0) {
    fprintf(stderr,"%s: Failed to initialize synthesizer. rate=%d chanc=%d\n",eggrt.exename,eggrt.hostio->audio->rate,eggrt.hostio->audio->chanc);
    return -2;
  }
  int err=eggrt_load_synth_resources();
  if (err<0) return err;
  return 0;
}

static int eggrt_init_input() {
  eggrt.mousex=eggrt.metadata.fbw>>1;
  eggrt.mousey=eggrt.metadata.fbh>>1;
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
  inmgr_set_signal(INMGR_BTN_MENU,eggrt_cb_quit);
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
    .cb_mmotion=eggrt_cb_mmotion,
    .cb_mbutton=eggrt_cb_mbutton,
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
  eggrt.input_mode=EGG_INPUT_MODE_GAMEPAD;
  
  // ROM must initialize before drivers, drivers before prefs, and prefs before client.
  if ((err=eggrt_rom_init())<0) {
    if (err!=-2) fprintf(stderr,"%s: Malformed ROM.\n",eggrt.exename);
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
  
  // In EGG_INPUT_MODE_MOUSE, we need to move the cursor according to gamepads.
  if (eggrt.input_mode==EGG_INPUT_MODE_MOUSE) {
    int input=inmgr_get_player(0);
    if (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
      int mouse_speed=eggrt.metadata.fbw/150;
      if (mouse_speed<1) mouse_speed=1;
      switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
        case EGG_BTN_LEFT: if ((eggrt.mousex-=mouse_speed)<0) eggrt.mousex=-1; break;
        case EGG_BTN_RIGHT: if ((eggrt.mousex+=mouse_speed)>=eggrt.metadata.fbw) eggrt.mousex=eggrt.metadata.fbw; break;
      }
      switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
        case EGG_BTN_UP: if ((eggrt.mousey-=mouse_speed)<0) eggrt.mousey=-1; break;
        case EGG_BTN_DOWN: if ((eggrt.mousey+=mouse_speed)>=eggrt.metadata.fbh) eggrt.mousey=eggrt.metadata.fbh; break;
      }
    }
  }
  
  // Update client.
  if (eggrt.umenu) {
    if ((err=umenu_update(eggrt.umenu,elapsed))<0) return err;
  } else {
    if ((err=eggrt_call_client_update(elapsed))<0) return err;
  }
  if ((err=eggrt_store_update())<0) return err;
  if (eggrt.terminate) return 0;
  
  // Render.
  if ((err=eggrt.hostio->video->type->gx_begin(eggrt.hostio->video))<0) return err;
  render_begin(eggrt.render);
  if ((err=eggrt_call_client_render())<0) return err; // Render the client even when umenu open; it may show in the background.
  if (eggrt.umenu) {
    if ((err=umenu_render(eggrt.umenu))<0) return err;
  }
  render_commit(eggrt.render);
  if ((err=eggrt.hostio->video->type->gx_end(eggrt.hostio->video))<0) return err;
  
  return 0;
}
