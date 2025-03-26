/* synth_pcm.h
 * PCM, Printer, and Wave.
 * Also let's dump Envelope and the little filter bits here, feels right.
 */
 
#ifndef SYNTH_PCM_H
#define SYNTH_PCM_H

/* PCM dump.
 *******************************************************************************/

// Sanity limit in samples, well below the overflow point and well above any sane use case.
#define SYNTH_PCM_LENGTH_LIMIT 0x10000000
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

/* Wave.
 *******************************************************************************/

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
int synth_wave_ref(struct synth_wave *wave);

/* Decode an EAU wave into a new object.
 * It's possible you'll get an existing object retained from (synth).
 */
struct synth_wave *synth_wave_new(struct synth *synth,const void *src,int srcc);

int synth_wave_measure(const void *src,int srcc);

/* PCM Printer.
 ****************************************************************************/
 
struct synth_printer {
  struct synth_pcm *pcm;
  struct synth *synth; // Private context that does the printing.
  int p;
};

void synth_printer_del(struct synth_printer *printer);

/* New printer for some EAU file.
 * On success, (printer->pcm) is fully allocated but all zeroes.
 */
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);

/* Print up to (framec) more frames of PCM.
 * Returns 0 if complete, >0 if still running, no errors.
 * It's legal to call with (framec<1) if you just want to test completion.
 */
int synth_printer_update(struct synth_printer *printer,int framec);

/* PCM Player.
 *******************************************************************************/
 
struct synth_pcmplay {
  struct synth_pcm *pcm;
  float gainl,gainr;
  int p;
  int chanc;
};

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay);

int synth_pcmplay_setup(struct synth_pcmplay *pcmplay,int chanc,struct synth_pcm *pcm,double trim,double pan);

/* Add my signal, multi-channel, to (v), up to (framec) frames.
 * Returns 0 if complete, >0 if still running, no errors.
 */
int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay);

/* Envelope config and runner.
 *******************************************************************************/
 
#define SYNTH_ENV_POINT_LIMIT 16 /* One less, if sustaining. */

#define SYNTH_ENV_FLAG_VELOCITY 0x01
#define SYNTH_ENV_FLAG_INITIAL  0x02
#define SYNTH_ENV_FLAG_SUSTAIN  0x04

struct synth_env {
  uint8_t flags;
  uint8_t susp;
  float initlo,inithi;
  struct synth_env_point {
    int tlo,thi; // frames
    float vlo,vhi;
  } pointv[SYNTH_ENV_POINT_LIMIT];
  int pointc;
// Runner only:
  float level;
  float dlevel;
  int c;
  int pointp;
};

/* Measure and decode an envelope config.
 * Times are converted to frames, minimum 1, and levels scaled to 0..1.
 * (level,dlevel,c,pointp) are not populated.
 * Returns consumed length.
 */
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int rate);

/* Adjust all values of an envelope config.
 */
void synth_env_scale(struct synth_env *env,float mlt);
void synth_env_bias(struct synth_env *env,float add);

/* Start an envelope runner, using a separate synth_env as configuration.
 * Runners only use their 'lo' line, and track level and timing.
 * (durframec) is the duration from start to release, for sustaining envelopes.
 */
void synth_env_reset(struct synth_env *runner,const struct synth_env *config,float velocity,int durframec);

/* If we are sustainable and haven't yet finished the sustain point, begin releasing now.
 * Or if we haven't reached the sustain leg yet, drop its duration to the minimum.
 */
void synth_env_release(struct synth_env *env);

// PRIVATE, only synth_env_update() should call it.
void synth_env_advance(struct synth_env *env);

static inline int synth_env_is_complete(const struct synth_env *env) {
  return (env->pointp>=env->pointc);
}

// How many frames left.
int synth_env_remaining(const struct synth_env *env);

/* Advance time by one frame and return the next value.
 */
static inline float synth_env_update(struct synth_env *env) {
  if (env->c<=0) synth_env_advance(env);
  env->c--;
  env->level+=env->dlevel;
  return env->level;
}

/* Ring buffer.
 ********************************************************************************/
 
struct synth_ring {
  int c,p;
  float *v;
};

void synth_ring_cleanup(struct synth_ring *ring);

int synth_ring_init(struct synth_ring *ring,int framec);

static inline void synth_ring_write(struct synth_ring *ring,float v) {
  ring->v[ring->p]=v;
}
static inline float synth_ring_read(const struct synth_ring *ring) {
  return ring->v[ring->p];
}
static inline void synth_ring_advance(struct synth_ring *ring) {
  ring->p++;
  if (ring->p>=ring->c) ring->p=0;
}

/* IIR runner.
 ********************************************************************************/
 
struct synth_iir3 {
  float dcv[3];
  float wcv[2];
  float dv[3];
  float wv[2];
};

void synth_iir3_init_lopass(struct synth_iir3 *iir,float freq);
void synth_iir3_init_hipass(struct synth_iir3 *iir,float freq);
void synth_iir3_init_bpass(struct synth_iir3 *iir,float mid,float wid);
void synth_iir3_init_notch(struct synth_iir3 *iir,float mid,float wid);

static inline float synth_iir3_update(struct synth_iir3 *iir,float src) {
  float dst=
    iir->dcv[0]*iir->dv[0]+
    iir->dcv[1]*iir->dv[1]+
    iir->dcv[2]*iir->dv[2]+
    iir->wcv[0]*iir->wv[0]+
    iir->wcv[1]*iir->wv[1];
  iir->dv[2]=iir->dv[1];
  iir->dv[1]=iir->dv[0];
  iir->dv[0]=src;
  iir->wv[1]=iir->wv[0];
  iir->wv[0]=dst;
  return dst;
}

#endif
