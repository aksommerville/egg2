#include "synth_internal.h"

/* Ring buffer.
 */
 
void synth_ring_cleanup(struct synth_ring *ring) {
  if (ring->v) synth_free(ring->v);
  ring->v=0;
  ring->c=0;
  ring->p=0;
}

int synth_ring_resize(struct synth_ring *ring,int framec) {
  if (framec<SYNTH_RING_MIN) framec=SYNTH_RING_MIN;
  else if (framec>SYNTH_RING_MAX) framec=SYNTH_RING_MAX;
  void *nv=synth_calloc(sizeof(float),framec);
  if (!nv) return -1;
  if (ring->v) {
    int cpc=(ring->c<framec)?ring->c:framec;
    if (cpc>0) __builtin_memcpy(nv,ring->v,sizeof(float)*cpc);
    synth_free(ring->v);
  }
  ring->v=nv;
  ring->c=framec;
  if (ring->p>=ring->c) ring->p=0;
  return 0;
}

/* Fake trigonometric functions using our lookup table.
 */
 
#define M_PI 3.141592653589793f
 
static float synth_cos(float t) {
  t+=M_PI*0.5f;
  int p=(int)((t*SYNTH_WAVE_SIZE_SAMPLES)/(M_PI*2.0f));
  p&=SYNTH_WAVE_SIZE_SAMPLES-1;
  return synth.sine.v[p];
}

/* Single stage IIR.
 */
 
void synth_iir3_init_bpass(struct synth_iir3 *iir3,float mid,float wid) {
  /* I have only a vague idea of how this works, and the formula is taken entirely on faith.
   * Reference:
   *   Steven W Smith: The Scientist and Engineer's Guide to Digital Signal Processing
   *   Ch 19, p 326, Equation 19-7
   */
  float r=1.0f-3.0f*wid;
  float cosfreq=synth_cos(M_PI*2.0f*mid);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  iir3->cv[0]=1.0f-k;
  iir3->cv[1]=2.0f*(k-r)*cosfreq;
  iir3->cv[2]=r*r-k;
  iir3->cv[3]=2.0f*r*cosfreq;
  iir3->cv[4]=-r*r;
  
  iir3->v[0]=iir3->v[1]=iir3->v[2]=iir3->v[3]=iir3->v[4]=0.0f;
}

/* Wave.
 */
 
void synth_wave_del(struct synth_wave *wave) {
  if (!wave) return;
  if (!wave->refc) return; // immortal
  if (wave->refc-->1) return;
  synth_free(wave);
}

struct synth_wave *synth_wave_new() {
  struct synth_wave *wave=synth_calloc(1,sizeof(struct synth_wave));
  if (!wave) return 0;
  wave->refc=1;
  return wave;
}

int synth_wave_ref(struct synth_wave *wave) {
  if (!wave) return -1;
  if (!wave->refc) return 0; // immortal
  if (wave->refc<1) return -1;
  if (wave->refc==INT_MAX) return -1;
  wave->refc++;
  return 0;
}

/* Wee helpers for generating waves.
 */
 
static void synth_wave_odd_symmetry(struct synth_wave *wave) {
  const int HALFPERIOD=SYNTH_WAVE_SIZE_SAMPLES>>1;
  const float *ap=wave->v;
  float *bp=wave->v+HALFPERIOD;
  int i=HALFPERIOD;
  for (;i-->0;ap++,bp++) *bp=-*ap;
}

static void synth_wave_plateau(float *v,int c,float level) {
  for (;c-->0;v++) *v=level;
}

static void synth_wave_ramp(float *v,int c,float levela,float levelz) {
  if (c<1) return;
  float leveld=(levelz-levela)/(float)c;
  for (;c-->0;v++,levela+=leveld) *v=levela;
}

static void synth_wave_rotate(struct synth_wave *wave,int np) {
  np%=SYNTH_WAVE_SIZE_SAMPLES;
  if (np<0) np+=SYNTH_WAVE_SIZE_SAMPLES;
  if (!np) return;
  float tmp[SYNTH_WAVE_SIZE_SAMPLES];
  int tmpp=0,srcp=np;
  for (;tmpp<SYNTH_WAVE_SIZE_SAMPLES;tmpp++,srcp++) {
    if (srcp>=SYNTH_WAVE_SIZE_SAMPLES) srcp=0;
    tmp[tmpp]=wave->v[srcp];
  }
  __builtin_memcpy(wave->v,tmp,sizeof(tmp));
}

