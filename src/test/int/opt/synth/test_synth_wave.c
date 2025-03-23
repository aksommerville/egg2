#include "test/egg_test.h"
#include "opt/synth/synth_internal.h"

/* Generic analysis.
 */
 
struct wavean {
  float lo,hi; // Verbatim peaks.
  float peak; // Absolute value of (lo) or (hi), whichever is more significant.
  float range; // 0..2 (or greater, if the wave overflows)
  float sum,sqsum; // Bookkeeping, usually not interesting.
  float avg,rms; // Straight mean and Root-Mean-Square of entire wave.
  float difflo,diffhi,diffmax; // Absolute value of most signifcant sample-to-sample change. (plus the precursors)
};

static void wavean_compute(struct wavean *wavean,const struct synth_wave *wave) {
  float d=wave->v[0]-wave->v[SYNTH_WAVE_SIZE_SAMPLES-1];
  memset(wavean,0,sizeof(struct wavean));
  wavean->lo=wavean->hi=wave->v[0];
  wavean->difflo=wavean->diffhi=d;
  wavean->sum=wave->v[0];
  wavean->sqsum=wavean->sum*wavean->sum;
  int i=1; for (;i<SYNTH_WAVE_SIZE_SAMPLES;i++) {
    
    float nd=wave->v[i]-wave->v[i-1];
    d=nd;
    if (d<wavean->difflo) wavean->difflo=d;
    else if (d>wavean->diffhi) wavean->diffhi=d;
    
    if (wave->v[i]<wavean->lo) wavean->lo=wave->v[i];
    else if (wave->v[i]>wavean->hi) wavean->hi=wave->v[i];
    wavean->sum+=wave->v[i];
    wavean->sqsum+=wave->v[i]*wave->v[i];
  }
  wavean->range=wavean->hi-wavean->lo;
  if (wavean->lo<0.0f) wavean->peak=-wavean->lo;
  else wavean->peak=wavean->lo;
  if (wavean->hi>wavean->peak) wavean->peak=wavean->hi;
  wavean->avg=wavean->sum/SYNTH_WAVE_SIZE_SAMPLES;
  wavean->rms=sqrtf(wavean->sqsum/SYNTH_WAVE_SIZE_SAMPLES);
  float ad=wavean->difflo;
  if (ad<0.0f) ad=-ad;
  if (wavean->diffhi>ad) ad=wavean->diffhi;
  wavean->diffmax=ad;
}

/* Output for troubleshooting.
 * Don't enable these by default, just as needed.
 */

static void wavean_log(const struct wavean *wavean,const char *desc) {
  fprintf(stderr,
    "%s: peak=%f range=%f rms=%f\n",
    desc,wavean->peak,wavean->range,wavean->rms
  );
}

static void wavean_draw(const struct synth_wave *wave) {
  // Arbitrary picture size in terminal cells:
  #define COLC 128
  #define ROWC 40
  char pic[(COLC+1)*ROWC];
  memset(pic,0x20,sizeof(pic));
  int stride=COLC+1;
  char *dst=pic+COLC;
  int i=ROWC;
  for (;i-->0;dst+=stride) *dst=0x0a;
  int pvx=-1;
  float lo,hi;
  int srcp=0;
  #define RENDERCOL { \
    /* Swap lo and hi here, because we're reversing y coordinates too. (positive=up) */ \
    int ylo=lround(hi*(ROWC*-0.5f))+(ROWC>>1); \
    int yhi=lround(lo*(ROWC*-0.5f))+(ROWC>>1); \
    if (ylo<0) ylo=0; else if (ylo>=ROWC) ylo=ROWC-1; \
    if (yhi<0) yhi=0; else if (yhi>=ROWC) yhi=ROWC-1; \
    int y=ylo; \
    dst=pic+y*stride+pvx; \
    for (;y<=yhi;y++,dst+=stride) { \
      *dst='X'; \
    } \
  }
  for (;srcp<SYNTH_WAVE_SIZE_SAMPLES;srcp++) {
    int x=(srcp*COLC)/SYNTH_WAVE_SIZE_SAMPLES;
    if (x<0) x=0; else if (x>=COLC) x=COLC-1;
    if (x==pvx) {
      if (wave->v[srcp]<lo) lo=wave->v[srcp];
      else if (wave->v[srcp]>hi) hi=wave->v[srcp];
    } else {
      if (pvx>=0) {
        RENDERCOL
      }
      pvx=x;
      lo=hi=wave->v[srcp];
    }
  }
  RENDERCOL
  #undef RENDERCOL
  fprintf(stderr,"%.*s",(int)sizeof(pic),pic);
  #undef COLC
  #undef ROWC
}

/* Shape 0, SINE, does not use qualifier, and must produce a nice smooth sine wave.
 */
 
static int wavean_sine(struct synth *synth) {
  struct synth_wave *wave;
  struct wavean wavean;
  int qual=0xff; // zero to run the whole gamut (silly to do that every time, we know it's the exact same wave every time)
  for (;qual<0x100;qual++) {
    uint8_t serial[]={EAU_SHAPE_SINE,qual,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"sine");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"sine %d",qual)
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"sine %d",qual)
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"sine %d",qual)
    synth_wave_del(wave);
  }
  return 0;
}

/* Square and saw use qualifier as a trivial low-pass, they gradually become sine-like.
 * Triangle does not use qualifier because I'm not sure how it ought to.
 */
 
