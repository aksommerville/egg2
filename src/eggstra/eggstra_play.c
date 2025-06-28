#include "eggstra.h"
#include "opt/hostio/hostio_audio.h"
#include "opt/synth/synth.h"
#include "opt/fs/fs.h"

/* More context.
 */
 
static struct play {
  struct hostio_audio *driver;
  struct synth *synth;
  int repeat;
  const char *srcpath;
  void *src;
  int srcc;
  float playhead;
} play={0};

static void play_quit() {
  hostio_audio_del(play.driver);
  synth_del(play.synth);
  if (play.src) free(play.src);
  memset(&play,0,sizeof(play));
}

/* PCM callback.
 */
 
static void play_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  synth_updatei(v,c,play.synth);
}

/* Acquire input file.
 */

static int play_acquire_serial() {
  if (eggstra.srcpathc!=1) {
    fprintf(stderr,"%s: 'play' requires exactly one input file, found %d.\n",eggstra.exename,eggstra.srcpathc);
    return -2;
  }
  play.srcpath=eggstra.srcpathv[0];
  if ((play.srcc=file_read(&play.src,play.srcpath))<0) {
    fprintf(stderr,"%s: Failed to read file.\n",play.srcpath);
    return -2;
  }
  //TODO Permit reading from stdin.
  //TODO Can we take WAV files too, and skip the synth?
  //TODO If it's not EAU, convert it.
  return 0;
}

/* Init driver.
 */
 
static int play_init_driver() {

  // Select type. Unlike eggrt, we'll only try one.
  const struct hostio_audio_type *type=0;
  const char *tname=eggstra_opt("driver",6);
  if (tname) {
    if (!(type=hostio_audio_type_by_name(tname,-1))) {
      fprintf(stderr,"%s: Audio driver '%s' not found.\n",eggstra.exename,tname);
      return -2;
    }
  } else {
    if (!(type=hostio_audio_type_by_index(0))) {
      fprintf(stderr,"%s: No available audio drivers.\n",eggstra.exename);
      return -2;
    }
  }
  
  struct hostio_audio_setup setup={
    .rate=eggstra_opti("rate",4,44100),
    .chanc=eggstra_opti("chanc",5,2),
    .device=eggstra_opt("device",6),
    .buffer_size=eggstra_opti("buffer",6,0),
  };
  
  struct hostio_audio_delegate delegate={
    .cb_pcm_out=play_cb_pcm_out,
  };
  if (!(play.driver=hostio_audio_new(type,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to stand audio driver '%s', rate=%d, chanc=%d.\n",eggstra.exename,type->name,setup.rate,setup.chanc);
    return -2;
  }
  
  if (eggstra_opt("repeat",6)) play.repeat=1;

  return 0;
}

/* Init synthesizer.
 */
 
static int play_init_synth() {
  if (!(play.synth=synth_new(play.driver->rate,play.driver->chanc))) {
    fprintf(stderr,"%s: Failed to initialize synthesizer, rate=%d, chanc=%d.\n",eggstra.exename,play.driver->rate,play.driver->chanc);
    return -2;
  }
  if (synth_install_song(play.synth,1,play.src,play.srcc)<0) return -1; // No validation; this should never fail.
  synth_play_song(play.synth,1,0,play.repeat);
  if (synth_get_songid(play.synth)!=1) {
    fprintf(stderr,"%s: Failed to play song. Is it well-formed EAU?\n",play.srcpath);
    return -2;
  }
  if (play.driver->type->play) {
    play.driver->type->play(play.driver,1);
  }
  return 0;
}

/* Play audio file, main entry point.
 */
 
int eggstra_main_play() {
  int err;
  if ((err=play_acquire_serial())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error acquiring input file.\n",eggstra.exename);
    return -2;
  }
  if ((err=play_init_driver())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error setting up for audio driver.\n",eggstra.exename);
    play_quit();
    return -2;
  }
  if ((err=play_init_synth())<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error setting up synthesizer.\n",eggstra.exename);
    play_quit();
    return -2;
  }
  while (!eggstra.sigc) {
    usleep(100000);
    if (!synth_get_songid(play.synth)) break; // non-repeat playback finished.
    float nph=synth_get_playhead(play.synth);
    if (nph<play.playhead) fprintf(stderr,"%s: Looped. Approximate duration %f s.\n",play.srcpath,play.playhead);
    play.playhead=nph;
  }
  //TODO Show CPU consumption.
  play_quit();
  return err;
}
