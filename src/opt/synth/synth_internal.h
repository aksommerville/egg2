#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include <limits.h>

/*
#if USE_native
  #define logint(n)
#else
  WASM_IMPORT("logint") void logint(int n);//XXX
#endif
*/

struct synth_song;
struct synth_channel;
struct synth_channel_type;
struct synth_pipe;
struct synth_pcmplay;
struct synth_printer;
struct synth_pcm;
struct synth_wave;
struct synth_env;
struct synth_iir3;

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

// We pack songs and sounds together in one resource list. Sound rids get this extra bit.
#define SYNTH_RID_SOUND 0x10000

#define SYNTH_FADEOUT_TIME_S 0.250 /* When a song is requested to stop, it does a quick fade-out first. */

#define SYNTH_DEFAULT_HOLD_TIME_S 20 /* If you start a note without saying when to stop, channels may set this future expiry time. */

#define SYNTH_BEND_LIMIT_CENTS 120000 /* 10 octaves, which is a crazy amount of pitch bend. No sense looping to compute further bends. */

/* Serial format can describe up to 15 points.
 * Live limit is one greater, because when sustain in play, we add a point.
 */
#define SYNTH_ENV_POINT_LIMIT 16

// synth_env.flags
#define SYNTH_ENV_INITIALS 0x01
#define SYNTH_ENV_VELOCITY 0x02
#define SYNTH_ENV_SUSTAIN  0x04
#define SYNTH_ENV_PRESENT  0x08 /* Forced on for all non-default envelopes. (and always off for default ones). */

// Fallback for synth_env_decode(), what to do if input is empty.
#define SYNTH_ENV_FALLBACK_ZERO  0 /* Constant zero, the default's default. */
#define SYNTH_ENV_FALLBACK_ONE   1 /* Constant one, eg for FM range. */
#define SYNTH_ENV_FALLBACK_HALF  2 /* Constant 0.5, eg for pitch bend. */
#define SYNTH_ENV_FALLBACK_LEVEL 3 /* Opinionated default for level envelopes. */

// Maximum adjustment to delay period for the "sparkle" effect, in ms.
#define SYNTH_SPARKLE_TIME_MS 20

/* Linear envelope.
 * The same object is used for the general config and the runner.
 * Runners only use the "lo" points.
 * Times are in frames (and we require (synth.rate) to be valid at decode).
 *****************************************************************************/
 
struct synth_env {
  uint8_t flags;
  float initlo,inithi;
  int susp; // If >=0, it's the index in (pointv) to hold at; the leg leading to that point is negotiable.
  int pointc;
  struct synth_env_point {
    int tlo,thi;
    float vlo,vhi;
  } pointv[SYNTH_ENV_POINT_LIMIT];
  
  // Only relevant to runners:
  int finished;
  int pointp;
  float v;
  float dv;
  int c; // Counts down to next (pointp).
};

/* Populates an envelope config and returns length consumed.
 */
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int fallback);

// Apply a scale or bias to every value.
void synth_env_mlt(struct synth_env *env,float mlt);
void synth_env_add(struct synth_env *env,float add);

/* Prepare a runner from a config.
 * Provide (durframes) so we can set the sustain time.
 */
void synth_env_apply(struct synth_env *runner,const struct synth_env *config,float velocity,int durframes);

/* If this runner sustains and hasn't finished sustaining yet, drop the sustain to zero.
 */
void synth_env_release(struct synth_env *env);

/* Advance runner by one frame; evaluates to the next level.
 */
#define synth_env_update(env) ({ \
  float _v=(env).v; \
  if ((env).c>0) { \
    (env).c--; \
    (env).v+=(env).dv; \
  } else { \
    synth_env_advance(&(env)); \
  } \
  (_v); \
})

// Should only be invoked by synth_env_update().
void synth_env_advance(struct synth_env *env);

/* Post-process pipe.
 *****************************************************************************/
 
struct synth_pipe_stage {
  uint8_t type;
  void (*del)(struct synth_pipe_stage *stage);
  void (*update_mono)(float *dst,struct synth_pipe_stage *stage,int framec);
  void (*update_stereo)(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec);
};
 