static int wavean_shapes(struct synth *synth) {
  struct synth_wave *wave;
  struct wavean wavean;
  
  // Trivial square must be two straight lines.
  // Its RMS of one is an unbeatable world record.
  {
    uint8_t serial[]={EAU_SHAPE_SQUARE,0x00,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"square 0");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"square 0")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"square 0")
    EGG_ASSERT_FLOATS(wavean.rms,1.0,0.001,"square 0")
    synth_wave_del(wave);
  }
  
  // Middle square.
  {
    uint8_t serial[]={EAU_SHAPE_SQUARE,0x80,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"square 128");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"square 128")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"square 128")
    // RMS must land comfortably between 0.707 and 1.000:
    EGG_ASSERT(wavean.rms>0.750,"square 128, rms=%f",wavean.rms)
    EGG_ASSERT(wavean.rms<0.950,"square 128, rms=%f",wavean.rms)
    synth_wave_del(wave);
  }
  
  // Square with full qualifier must be essentially a sine wave.
  {
    uint8_t serial[]={EAU_SHAPE_SQUARE,0xff,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"square max");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"square max")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"square max")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.005,"square max") // Broader tolerance than usual because the process is not exact.
    synth_wave_del(wave);
  }
  
  // Pure saw is nice and simple.
  {
    uint8_t serial[]={EAU_SHAPE_SAW,0x00,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"saw 0");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.005,"saw 0")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.005,"saw 0")
    EGG_ASSERT_FLOATS(wavean.rms,0.577,0.005,"saw 0")
    synth_wave_del(wave);
  }
  
  // Full qualifier for saw produces half of a sine wave, with a half-period ramp connecting the peaks.
  {
    uint8_t serial[]={EAU_SHAPE_SAW,0xff,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"saw max");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.005,"saw max")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.005,"saw max")
    // rms for qualified saws is kind of tricky to predict, not going to worry about it.
    synth_wave_del(wave);
  }
  
  // Triangle.
  {
    uint8_t serial[]={EAU_SHAPE_TRIANGLE,0x00,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"triangle");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.005,"triangle")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.005,"triangle")
    EGG_ASSERT_FLOATS(wavean.rms,0.577,0.005,"triangle")
    synth_wave_del(wave);
  }
  
  return 0;
}

/* Fixed FM, our neatest trick.
 * I've really just enabled the drawing and confirmed by eye that they Do Stuff.
 * Won't be able to say much about the sound until we hear it.
 */
 
static int wavean_fixedfm(struct synth *synth) {
  struct synth_wave *wave;
  struct wavean wavean;
  
  // If either field of the qualifier is zero, output must be a pure sine wave.
  {
    uint8_t serial[]={EAU_SHAPE_FIXEDFM,0x00,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"fixedfm edge");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"fixedfm edge")
    synth_wave_del(wave);
  }
  {
    uint8_t serial[]={EAU_SHAPE_FIXEDFM,0x0f,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"fixedfm edge");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"fixedfm edge")
    synth_wave_del(wave);
  }
  {
    uint8_t serial[]={EAU_SHAPE_FIXEDFM,0xf0,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"fixedfm edge");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"fixedfm edge")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"fixedfm edge")
    synth_wave_del(wave);
  }
  
  {
    uint8_t serial[]={EAU_SHAPE_FIXEDFM,0x81,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"fixedfm 0x81");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"fixedfm 0x81")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"fixedfm 0x81")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"fixedfm 0x81")
    synth_wave_del(wave);
  }
  
  {
    uint8_t serial[]={EAU_SHAPE_FIXEDFM,0x88,0};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"fixedfm 0x88");
    //wavean_draw(wave);
    EGG_ASSERT_FLOATS(wavean.lo,-1.0,0.001,"fixedfm 0x88")
    EGG_ASSERT_FLOATS(wavean.hi,1.0,0.001,"fixedfm 0x88")
    EGG_ASSERT_FLOATS(wavean.rms,0.707,0.001,"fixedfm 0x88")
    synth_wave_del(wave);
  }
  
  return 0;
}

/* Test a few with harmonic addition.
 */
 
static int wavean_harmonics(struct synth *synth) {
  struct synth_wave *wave;
  struct wavean wavean;
  
  // With a small set of harmonics against pure square, you can see an interesting staircase.
  // And even prettier with a low qualifier too.
  // Use FIXEDFM instead, and you can generate all kinds of crazy shapes.
  {
    uint8_t serial[]={EAU_SHAPE_SQUARE,0x21,3, 0x80,0x00, 0x40,0x00, 0x20,0x00};
    EGG_ASSERT(wave=synth_wave_new(synth,serial,sizeof(serial)))
    wavean_compute(&wavean,wave);
    //wavean_log(&wavean,"square plus");
    //wavean_draw(wave);
    // Not sure what to assert for these... just enable the logging and validate by eye.
    synth_wave_del(wave);
  }
  
  return 0;
}

/* Wave analysis, main entry point.
 */

EGG_ITEST(synth_wave_analysis) {
  int err=0;

  /* We need a synthesizer in order to generate waves.
   * That's not a technical necessity, just an administrative one.
   * The rate and channel count we provide here are completely irrelevant.
   */
  struct synth *synth=synth_new(44100,1);
  EGG_ASSERT(synth)
  
  if ((err=wavean_sine(synth))<0) goto _done_;
  if ((err=wavean_shapes(synth))<0) goto _done_;
  if ((err=wavean_fixedfm(synth))<0) goto _done_;
  if ((err=wavean_harmonics(synth))<0) goto _done_;
  
 _done_:;
  synth_del(synth);
  return err;
}
