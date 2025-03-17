#include "eggrt_internal.h"
#include <unistd.h>

struct eggrt eggrt={0};

/* Quit.
 */
 
void eggrt_quit(int status) {
  
  eggrt_call_client_quit(status);
  
  if (!status) {
    //TODO Report performance.
    fprintf(stderr,"%s:%d:TODO: Performance report\n",__FILE__,__LINE__);
  }
  
  hostio_del(eggrt.hostio);
  eggrt.hostio=0;
  
  eggrt_rom_quit();
}

/* Init drivers.
 */
 
static int eggrt_init_video() {
  struct hostio_video_setup setup={
    .title="EGG GAME",//TODO
    .iconrgba=0,///TODO
    .iconw=0,
    .iconh=0,
    .w=0,
    .h=0,
    .fullscreen=eggrt.fullscreen,
    .device=eggrt.video_device,
  };
  if (hostio_init_video(eggrt.hostio,eggrt.video_driver,&setup)<0) {
    fprintf(stderr,"%s: Failed to initialize any video driver.\n",eggrt.exename);
    return -2;
  }
  if (!eggrt.hostio->video->type->gx_begin||!eggrt.hostio->video->type->gx_end) {
    fprintf(stderr,"%s: Video driver '%s' does not appear to support OpenGL.\n",eggrt.exename,eggrt.hostio->video->type->name);
    return -2;
  }
  //TODO Renderer.
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
  //TODO Synthesizer.
  return 0;
}

static int eggrt_init_input() {
  struct hostio_input_setup setup={0};
  if (hostio_init_input(eggrt.hostio,eggrt.input_driver,&setup)<0) {
    fprintf(stderr,"%s: Error initializing input drivers.\n",eggrt.exename);
    return -2;
  }
  //TODO Input manager.
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
  //TODO clock
  
  return 0;
}

/* Update.
 */
 
int eggrt_update() {
  int err;

  //TODO Tick clock.
  usleep(200000);
  double elapsed=0.016666;
  
  // Update drivers.
  if ((err=hostio_update(eggrt.hostio))<0) {
    if (err!=-2) fprintf(stderr,"%s: Error updating platform drivers.\n",eggrt.exename);
    return -2;
  }
  
  // Update client.
  if ((err=eggrt_call_client_update(elapsed))<0) return err;
  
  // Render.
  if ((err=eggrt.hostio->video->type->gx_begin(eggrt.hostio->video))<0) return err;
  //TODO reset renderer
  if ((err=eggrt_call_client_render())<0) return err;
  if ((err=eggrt.hostio->video->type->gx_end(eggrt.hostio->video))<0) return err;
  
  return 0;
}
