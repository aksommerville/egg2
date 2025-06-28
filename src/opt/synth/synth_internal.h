#ifndef SYNTH_INTERNAL_H
#define SYNTH_INTERNAL_H

#include "synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define SYNTH_RATE_MIN 2000
#define SYNTH_RATE_MAX 200000
#define SYNTH_CHANC_MIN 1
#define SYNTH_CHANC_MAX 8
#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)
#define SYNTH_ID_SONG 0x10000
#define SYNTH_ID_SOUND 0x20000
#define SYNTH_UPDATE_LIMIT 1024 /* Longest update of the signal graph, in frames. */
#define SYNTH_PCM_LIMIT_SAMPLES (1<<20) /* Sanity limit. Keep it way too long, since it doesn't account for rate. */
#define SYNTH_SONG_FADE_TIME 0.500f

struct synth_pcm;
struct synth_wave;
struct synth_printer;
struct synth_pcmplay;
struct synth_song;

/* PCM dump etc.
 *****************************************************************************/
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
int synth_wave_ref(struct synth_wave *wave);
struct synth_wave *synth_wave_new();

// There isn't a "sine". Use (synth->sine), precalculated at context init.
struct synth_wave *synth_wave_new_square();
struct synth_wave *synth_wave_new_saw();
struct synth_wave *synth_wave_new_triangle();

// To build up harmonics, you must supply us the sine.
struct synth_wave *synth_wave_new_harmonics(const float *ref,const float *v,int c);

struct synth_printer {
  struct synth_song *song;
  struct synth_pcm *pcm;
  int p;
};

void synth_printer_del(struct synth_printer *printer);
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);
int synth_printer_update(struct synth_printer *printer,int c); // >0 if still running

struct synth_pcmplay {
  struct synth_pcm *pcm;
  int p;
  int chanc;
  float triml,trimr;
};

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay);
int synth_pcmplay_init(struct synth_pcmplay *pcmplay,struct synth_pcm *pcm,int chanc,float trim,float pan);
int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay); // adds; >0 if still running

/* Envelope.
 * The (synth_env) object can be either a configuration or a runner.
 * For runners, only the 'lo' values are applicable.
 *******************************************************************************/
 
#define SYNTH_ENV_POINT_LIMIT 16 /* Not negotiable; mandated by spec. */
#define SYNTH_ENV_FLAG_INITIALS 0x01
#define SYNTH_ENV_FLAG_VELOCITY 0x02
#define SYNTH_ENV_FLAG_SUSTAIN  0x04
#define SYNTH_ENV_FLAG_DEFAULT  0x80 /* Not encoded. Added to envelopes that received their default. */

struct synth_env {
  uint8_t flags;
  float initlo,inithi;
  int susp;
  int pointc;
  struct synth_env_point {
    int tlo,thi;
    float vlo,vhi;
  } pointv[SYNTH_ENV_POINT_LIMIT];
// Only for runner:
  float v;
  float dv;
  int c;
  int pointp;
  int finished;
};

/* Decode a config and return length consumed.
 * The default env [0,0] is treated literally; caller should manage those separate.
 * Freshly-decoded values are in 0..65535 just as encoded, but floating-point.
 * (rate) in Hz is required, so we can phrase times in frames rather than the encoded milliseconds.
 */
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int rate);

/* If you encounter [0,0], call one of these defaulters instead of decode, and consume the two zeroes.
 */
void synth_env_default_level(struct synth_env *env,int rate);
void synth_env_default_range(struct synth_env *env,int rate);
void synth_env_default_pitch(struct synth_env *env,int rate);

void synth_env_scale(struct synth_env *env,float mlt);
void synth_env_bias(struct synth_env *env,float add);

/* Initialize an envelope runner from a configuration, with velocity and hold time.
 */
void synth_env_apply(struct synth_env *runner,const struct synth_env *config,float velocity,int durframes);

/* If this runner has a sustain point and we haven't reached it yet, or are in the middle of it,
 * force it to release immediately.
 * Noop if there's no sustain point or we're already past it.
 */
void synth_env_release(struct synth_env *env);

// Internal use only.
void synth_env_advance(struct synth_env *env);

/* Advance envelope runner by one frame and return the next value.
 */
static inline float synth_env_update(struct synth_env *env) {
  if (env->c>0) {
    env->c--;
    env->v+=env->dv;
  } else {
    synth_env_advance(env);
  }
  return env->v;
}

/* Post pipe.
 ******************************************************************************/
 
struct synth_pipe {
  struct synth *synth;
  int rate,chanc,tempo; // tempo in frames
  struct synth_stage {
    void (*del)(struct synth_stage *stage);
    // Both update hooks are required:
    void (*update_mono)(float *v,int c,struct synth_stage *stage);
    void (*update_stereo)(float *v,int framec,struct synth_stage *stage);
  } **stagev;
  int stagec,stagea;
};