struct synth_pipe {
  struct synth_pipe_stage **stagev;
  int stagec,stagea;
  struct synth_song *song; // WEAK
};

void synth_pipe_del(struct synth_pipe *pipe);
struct synth_pipe *synth_pipe_new(struct synth_song *owner,const uint8_t *src,int srcc);
void synth_pipe_update_mono(float *dst,struct synth_pipe *pipe,int framec);
void synth_pipe_update_stereo(float *dstl,float *dstr,struct synth_pipe *pipe,int framec);

/* Song channel.
 *******************************************************************************/
 
struct synth_channel {
  const struct synth_channel_type *type;
  struct synth_song *song; // WEAK
  float trim,pan;
  float wheelf; // -1..1
  uint8_t chid;
  uint8_t mode;
  struct synth_pipe *post;
  float *bufl,*bufr; // (bufr) only exists when (song->chanc>=2)
  int defunct; // Nonzero after a fadeout completes; nothing more can happen on the channel.
  float fadeout; // 1=>0
  float fadeoutd;
  
  /* (update_mono) is required, stereo optional. It's normal to implement only mono.
   * Channels that update in stereo generally ignore the song and channel level pan. Your update hook may choose to respect them.
   * Outputs are normally (channel->bufl,bufr). Channel must add; the outer layers zero them first.
   * Do not apply trim, pan, or post.
   * These live on the instance instead of the type, so that types can easily split out differently-optimized implementations.
   */
  void (*update_mono)(float *dst,struct synth_channel *channel,int framec);
  void (*update_stereo)(float *dstl,float *dstr,struct synth_channel *channel,int framec);
};

struct synth_channel_type {
  const char *name;
  int objlen;
  void (*del)(struct synth_channel *channel);
  
  /* Buffers and post are not initialized yet when we call this.
   * (del) will only be called on channels for which we called (init), and *will* be called if init fails.
   * You *must* set (channel->update_mono) during this call, or the wrapper will fail.
   */
  int (*init)(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc);
  
  /* All channels with addressable voices must implement this.
   * Effect is the same as note_off() for every addressable voice.
   * Those started with note_once() should release immediately too, if possible.
   */
  void (*release_all)(struct synth_channel *channel);
  
  // OPTIONAL. Notification that (channel->wheelf) was just changed. Update addressable voices, if you do that.
  void (*wheel_changed)(struct synth_channel *channel);
  
  /* If you only do fire-and-forget voices, eg drum, it makes sense to implement only (note_once).
   * Note that duration is in frames, velocity is floating-point, and (note_off) does not get a velocity -- unlike the general event api.
   * It's legal but ridiculous to leave all of these unset.
   * If you implement (on/off) but not (once), any request for note_once will effectively have zero duration. We don't try to fake it.
   */
  void (*note_once)(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes);
  void (*note_on)(struct synth_channel *channel,uint8_t noteid,float velocity);
  void (*note_off)(struct synth_channel *channel,uint8_t noteid);
};

const struct synth_channel_type *synth_channel_type_for_mode(uint8_t mode);
extern const struct synth_channel_type synth_channel_type_trivial; // mode 1
extern const struct synth_channel_type synth_channel_type_fm; // mode 2
extern const struct synth_channel_type synth_channel_type_sub; // mode 3
extern const struct synth_channel_type synth_channel_type_drum; // mode 4

void synth_channel_del(struct synth_channel *channel);

struct synth_channel *synth_channel_new(
  struct synth_song *owner,
  uint8_t chid,uint8_t trim,uint8_t pan,uint8_t mode,
  const uint8_t *modecfg,int modecfgc,
  const uint8_t *post,int postc
);

/* You can set trim or pan to its existing value, to force re-read of the owning song's.
 */
void synth_channel_set_trim(struct synth_channel *channel,float trim);
void synth_channel_set_pan(struct synth_channel *channel,float pan);

