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
  int force_rate,force_chanc; // If we are playing a WAV file, the driver must match its format. No resampling, or even channel swizzling.
  int repeat;
  const char *srcpath;
  void *src;
  int srcc;
  float playhead;
  float *bufferv[CHANC_LIMIT];
  char *pcmsrc; // s16le, interleaved (force_chanc) channels.
  int pcmsrcc; // In bytes.
  int pcmsrcp; // Also in bytes.
  int pcmfinished;
  int framec;
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
 
// Normal callback, using synth.
static void play_cb_pcm_out(int16_t *v,int c,struct hostio_audio *driver) {
  int framec=c/driver->chanc;
  play.framec+=framec;
  while (framec>0) {
    int updc=framec;
    if (updc>BUFFER_FRAMES) updc=BUFFER_FRAMES;
    synth_update(updc);
    play_pack_output(v,updc);
    v+=updc*driver->chanc;
    framec-=updc;
  }
}

// Raw PCM mode.
static void play_cb_pcm_out_raw(int16_t *v,int c,struct hostio_audio *driver) {
  play.framec+=c/driver->chanc;
  // Phrase it all in bytes.
  char *dst=(char*)v;
  int dstc=c<<1;
  while (dstc>0) {
    if (play.pcmsrcp>=play.pcmsrcc) {
      if (play.repeat) {
        play.pcmsrcp=0;
      } else {
        play.pcmfinished=1;
        break;
      }
    }
    int cpc=play.pcmsrcc-play.pcmsrcp; // bytes
    if (cpc>dstc) cpc=dstc;
    memcpy(dst,play.pcmsrc+play.pcmsrcp,cpc);
    play.pcmsrcp+=cpc;
    dst+=cpc;
    dstc-=cpc;
  }
  if (dstc>0) memset(dst,0,dstc);
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

/* Read a WAV file and prepare to play it.
 */
 
static int play_acquire_wav() {
  const uint8_t *SRC=play.src;
  if ((play.srcc<12)||memcmp(SRC,"RIFF",4)||memcmp(SRC+8,"WAVE",4)) {
    fprintf(stderr,"%s: Not a WAV file.\n",play.srcpath);
    return -2;
  }
  int srcp=12;
  while (srcp<play.srcc) {
    if (srcp>play.srcc-8) {
      fprintf(stderr,"%s: Malformed WAV file.\n",play.srcpath);
      return -2;
    }
    const uint8_t *chunkid=SRC+srcp; srcp+=4;
    int chunklen=SRC[srcp]|(SRC[srcp+1]<<8)|(SRC[srcp+2]<<16)|(SRC[srcp+3]<<24); srcp+=4;
    if ((chunklen<0)||(srcp>play.srcc-chunklen)) {
      fprintf(stderr,"%s: Malformed WAV file.\n",play.srcpath);
      return -2;
    }
    const uint8_t *chunk=SRC+srcp;
    srcp+=chunklen;
    
    if (!memcmp(chunkid,"fmt ",4)) {
      if (play.force_rate) {
        fprintf(stderr,"%s: Multiple 'fmt ' chunks in WAV file.\n",play.srcpath);
        return -2;
      }
      if (chunklen<16) {
        fprintf(stderr,"%s: Malformed WAV file, fmt len %d.\n",play.srcpath,chunklen);
        return -2;
      }
      int format=chunk[0]|(chunk[1]<<8);
      if (format!=1) {
        fprintf(stderr,"%s: WAV format must be 1 (LPCM), found %d.\n",play.srcpath,format);
        return -2;
      }
      play.force_chanc=chunk[2]|(chunk[3]<<8);
      if ((play.force_chanc<1)||(play.force_chanc>16)) {
        fprintf(stderr,"%s: Impossible WAV channel count %d.\n",play.srcpath,play.force_chanc);
        return -2;
      }
      play.force_rate=chunk[4]|(chunk[5]<<8)|(chunk[6]<<16)|(chunk[7]<<24);
      if ((play.force_rate<200)||(play.force_rate>200000)) {
        fprintf(stderr,"%s: Improbable WAV sample rate %d hz.\n",play.srcpath,play.force_rate);
        return -2;
      }
      int samplesize=chunk[14]|(chunk[15]<<8);
      if (samplesize!=16) {
        fprintf(stderr,"%s: Unsupported WAV sample size %d, only 16 is allowed.\n",play.srcpath,samplesize);
        return -2;
      }
      
    } else if (!memcmp(chunkid,"data",4)) {
      if (play.pcmsrcc>INT_MAX-chunklen) {
        fprintf(stderr,"%s: Impossibly long WAV file.\n",play.srcpath);
        return -2;
      }
      if (!play.force_chanc) {
        fprintf(stderr,"%s: Malformed WAV file, 'data' before 'fmt '.\n",play.srcpath);
        return -2;
      }
      int framelen=play.force_chanc*2;
      if (chunklen%framelen) {
        fprintf(stderr,"%s: WAV data chunk length not a multiple of frame size.\n",play.srcpath);
        return -2;
      }
      int na=play.pcmsrcc+chunklen;
      void *nv=realloc(play.pcmsrc,na);
      if (!nv) return -1;
      play.pcmsrc=nv;
      memcpy(play.pcmsrc+play.pcmsrcc,chunk,chunklen);
      play.pcmsrcc+=chunklen;
    }
  }
  if (!play.pcmsrcc) {
    fprintf(stderr,"%s: No data in WAV file.\n",play.srcpath);
    return -2;
  }
  return 0;
}

/* Acquire input file.
 */

static int play_acquire_serial() {

  // Get the raw input.
  if ((play.srcc=eggstra_single_input(&play.src))<0) return play.srcc;
  if (eggstra.srcpathc>=1) play.srcpath=eggstra.srcpathv[0];
  else play.srcpath="<stdin>";
  
  // Convert to EAU.
  int format=play_guess_format(play.src,play.srcc,play.srcpath);
  switch (format) {
    case FORMAT_EAU: break; // Cool.
    
    case FORMAT_MIDI: { // Convert.
        struct sr_encoder eau={0};
        struct sr_convert_context ctx={
          .dst=&eau,
          .src=play.src,
          .srcc=play.srcc,
          .refname=play.srcpath,
        };
        eau_get_chdr=eggstra_get_chdr;
        int err=eau_cvt_eau_midi(&ctx);
        if (err<0) {
          if (err!=-2) fprintf(stderr,"%s: Unspecified error converting MIDI to EAU.\n",play.srcpath);
          sr_encoder_cleanup(&eau);
          return -2;
        }
        free(play.src);
        play.src=eau.v; // YOINK
        play.srcc=eau.c;
      } break;
    
    /* It's not really our job, in a world where `aplay` exists, but if they ask us to play a WAV file, let's do it.
     */
    case FORMAT_WAV: return play_acquire_wav();
      
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
    .device=eggstra_opt("device",6),
    .buffer_size=eggstra_opti("buffer",6,0),
  };
  if (play.pcmsrc) {
    setup.rate=play.force_rate;
    setup.chanc=play.force_chanc;
    fprintf(stderr,"%s: Using rate=%d and chanc=%d per WAV file.\n",play.srcpath,setup.rate,setup.chanc);
  } else {
    setup.rate=eggstra_opti("rate",4,44100);
    setup.chanc=eggstra_opti("chanc",5,2);
    if (setup.chanc<1) setup.chanc=1;
    else if (setup.chanc>CHANC_LIMIT) setup.chanc=CHANC_LIMIT;
  }
  
  struct hostio_audio_delegate delegate={
    .cb_pcm_out=play_cb_pcm_out,
  };
  if (play.pcmsrc) {
    delegate.cb_pcm_out=play_cb_pcm_out_raw;
  }
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
  if (!play.pcmsrc) { // Playing raw PCM, no synth necessary.
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
  double start_real=eggstra_now_real();
  double start_cpu=eggstra_now_cpu();
  while (!eggstra.sigc) {
    usleep(100000);
    if (play.pcmsrc) {
      if (play.pcmfinished) break;
    } else {
      if (synth_get(1,0xff,SYNTH_PROP_EXISTENCE)<1.0f) break; // non-repeat playback finished.
      float nph=synth_get(1,0xff,SYNTH_PROP_PLAYHEAD);
      if (nph<play.playhead) fprintf(stderr,"%s: Looped. Approximate duration %f s.\n",play.srcpath,play.playhead);
      play.playhead=nph;
    }
  }
  if (play.framec>0) {
    double elapsed_real=eggstra_now_real()-start_real;
    double elapsed_cpu=eggstra_now_cpu()-start_cpu;
    double expect_real=(double)play.framec/(double)play.driver->rate;
    double cpuload=elapsed_cpu/elapsed_real;
    fprintf(stderr,"%s: %d frames (%.03fs) played in %.03fs, CPU consumption %.06f.\n",play.srcpath,play.framec,expect_real,elapsed_real,cpuload);
  }
  play_quit();
  return err;
}