static void synth_wave_add_harmonic(struct synth_wave *dst,const struct synth_wave *src,int step,float level) {
  if (step<0) return;
  if (step>=SYNTH_WAVE_SIZE_SAMPLES) return;
  float *dstp=dst->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  int srcp=0;
  for (;i-->0;dstp++,srcp+=step) {
    if (srcp>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
    (*dstp)+=src->v[srcp]*level;
  }
}

/* Approximate sine wave.
 */
 
void synth_wave_generate_sine(struct synth_wave *wave) {
  const int HALFPERIOD=SYNTH_WAVE_SIZE_SAMPLES>>1;
  const int QUARTERPERIOD=SYNTH_WAVE_SIZE_SAMPLES>>2;
  const float AVERAGE=0.6366192487687196f; // The average amplitude of half of a sine wave.
  const float k=1.0f/(AVERAGE*QUARTERPERIOD);
  float *ap=wave->v;
  float *bp=wave->v+QUARTERPERIOD;
  *ap=0.0f; // sin(0)=0
  *bp=1.0f; // sin(pi/4)=1
  
  // For the first half of the wave, our two pointers are each other's derivative.
  int i=QUARTERPERIOD;
  while (i-->0) {
    ap++;
    ap[0]=ap[-1]+bp[0]*k;
    bp++;
    bp[0]=bp[-1]-ap[0]*k;
  }
  
  // And the second half of the wave is just the first upside-down.
  synth_wave_odd_symmetry(wave);
}

/* Decode one wave command.
 */

static int synth_wave_decode_sine(struct synth_wave *wave,const uint8_t *src,int srcc) {
  __builtin_memcpy(wave->v,synth.sine.v,sizeof(wave->v));
  return 0;
}

static int synth_wave_decode_square(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t smooth=src[0];
  /* We'll generate: SIGMOID-UP, PLATEAU, SIGMOID-DOWN, VALLEY.
   * Then rotate to maintain zero phase.
   */
  const int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  const int quarterlen=halflen>>1;
  const int threequarters=halflen+quarterlen; // Start of the upward sigmoid, in the sine wave.
  int curvelen=(smooth*halflen)>>8;
  if (curvelen>0) {
    int i=0;
    for (;i<curvelen;i++) {
      int srcp=(threequarters+(i*halflen)/curvelen)&(SYNTH_WAVE_SIZE_SAMPLES-1);
      wave->v[i]=synth.sine.v[srcp];
    }
  }
  synth_wave_plateau(wave->v+curvelen,halflen-curvelen,1.0f);
  synth_wave_odd_symmetry(wave);
  synth_wave_rotate(wave,curvelen>>1);
  return 1;
}

static int synth_wave_decode_saw(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t smooth=src[0];
  /* We range from downward saw (0) to triangle (0xff).
   * Final output always begins halfway up the upward leg of the triangle.
   */
  const int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  int upleglen=(smooth*halflen)>>8;
  int dstp=0;
  if (upleglen>=2) {
    int ulhalf=upleglen>>1;
    synth_wave_ramp(wave->v+dstp,ulhalf,0.0f,1.0f);
    dstp+=ulhalf;
    upleglen-=ulhalf; // if it's odd, keep the odd sample in this lower half
  }
  int downleglen=SYNTH_WAVE_SIZE_SAMPLES-upleglen-dstp;
  synth_wave_ramp(wave->v+dstp,downleglen,1.0f,-1.0f);
  dstp+=downleglen;
  if (upleglen>0) {
    synth_wave_ramp(wave->v+dstp,upleglen,-1.0f,0.0f);
  }
  return 1;
}

static int synth_wave_decode_triangle(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t smooth=src[0];
  /* We'll generate a positive trapezoid in the front half: a perfect triangle at (0x00) and a perfect square at (0xff).
   */
  const int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  const int quarterlen=SYNTH_WAVE_SIZE_SAMPLES>>2;
  int leglen=quarterlen-((smooth*quarterlen)>>8);
  synth_wave_ramp(wave->v,leglen,0.0f,1.0f);
  synth_wave_plateau(wave->v+leglen,halflen-(leglen<<1),1.0f);
  synth_wave_ramp(wave->v+halflen-leglen,leglen,1.0f,0.0f);
  synth_wave_odd_symmetry(wave);
  return 1;
}

static int synth_wave_decode_noise(struct synth_wave *wave,const uint8_t *src,int srcc) {
  // Start with pure white noise.
  synth_rand(wave->v,SYNTH_WAVE_SIZE_SAMPLES,0xaaaaaaaa);
  // Then force the first half positive and second half negative.
  float *v=wave->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES>>1;
  for (;i-->0;v++) if (*v<0.0f) *v=-*v;
  for (i=SYNTH_WAVE_SIZE_SAMPLES>>1;i-->0;v++) if (*v>0.0f) *v=-*v;
  return 0;
}

static int synth_wave_decode_rotate(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  uint8_t phase=src[0];
  int np=(phase*SYNTH_WAVE_SIZE_SAMPLES)>>8;
  synth_wave_rotate(wave,np);
  return 1;
}

static int synth_wave_decode_gain(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<2) return -1;
  float mlt=src[0]+src[1]/256.0f;
  float *v=wave->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++) (*v)*=mlt;
  return 2;
}

