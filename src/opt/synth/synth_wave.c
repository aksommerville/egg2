#include "synth_internal.h"

/* Require the shared sine wave.
 */
 
static const float *synth_sine_require(struct synth *synth) {
  if (!synth) return 0;
  if (synth->sine) return synth->sine->v;
  if (!(synth->sine=synth_wave_new(0,"\0\0\0",3))) return 0;
  return synth->sine->v;
}

/* Object lifecycle.
 */

void synth_wave_del(struct synth_wave *wave) {
  if (!wave) return;
  if (wave->refc-->1) return;
  free(wave);
}

int synth_wave_ref(struct synth_wave *wave) {
  if (!wave) return -1;
  if (wave->refc<1) return -1;
  if (wave->refc>=INT_MAX) return -1;
  wave->refc++;
  return 0;
}

/* Pure sine.
 */
 
static void synth_wave_print_sine(float *v) {
  float t=0.0f;
  float dt=(M_PI*2.0f)/SYNTH_WAVE_SIZE_SAMPLES;
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  for (;i-->0;t+=dt,v++) *v=sinf(t);
}

/* Linear ramp.
 */
 
static void synth_wave_print_ramp(float *v,int c,float a,float z) {
  if (c<1) return;
  float d=(z-a)/(float)c;
  int i=c; for (;i-->0;v++,a+=d) *v=a;
}

/* Square with optional tapering.
 */
 
static void synth_wave_print_square(float *v,uint8_t qual,struct synth *synth) {
  const int halfperiod=SYNTH_WAVE_SIZE_SAMPLES>>1;
  if (qual&&synth_sine_require(synth)) {
    const int quarter=halfperiod>>1;
    int curvelen=(qual*quarter)>>8;
    if (curvelen<1) curvelen=1;
    uint32_t srcp=0;
    uint32_t srcdp=0x40000000/curvelen;
    int i=0;
    for (;i<curvelen;i++,srcp+=srcdp) v[i]=synth->sine->v[srcp>>SYNTH_WAVE_SHIFT];
    for (;i<quarter;i++) v[i]=1.0f;
    int from=i-1;
    for (;i<halfperiod;i++,from--) v[i]=v[from];
    for (from=0;i<SYNTH_WAVE_SIZE_SAMPLES;i++,from++) v[i]=-v[from];
  } else {
    int i;
    for (i=halfperiod;i-->0;v++) *v=1.0f;
    for (i=halfperiod;i-->0;v++) *v=-1.0f;
  }
}

/* Saw with optional tapering.
 */
 
static void synth_wave_print_saw(float *v,uint8_t qual,struct synth *synth) {
  if (qual&&synth_sine_require(synth)) {
    // Unlike the similar square, we have a mild discontinuity even at maximum qual.
    // The sine segment reaches the top and then immediately turns into a ramp.
    // TODO Confirm it sounds ok. My gut says it will be fine.
    const int halfperiod=SYNTH_WAVE_SIZE_SAMPLES>>1;
    const int quarter=halfperiod>>1;
    int curvelen=(qual*quarter)>>8;
    if (curvelen<1) curvelen=1;
    uint32_t srcp=0;
    uint32_t srcdp=0x40000000/curvelen;
    int i=0;
    for (;i<curvelen;i++,srcp+=srcdp) v[i]=synth->sine->v[srcp>>SYNTH_WAVE_SHIFT];
    synth_wave_print_ramp(v+curvelen,halfperiod-curvelen,1.0f,0.0f);
    int from=halfperiod-1;
    for (i=halfperiod;i<SYNTH_WAVE_SIZE_SAMPLES;i++,from--) v[i]=-v[from];
  } else {
    synth_wave_print_ramp(v,SYNTH_WAVE_SIZE_SAMPLES,1.0f,-1.0f);
  }
}

/* Triangle.
 * We accept a qualifier but ignore it; a triangle is always a perfect triangle.
 */
 
static void synth_wave_print_triangle(float *v,uint8_t qual,struct synth *synth) {
  const int halfperiod=SYNTH_WAVE_SIZE_SAMPLES>>1;
  synth_wave_print_ramp(v,halfperiod,-1.0f,1.0f);
  synth_wave_print_ramp(v+halfperiod,halfperiod,1.0f,-1.0f);
}

/* Fixed FM.
 */
 
