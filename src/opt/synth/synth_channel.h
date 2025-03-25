/* synth_channel.h
 * Tuned signal generator, multiple voices.
 */
 
#ifndef SYNTH_CHANNEL_H
#define SYNTH_CHANNEL_H

/* Generic channel.
 ************************************************************************************/

struct synth_channel {
  uint8_t chid;
  uint8_t mode;
  float gainl,gainr;
  struct synth *synth; // WEAK
  int ttl; // If >0, we'll report termination after so many frames.
  int chanc;
  void (*xfer)(float *dst,const float *src,int framec,struct synth_channel *channel); // Owned by the wrapper, don't touch.
  
  void (*del)(struct synth_channel *channel);
  
  /* Must use the channel count stored here.
   * Controller is free to change that count at init, but only to 1 or 2. It's 1 by default, regardless of the context.
   * Must overwrite (v) entirely. Most implementations should start with a memset.
   * Do not apply trim, pan, or post.
   * REQUIRED.
   */
  void (*update)(float *v,int framec,struct synth_channel *channel);
};

void synth_channel_del(struct synth_channel *channel);
struct synth_channel *synth_channel_new(struct synth *synth,const struct eau_channel_entry *src);

/* Channel guarantees to report termination during a near future update.
 * Exact timing is not constrained.
 */
void synth_channel_terminate(struct synth_channel *channel);

/* Adds finished signal to (v), and returns >0 if still running. No errors.
 * (framec) must be <=SYNTH_UPDATE_LIMIT_FRAMES.
 * (v) has the context's channel count, not necessarily (channel)'s.
 */
int synth_channel_update(float *v,int framec,struct synth_channel *channel);

/* Events straight off the song.
 */
void synth_channel_note(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes);
void synth_channel_wheel(struct synth_channel *channel,uint8_t v);

/* Specific implementations.
 ***********************************************************************************/
 
struct synth_channel_drum {
  struct synth_channel hdr;
};

int synth_channel_drum_init(struct synth_channel *channel,const uint8_t *src,int srcc);
void synth_channel_drum_terminate(struct synth_channel *channel);
void synth_channel_drum_note(struct synth_channel *channel,uint8_t noteid,float velocity);
 
struct synth_fm_voice {
  uint32_t p;
  uint32_t dp;
  uint32_t modp;
  uint32_t moddp;
  struct synth_env level; // runner
  struct synth_env pitchenv;
  struct synth_env rangeenv;
};
struct synth_channel_fm {
  struct synth_channel hdr;
  struct synth_wave *sine; // If FM in play.
  struct synth_wave *carrier;
  struct synth_env level; // config
  struct synth_env pitchenv; // config, value in cents -32k..32k
  int wheelrange; // cents
  float modrate;
  float modrange; // baked into (rangeenv)
  struct synth_env rangeenv;
  float lforate;
  float lfodepth; // 0..1
  uint8_t use_pitchenv,use_moddp,use_rangeenv,use_lfo;
  
  // Selected at init.
  void (*voice_update)(float *v,int c,struct synth_fm_voice *voice,struct synth_channel *channel);
  
  struct synth_fm_voice *voicev;
  int voicec,voicea;
  
  struct synth_wave *lfo;
  uint32_t lfop;
  uint32_t lfodp;
  float *lfobuf;
};

int synth_channel_fm_init(struct synth_channel *channel,const uint8_t *src,int srcc);
void synth_channel_fm_terminate(struct synth_channel *channel);
void synth_channel_fm_note(struct synth_channel *channel,uint8_t noteid,float velocity,int dur);
void synth_channel_fm_wheel(struct synth_channel *channel,uint8_t v);

struct synth_channel_sub {
  struct synth_channel hdr;
};

int synth_channel_sub_init(struct synth_channel *channel,const uint8_t *src,int srcc);
void synth_channel_sub_terminate(struct synth_channel *channel);
void synth_channel_sub_note(struct synth_channel *channel,uint8_t noteid,float velocity,int dur);

#endif