void synth_channel_set_wheel(struct synth_channel *channel,float v);
void synth_channel_note_off(struct synth_channel *channel,uint8_t noteid,uint8_t velocity);
void synth_channel_note_on(struct synth_channel *channel,uint8_t noteid,uint8_t velocity);
void synth_channel_note_once(struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int durms);

void synth_channel_fade_out(struct synth_channel *channel,int framec);
void synth_channel_release_all(struct synth_channel *channel);

void synth_channel_update_stereo(float *dstl,float *dstr,struct synth_channel *channel,int framec);
void synth_channel_update_mono(float *dst,struct synth_channel *channel,int framec);

/* Song player.
 *****************************************************************************/
 
struct synth_song {
  int songid;
  int rid;
  int exclusive;
  int repeat;
  float trim,pan;
  int chanc; // Not necessarily the global chanc; it's always 1 if we're owned by a printer.
  float tempo; // s/qnote
  struct synth_channel **channelv;
  int channelc,channela;
  struct synth_channel *channel_by_chid[16]; // WEAK, sparse
  const uint8_t *evtv; // Borrowed from input serial.
  int evtc;
  int evtp;
  int delay; // Frames.
  int loopp; // Bytes into (evtv). Zero until we pass a Loop Point event.
  int terminated;
  int deathclock; // Frames.
  int durframes; // Computed the first time someone asks. We don't use internally.
  int phframes; // Playhead. Total output since beginning of song, but it wraps around at loop.
  int loopframes; // Playhead at loop point.
};

void synth_song_del(struct synth_song *song);

/* (src) is borrowed; caller must keep it alive as long as the song is playing.
 */
struct synth_song *synth_song_new(int chanc,const void *src,int srcc,float trim,float pan);

/* <0 for errors, 0 if complete, >0 if still running.
 * (dstr) optional.
 * Adds to (dstl,dstr).
 */
int synth_song_update(float *dstl,float *dstr,struct synth_song *song,int framec);

/* Release all notes and arrange not to produce any new ones.
 * Arrange for signal output to end soon, preferably with a brief fade-out.
 */
void synth_song_stop(struct synth_song *song);

/* Sum of delays.
 */
int synth_song_get_duration_frames(struct synth_song *song);

float synth_song_get_playhead(const struct synth_song *song);
void synth_song_set_playhead(struct synth_song *song,float s);
void synth_song_set_trim(struct synth_song *song,float trim);
void synth_song_set_pan(struct synth_song *song,float pan);

// Return a normalized wave step or duration in frames, for some period in qnotes, according to (song)'s tempo.
uint32_t synth_song_get_tempo_step(const struct synth_song *song,float qnotes);
int synth_song_get_tempo_frames(const struct synth_song *song,float qnotes);

/* General-purpose ring buffer.
 ************************************************************************/
 
// Sanity limits in frames regardless of main rate.
#define SYNTH_RING_MIN 10
#define SYNTH_RING_MAX 100000
 
struct synth_ring {
  int c,p;
  float *v;
};

void synth_ring_cleanup(struct synth_ring *ring);
int synth_ring_resize(struct synth_ring *ring,int framec);

#define synth_ring_read(ring) ((ring).v[(ring).p])
#define synth_ring_write(ring,vv) ((ring).v[(ring).p]=(vv))
#define synth_ring_step(ring) ({ if (++((ring).p)>=(ring).c) (ring).p=0; 0; })

/* Single-stage 3-point IIR filter.
 * Suitable for bandpass and notch, though usually you'll want to chain two or three of them.
 **********************************************************************/
 
struct synth_iir3 {
  float v[5];
  float cv[5];
};

void synth_iir3_init_bpass(struct synth_iir3 *iir3,float mid,float wid);

static inline float synth_iir3_update(struct synth_iir3 *iir3,float src) {
  iir3->v[2]=iir3->v[1];
  iir3->v[1]=iir3->v[0];
  iir3->v[0]=src;
  float wet=
    iir3->v[0]*iir3->cv[0]+
    iir3->v[1]*iir3->cv[1]+
    iir3->v[2]*iir3->cv[2]+
    iir3->v[3]*iir3->cv[3]+
    iir3->v[4]*iir3->cv[4];
  iir3->v[4]=iir3->v[3];
  iir3->v[3]=wet;
  return wet;
}

