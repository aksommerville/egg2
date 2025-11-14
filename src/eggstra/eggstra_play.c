#include "eggstra.h"
#include "opt/hostio/hostio_audio.h"
#include "opt/synth/synth.h"
#include "opt/eau/eau.h"

#define BUFFER_FRAMES 1024
#define CHANC_LIMIT 8

/* More context.
 */
 
static struct play {
  struct hostio_audio *driver;
  int repeat;
  const char *srcpath;
  void *src;
  int srcc;
  float playhead;
  float *bufferv[CHANC_LIMIT];
} play={0};

static void play_quit() {
  hostio_audio_del(play.driver);
  synth_quit();
  if (play.src) free(play.src);
  memset(&play,0,sizeof(play));
}

/* Quantize and interleave output from our buffers to the provided driver buffer.
 */
 
static void play_pack_output(int16_t *v,int framec) {
  #define Q(src) (((src)<=-1.0f)?-32768:((src)>=1.0f)?32767:(int)((src)*32767.0f))
  int chanc=play.driver->chanc;
  if (chanc==1) {
    const float *srcl=play.bufferv[0];
    for (;framec-->0;v++,srcl++) *v=Q(*srcl);
  } else {
    const float *srcl=play.bufferv[0];
    const float *srcr=play.bufferv[1];
    for (;framec-->0;v+=chanc,srcl++,srcr++) {
      v[0]=Q(*srcl);
      v[1]=Q(*srcr);
    }
  }
  #undef Q
}

/* PCM callback.
 */
 
static void play_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  int framec=c/driver->chanc;
  while (framec>0) {
    int updc=framec;
    if (updc>BUFFER_FRAMES) updc=BUFFER_FRAMES;
    synth_update(updc);
    play_pack_output(v,updc);
    v+=updc*driver->chanc;
    framec-=updc;
  }
}

/* Guess format.
 * eggdev is much better at this, but the good parts are in its primary unit so we can't share.
 */
 
#define FORMAT_UNKNOWN 0
#define FORMAT_EAU     1
#define FORMAT_MIDI    2
#define FORMAT_WAV     3
 
static int play_guess_format(const uint8_t *src,int srcc,const char *path) {
  if (!src) return FORMAT_UNKNOWN;
  
  // First, unambiguous signatures. Really, every audio format should have a signature.
  if ((srcc>=4)&&!memcmp(src,"\0EAU",4)) return FORMAT_EAU;
  if ((srcc>=8)&&!memcmp(src,"MThd\0\0\0\6",8)) return FORMAT_MIDI;
  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp(src+8,"WAVE",4)) return FORMAT_WAV;
  
  // Normalize suffix and check.
  if (path) {
    const char *sfxsrc=0;
    int sfxc=0,pathp=0;
    for (;path[pathp];pathp++) {
      if (path[pathp]=='/') {
        sfxsrc=0;
        sfxc=0;
      } else if (path[pathp]=='.') {
        sfxsrc=path+pathp+1;
        sfxc=0;
      } else if (sfxsrc) {
        sfxc++;
      }
    }
    char sfx[16];
    if ((sfxc>0)&&(sfxc<=sizeof(sfx))) {
      int i=sfxc; while (i-->0) {
        if ((sfxsrc[i]>='A')&&(sfxsrc[i]<='Z')) sfx[i]=sfxsrc[i]+0x20;
        else sfx[i]=sfxsrc[i];
      }
      switch (sfxc) {
        case 3: {
            if (!memcmp(sfx,"eau",3)) return FORMAT_EAU;
            if (!memcmp(sfx,"mid",3)) return FORMAT_MIDI;
            if (!memcmp(sfx,"wav",3)) return FORMAT_WAV;
          } break;
      }
    }
  }
  
  // Give up.
  return FORMAT_UNKNOWN;
}

/* Acquire input file.
 */