static int synth_wave_decode_clip(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  float plimit=src[0]/255.0f;
  float nlimit=-plimit;
  float *v=wave->v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++) {
    if (*v<nlimit) *v=nlimit;
    else if (*v>plimit) *v=plimit;
  }
  return 1;
}

static int synth_wave_decode_norm(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  float limit=src[0]/255.0f;
  float *v=wave->v;
  float lo=v[0],hi=v[0];
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;v++) {
    if (*v<lo) lo=*v;
    else if (*v>hi) hi=*v;
  }
  if (lo<0.0f) lo=-lo;
  if (hi<0.0f) hi=-hi; // huh?
  if (lo>hi) hi=lo;
  if (hi<=0.0f) return 1; // important to get out if it's zero, norming a silent buffer
  float mlt=limit/hi;
  for (i=SYNTH_WAVE_SIZE_SAMPLES,v=wave->v;i-->0;v++) {
    (*v)*=mlt;
  }
  return 1;
}

static int synth_wave_decode_harmonics(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  int coefc=src[0];
  if (coefc>(srcc-1)>>1) return -1;
  struct synth_wave tmp={0};
  src+=1;
  int i=coefc,step=1;
  for (;i-->0;src+=2,step++) {
    int leveli=(src[0]<<8)|src[1];
    if (!leveli) continue;
    float levelf=leveli/65535.0f;
    synth_wave_add_harmonic(&tmp,wave,step,levelf);
  }
  __builtin_memcpy(wave->v,tmp.v,sizeof(tmp.v));
  return 1+(coefc<<1);
}

static int synth_wave_decode_harmfm(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  int rate=src[0]>>4;
  int range=src[0]&15;
  float frange=(float)range;
  struct synth_wave tmp={0};
  int modp=0; // position in (synth.sine.v)
  float carp=0.0f; // position in (wave.v). Round when reading.
  float *dstv=tmp.v;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;dstv++,modp+=rate) {
    if (modp>=SYNTH_WAVE_SIZE_SAMPLES) modp-=SYNTH_WAVE_SIZE_SAMPLES;
    float mod=synth.sine.v[modp]*frange;
    carp+=1.0f+mod;
    int icarp=((int)carp)&(SYNTH_WAVE_SIZE_SAMPLES-1);
    *dstv=wave->v[icarp];
  }
  __builtin_memcpy(wave->v,tmp.v,sizeof(tmp.v));
  return 1;
}

static int synth_wave_decode_mavg(struct synth_wave *wave,const uint8_t *src,int srcc) {
  if (srcc<1) return -1;
  int len=src[0];
  if (!len) return 1;
  // Prepopulate buffer with the wave's tail.
  float buf[255];
  __builtin_memcpy(buf,wave->v+SYNTH_WAVE_SIZE_SAMPLES-len,sizeof(float)*len);
  // Calculate the sum of that.
  float sum=0.0f;
  float *v=buf;
  int i=len;
  for (;i-->0;v++) sum+=*v;
  // And run across the whole wave.
  int bufp=0;
  float mlt=1.0f/(float)(len+1);
  for (i=SYNTH_WAVE_SIZE_SAMPLES,v=wave->v;i-->0;v++) {
    float pv=buf[bufp];
    sum-=pv;
    sum+=*v;
    buf[bufp++]=*v;
    if (bufp>=len) bufp=0;
    *v=sum*mlt;
  }
  return 1;
}

/* Decode wave.
 */
 