void synth_pipe_del(struct synth_pipe *pipe);

/* (chanc) must be 1 or 2; we'll force to 2 if higher.
 */
struct synth_pipe *synth_pipe_new(struct synth *synth,int chanc,int tempo,const uint8_t *src,int srcc);

/* 1 or 2 samples per frame, according to (chanc).
 */
void synth_pipe_update(float *v,int framec,struct synth_pipe *pipe);

/* Channel.
 ********************************************************************************/
 
struct synth_channel {
  struct synth *synth; // WEAK, REQUIRED
  int rate,chanc,tempo; // tempo in frames
  float pan;
  float triml,trimr; // (pan) expressed as two trims. At least one of these is 1.0.
  float trim; // For mono=>stereo, normalized trim for pan, then we run post, then main trim.
  uint8_t mode;
  uint8_t chid;
  struct synth_pipe *post;
  
  /* Prepared by mode-specific initialization.
   * All voicing modes must set update_mono.
   * Ones that do their own stereo placement (DRUM) should also set update_stereo.
   * There is no generic application of (pan) when update_stereo in use; voices should respect it on their own if possible.
   * Both updates get a zeroed buffer initially.
   */
  void (*del)(struct synth_channel *channel);
  void (*update_mono)(float *v,int framec,struct synth_channel *channel);
  void (*update_stereo)(float *v,int framec,struct synth_channel *channel);
  void *extra;
  
  float tmp[SYNTH_UPDATE_LIMIT*2]; // Channel always has enough scratch space for a stereo update.
};

void synth_channel_del(struct synth_channel *channel);

/* (src) is a full EAU "CHDR" chunk.
 */
struct synth_channel *synth_channel_new(struct synth *synth,int chanc,int tempo,const uint8_t *src,int srcc);

/* Adds to (v).
 */
void synth_channel_update(float *v,int framec,struct synth_channel *channel);

void synth_channel_note(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes);
void synth_channel_wheel(struct synth_channel *channel,int v); // -512..0..511
void synth_channel_release_all(struct synth_channel *channel);

/* Song player.
 *******************************************************************************/
 
struct synth_song {
  struct synth *synth; // WEAK, REQUIRED
  int songid; // 0..0xffff
  int repeat;
  int chanc; // Matches (synth) for the main song, but will always be 1 for pcm printers.
  int delay; // frames
  const uint8_t *v; // Events only.
  int c;
  int p; // Position in (v), past any delay events already added to (delay).
  int phframes; // Playhead, in frames. Resets at loop.
  int tempo; // frames/qnote, established at construction.
  int loopp;
  int terminated;
  float fade,dfade; // In play once (terminated).
  struct synth_channel **channelv;
  int channelc,channela;
  struct synth_channel *channel_by_chid[16]; // WEAK, SPARSE. Addressable channels.
};

void synth_song_del(struct synth_song *song);
struct synth_song *synth_song_new(struct synth *synth,const void *v,int c,int repeat,int chanc);

/* Release all held notes and poison the player so as not to accept any new events.
 * A terminated song must report completion soon.
 */
void synth_song_terminate(struct synth_song *song);

float synth_song_get_playhead(const struct synth_song *song);
void synth_song_set_playhead(struct synth_song *song,float s);
int synth_song_frames_for_bytes(const struct synth_song *song,int p);
int synth_song_measure_frames(const struct synth_song *song); // => sum of delays only

/* Advance song's clock by the given frame count, reading new events and generating a signal.
 * You must be aware of (song->chanc) and provide an agreeable buffer.
 * Adds to (v).
 * Returns >0 if still running.
 */
int synth_song_update(float *v,int framec,struct synth_song *song);

void synth_song_note(struct synth_song *song,uint8_t chid,uint8_t noteid,float velocity,int durframes);
void synth_song_wheel(struct synth_song *song,uint8_t chid,int v); // -512..0..511

/* Synth context.
 ******************************************************************************/
 
struct synth {
  int rate,chanc;
  float *qbuf;
  int qbufa;
  float ratefv[128];
  uint32_t rateiv[128];
  float sine[SYNTH_WAVE_SIZE_SAMPLES];
  
  struct synth_res {
    int id;
    const void *v;
    int c;
    struct synth_pcm *pcm; // For sound only, and null until the first use. Printing may be in progress.
  } *resv;
  int resc,resa;
  
  struct synth_printer **printerv;
  int printerc,printera;
  int framec_in_progress; // New printers must print so much immediately. Also serves as a reentry detector.
  
  struct synth_pcmplay *pcmplayv;
  int pcmplayc,pcmplaya;
  
  struct synth_song *song;
  struct synth_song *pvsong;
};

int synth_frames_from_ms(const struct synth *synth,int ms);
void synth_apply_pan(float *triml,float *trimr,float trim,float pan);

struct synth_pcm *synth_begin_print(struct synth *synth,const void *v,int c); // => STRONG

#endif