static int play_acquire_serial() {

  // Get the raw input.
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
  
  // Convert to EAU.
  int format=play_guess_format(play.src,play.srcc,play.srcpath);
  switch (format) {
    case FORMAT_EAU: break; // Cool.
    
    case FORMAT_MIDI: { // Convert.
        struct sr_encoder eau={0};
        int err=eau_cvt_eau_midi(&eau,play.src,play.srcc,play.srcpath,0,1,0);
        if (err<0) {
          if (err!=-2) fprintf(stderr,"%s: Unspecified error converting MIDI to EAU.\n",play.srcpath);
          sr_encoder_cleanup(&eau);
          return -2;
        }
        free(play.src);
        play.src=eau.v; // YOINK
        play.srcc=eau.c;
      } break;
      
    case FORMAT_WAV: // TODO We could maybe shovel these out to the driver direct. But why bother? `aplay` already exists.
    default: {
        fprintf(stderr,"%s: Unknown format for %d-byte file.\n",play.srcpath,play.srcc);
        return -2;
      }
  }

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
  if (setup.chanc<1) setup.chanc=1;
  else if (setup.chanc>CHANC_LIMIT) setup.chanc=CHANC_LIMIT;
  
  struct hostio_audio_delegate delegate={
    .cb_pcm_out=play_cb_pcm_out,
  };
  if (!(play.driver=hostio_audio_new(type,&delegate,&setup))) {
    fprintf(stderr,"%s: Failed to stand audio driver '%s', rate=%d, chanc=%d.\n",eggstra.exename,type->name,setup.rate,setup.chanc);
    return -2;
  }
  if (play.driver->chanc>CHANC_LIMIT) {
    fprintf(stderr,"%s: Asked for %d channels and %s gave us %d. We can't go above %d (arbitrarily).\n",eggstra.exename,setup.chanc,play.driver->type->name,play.driver->chanc,CHANC_LIMIT);
    return -2;
  }
  
  if (eggstra_opt("repeat",6)) play.repeat=1;

  return 0;
}

/* Install song.
 */
 
static int play_install_song(const void *src,int srcc) {
  const int pfxlen=3;
  if ((srcc<1)||(srcc>0x400000)) return -1;
  uint8_t *rom=synth_get_rom(pfxlen+srcc);
  if (!rom) return -1;
  rom[0]=0x80|((srcc-1)>>16);
  rom[1]=(srcc-1)>>8;
  rom[2]=srcc-1;
  memcpy(rom+pfxlen,src,srcc);
  return 0;
}

/* Init synthesizer.
 */
 
static int play_init_synth() {
  if (synth_init(play.driver->rate,play.driver->chanc,BUFFER_FRAMES)<0) {
    fprintf(stderr,"%s: Failed to initialize synthesizer, rate=%d, chanc=%d.\n",eggstra.exename,play.driver->rate,play.driver->chanc);
    return -2;
  }
  int i=0;
  for (;i<play.driver->chanc;i++) {
    if (!(play.bufferv[i]=synth_get_buffer(i))) {
      fprintf(stderr,"%s: Failed to acquire buffer %d/%d\n",eggstra.exename,i,play.driver->chanc);
      return -2;
    }
  }
  if (play_install_song(play.src,play.srcc)<0) return -1; // No validation; this should never fail.
  synth_play_song(1,1,play.repeat,1.0f,0.0f);
  if (synth_get(1,0xff,SYNTH_PROP_EXISTENCE)<1.0f) {
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
    if (synth_get(1,0xff,SYNTH_PROP_EXISTENCE)<1.0f) break; // non-repeat playback finished.
    float nph=synth_get(1,0xff,SYNTH_PROP_PLAYHEAD);
    if (nph<play.playhead) fprintf(stderr,"%s: Looped. Approximate duration %f s.\n",play.srcpath,play.playhead);
    play.playhead=nph;
  }
  //TODO Show CPU consumption.
  play_quit();
  return err;
}