/* Single-period wave.
 **************************************************************************/
 
struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
struct synth_wave *synth_wave_new();
int synth_wave_ref(struct synth_wave *wave);

void synth_wave_generate_sine(struct synth_wave *wave);
int synth_wave_decode(struct synth_wave *wave,const void *src,int srcc);

/* Dumb PCM dump.
 ***************************************************************************/
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);
int synth_pcm_ref(struct synth_pcm *pcm);

/* PCM player.
 *****************************************************************************/
 
struct synth_pcmplay {
  struct synth_pcm *pcm; // WEAK
  int p;
  float triml,trimr;
};

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay);

int synth_pcmplay_init(struct synth_pcmplay *pcmplay,struct synth_pcm *pcm,float trim,float pan);

/* <0 for errors, 0 if complete, >0 if still running.
 * (dstr) optional.
 * Adds to (dstl,dstr).
 */
int synth_pcmplay_update(float *dstl,float *dstr,struct synth_pcmplay *pcmplay,int framec);

/* PCM printer.
 ********************************************************************************/
 
struct synth_printer {
  struct synth_pcm *pcm;
  struct synth_song *song;
  int p;
};

void synth_printer_del(struct synth_printer *printer);

/* Make a new printer to generate PCM from the given EAU file.
 */
struct synth_printer *synth_printer_new(const void *src,int srcc);

/* Advance by at least (framec) frames, printing to my own buffer.
 * <0 for error, 0 if complete, >0 if still running.
 */
int synth_printer_update(struct synth_printer *printer,int framec);

/* Global context.
 *****************************************************************************/
 
extern struct synth {
  int rate,chanc,buffer_frames;
  
  float *bufl,*bufr; // (bufr) only exists if (chanc>=2)
  
  uint8_t *rom;
  int romc;
  
  struct synth_song **songv;
  int songc,songa;
  struct synth_pcmplay *pcmplayv;
  int pcmplayc,pcmplaya;
  struct synth_printer **printerv;
  int printerc,printera;
  
  int framec_in_progress;
  
  struct synth_res {
    int rid;
    struct synth_pcm *pcm; // only if sound
    const void *serial; // WEAK, points into (rom)
    int serialc;
  } *resv;
  int resc,resa;
  
  struct synth_wave sine;
  float fratev[128];
  uint32_t iratev[128];
  float cents_octave[12]; // Twelfth roots of two, ie intervals between notes in one octave.
  float cents_halfstep[100]; // Twelve-hundredth roots of two, ie multipliers for cent intervals between two notes.
  float invcents_octave[12];
  float invcents_halfstep[100];
  
} synth;

int synth_frames_from_ms(int ms);

/* These are constant. If libm were in play, or in pure math, it's just `pow(2.0f,cents/1200.0f)`.
 * We're not using libm, so we do a kind of complex lookup-table dance.
 */
float synth_bend_from_cents(int cents);

/* Install a printer and return its PCM (not yet printed).
 * Returns STRONG.
 */
struct synth_pcm *synth_begin_print(const void *src,int srcc);

/* synth_stdlib.c
 * A few things that either come from real stdlib, or our own fake implementation.
 * Build with -DUSE_native=1 to use standard malloc, or -DUSE_web=1 to use ours, taking advantage of some wasm intrinsics.
 * In either case, rand() is a private implementation, so as to be consistent.
 ****************************************************************************************/

/* Generate (c) pseudo-random numbers in -1..1, the same sequence every time for a given (state).
 * Returns the next (state) in case you want to continue the sequence later.
 * (state) must be nonzero, I recommend 0xaaaaaaaa to start.
 */
uint32_t synth_rand(float *v,int c,uint32_t state);

void synth_malloc_quit();
int synth_malloc_init();
void synth_free(void *p);
void *synth_malloc(int len);
void *synth_calloc(int a,int b);
void *synth_realloc(void *p,int len);

#endif