static void synth_wave_print_fixedfm(float *v,uint8_t qual,const float *src) {
  int rate=qual>>4;
  float range=(float)(qual&15);//TODO scale down?
  range*=(float)(1<<SYNTH_WAVE_SHIFT);
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  uint32_t carp=0,modp=0;
  uint32_t moddp=rate<<SYNTH_WAVE_SHIFT;
  for (;i-->0;v++) {
    *v=src[carp>>SYNTH_WAVE_SHIFT];
    float mod=src[modp>>SYNTH_WAVE_SHIFT]*range;
    modp+=moddp;
    uint32_t cardp=(int32_t)mod+(1<<SYNTH_WAVE_SHIFT);
    carp+=cardp;
  }
}

/* Apply harmonics.
 * This requires a second buffer so it's fallible.
 */
 
static void synth_wave_apply_harmonic(float *dst,const float *src,float level,int step) {
  if (step>=SYNTH_WAVE_SIZE_SAMPLES) return; // Impossible, given the constants we operate under, but would be devastating if true.
  int i=SYNTH_WAVE_SIZE_SAMPLES;
  int srcp=0;
  for (;i-->0;dst++) {
    (*dst)+=src[srcp]*level;
    if ((srcp+=step)>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
  }
}
 
static int synth_wave_apply_harmonics(struct synth_wave *wave,const float *coefv,int coefc) {
  float *scratch=calloc(sizeof(float),SYNTH_WAVE_SIZE_SAMPLES);
  if (!scratch) return -1;
  int step=1;
  for (;coefc-->0;coefv++,step++) {
    if (*coefv<=0.0f) continue;
    synth_wave_apply_harmonic(scratch,wave->v,*coefv,step);
  }
  memcpy(wave->v,scratch,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
  free(scratch);
  return 0;
}

/* New wave.
 */
 
struct synth_wave *synth_wave_new(struct synth *synth,const void *src,int srcc) {
  if ((srcc<3)||!src) return 0;
  const uint8_t *SRC=src;
  int shape=SRC[0];
  int qual=SRC[1];
  int coefc=SRC[2];
  if (3+coefc*2>srcc) return 0;
  struct synth_wave *wave=0;
  switch (shape) {
  
    case EAU_SHAPE_SINE: {
        if (synth_sine_require(synth)) {
          if (!coefc) { // They're asking for a plain sine wave. Use the reference directly.
            if (synth_wave_ref(synth->sine)>=0) return synth->sine;
          }
          if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
          wave->refc=1;
          memcpy(wave->v,synth->sine->v,sizeof(float)*SYNTH_WAVE_SIZE_SAMPLES);
        } else {
          if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
          wave->refc=1;
          synth_wave_print_sine(wave->v);
        }
      } break;
      
    case EAU_SHAPE_SQUARE: {
        if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
        wave->refc=1;
        synth_wave_print_square(wave->v,qual,synth);
      } break;
      
    case EAU_SHAPE_SAW: {
        if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
        wave->refc=1;
        synth_wave_print_saw(wave->v,qual,synth);
      } break;
      
    case EAU_SHAPE_TRIANGLE: {
        if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
        wave->refc=1;
        synth_wave_print_triangle(wave->v,qual,synth);
      } break;
      
    case EAU_SHAPE_FIXEDFM: {
        if (!synth_sine_require(synth)) return 0;
        if (!qual&&!coefc) { // With no qualifier or coefficients, it's just a sine wave.
          if (synth_wave_ref(synth->sine)>=0) return synth->sine;
        }
        if (!(wave=calloc(1,sizeof(struct synth_wave)))) return 0;
        wave->refc=1;
        synth_wave_print_fixedfm(wave->v,qual,synth->sine->v);
      } break;
      
    default: return 0;
  }
  if (coefc>0) {
    float coefv[128];
    int i=0;
    SRC+=3;
    for (;i<coefc;i++,SRC+=2) {
      coefv[i]=((SRC[0]<<8)|SRC[1])/65535.0f;
    }
    if (synth_wave_apply_harmonics(wave,coefv,coefc)<0) {
      synth_wave_del(wave);
      return 0;
    }
  }
  return wave;
}

/* Measure serial.
 */
 
int synth_wave_measure(const void *src,int srcc) {
  if (!src||(srcc<3)) return -1;
  const uint8_t *SRC=src;
  int coefc=SRC[2];
  int len=3+coefc*2;
  if (len>srcc) return -1;
  return len;
}