int synth_wave_decode(struct synth_wave *wave,const void *src,int srcc) {
  const uint8_t *SRC=src;
  if (!srcc||((srcc==1)&&!SRC[0])) { // In general EOF is required. There's a special case for empty or EOF-only.
    __builtin_memcpy(wave->v,synth.sine.v,sizeof(wave->v));
    return srcc;
  }
  int srcp=0;
  for (;;) {
    if (srcp>=srcc) return -1; // EOF is required.
    uint8_t cmd=SRC[srcp++];
    if (!cmd) break; // EOF.
    int err=-1;
    switch (cmd) {
      case 0x01: err=synth_wave_decode_sine(wave,src+srcp,srcc-srcp); break;
      case 0x02: err=synth_wave_decode_square(wave,src+srcp,srcc-srcp); break;
      case 0x03: err=synth_wave_decode_saw(wave,src+srcp,srcc-srcp); break;
      case 0x04: err=synth_wave_decode_triangle(wave,src+srcp,srcc-srcp); break;
      case 0x05: err=synth_wave_decode_noise(wave,src+srcp,srcc-srcp); break;
      case 0x06: err=synth_wave_decode_rotate(wave,src+srcp,srcc-srcp); break;
      case 0x07: err=synth_wave_decode_gain(wave,src+srcp,srcc-srcp); break;
      case 0x08: err=synth_wave_decode_clip(wave,src+srcp,srcc-srcp); break;
      case 0x09: err=synth_wave_decode_norm(wave,src+srcp,srcc-srcp); break;
      case 0x0a: err=synth_wave_decode_harmonics(wave,src+srcp,srcc-srcp); break;
      case 0x0b: err=synth_wave_decode_harmfm(wave,src+srcp,srcc-srcp); break;
      case 0x0c: err=synth_wave_decode_mavg(wave,src+srcp,srcc-srcp); break;
    }
    if (err<0) return -1;
    srcp+=err;
  }
  return srcp;
}

/* PCM dump.
 */
 
void synth_pcm_del(struct synth_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  synth_free(pcm);
}

struct synth_pcm *synth_pcm_new(int c) {
  if (c<0) return 0;
  if (c>(INT_MAX-sizeof(struct synth_pcm))/sizeof(float)) return 0;
  struct synth_pcm *pcm=synth_calloc(1,sizeof(struct synth_pcm)+c*sizeof(float));
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}

int synth_pcm_ref(struct synth_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc>=INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

/* Player cleanup.
 */
 
void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay) {
}

/* Player init.
 */
 
int synth_pcmplay_init(struct synth_pcmplay *pcmplay,struct synth_pcm *pcm,float trim,float pan) {
  if (!pcm) return -1;
  pcmplay->pcm=pcm;
  pcmplay->p=0;
  if (trim<0.0f) trim=0.0f; else if (trim>1.0f) trim=1.0f;
  if (pan<=-1.0f) {
    pcmplay->triml=trim;
    pcmplay->trimr=0.0f;
  } else if (pan>=1.0f) {
    pcmplay->triml=0.0f;
    pcmplay->trimr=trim;
  } else if (pan<0.0f) {
    pcmplay->triml=trim;
    pcmplay->trimr=(pan+1.0f)*trim;
  } else if (pan>0.0f) {
    pcmplay->triml=(1.0f-pan)*trim;
    pcmplay->trimr=trim;
  } else {
    pcmplay->triml=trim;
    pcmplay->trimr=trim;
  }
  return 0;
}

/* Update player.
 */
 
int synth_pcmplay_update(float *dstl,float *dstr,struct synth_pcmplay *pcmplay,int framec) {
  int available=pcmplay->pcm->c-pcmplay->p;
  if (available<1) return 0;
  if (framec>available) framec=available;
  int i=framec;
  const float *src=pcmplay->pcm->v+pcmplay->p;
  if (dstr) {
    for (;i-->0;dstl++,dstr++,src++) {
      (*dstl)+=(*src)*pcmplay->triml;
      (*dstr)+=(*src)*pcmplay->trimr;
    }
  } else {
    for (;i-->0;dstl++,src++) {
      (*dstl)+=(*src)*pcmplay->triml;
    }
  }
  pcmplay->p+=framec;
  return 1;
}

/* Delete printer.
 */
 
void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_song_del(printer->song);
  synth_pcm_del(printer->pcm);
  synth_free(printer);
}

/* New printer.
 */
 
struct synth_printer *synth_printer_new(const void *src,int srcc) {
  struct synth_song *song=synth_song_new(1,src,srcc,1.0f,0.0f);
  if (!song) return 0;
  int framec=synth_song_get_duration_frames(song);
  if (framec<1) framec=1;
  struct synth_pcm *pcm=synth_pcm_new(framec);
  if (!pcm) {
    synth_song_del(song);
    return 0;
  }
  struct synth_printer *printer=synth_calloc(1,sizeof(struct synth_printer));
  if (!printer) {
    synth_song_del(song);
    synth_pcm_del(pcm);
    return 0;
  }
  printer->pcm=pcm;
  printer->song=song;
  return printer;
}

/* Update printer.
 */
 
int synth_printer_update(struct synth_printer *printer,int framec) {
  int available=printer->pcm->c-printer->p;
  if (available<1) return 0;
  if (framec>available) framec=available;
  synth_song_update(printer->pcm->v+printer->p,0,printer->song,framec);
  printer->p+=framec;
  return 1;
}
