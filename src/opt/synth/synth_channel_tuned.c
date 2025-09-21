/* synth_channel_tuned.c
 * Modes FM, HARSH, and HARM, which are all pretty alike.
 */

#include "synth_internal.h"

#define SYNTH_CHANNEL_VOICE_LIMIT 16

struct synth_tuned_extra {
  struct synth_wave *wave;
  const float *wavefv; // Either (wave->v) or (synth->sine), borrowed wave.
  int playseq;
  float *lfobuf;
  uint32_t lfop,lfodp;
  float lfoscale,lfobias;
  int wheeli; // Most recent value from the bus. -512..511
  float wheelm; // Multiplier.

  int modabs; // boolean
  float modrate; // multiplier; relative only
  uint32_t moddp; // fixed rate; absolute only
  struct synth_env levelenv;
  struct synth_env rangeenv; // Gets (modrange) baked in at decode.
  struct synth_env pitchenv;
  int wheelrange; // cents
  
  // One voice object that suits all three modes. The modes may use different bits. No cleanup.
  struct synth_voice {
    int playseq; // Lower values are older.
    int holdid;
    struct synth_env levelenv; // all
    struct synth_env rangeenv; // fm_basic,fm_full
    struct synth_env pitchenv; // wave_bend,fm_full
    uint32_t carp,cardp,cardp_base; // all. (cardp_base) is the note's pitch, without wheel.
    uint32_t modp; // fm_basic,fm_full
  } voicev[SYNTH_CHANNEL_VOICE_LIMIT];
  int voicec;
};

#define EXTRA ((struct synth_tuned_extra*)channel->extra)

/* Cleanup.
 */
 
static void synth_channel_cleanup_tuned(struct synth_channel *channel) {
  if (!channel->extra) return;
  synth_wave_del(EXTRA->wave);
  if (EXTRA->lfobuf) free(EXTRA->lfobuf);
  free(channel->extra);
}

/* The channel's own update hook is always basically the same thing.
 * We do make separate copies of it for each voice mode. (the alternative would be yet another function pointer dereference).
 */
 
#define CHUPDATE(name) \
  static void synth_channel_update_##name(float *v,int c,struct synth_channel *channel) { \
    struct synth_voice *voice=EXTRA->voicev; \
    int i=EXTRA->voicec; \
    for (;i-->0;voice++) { \
      if (voice->levelenv.finished) continue; \
      synth_voice_update_##name(v,c,voice,channel); \
    } \
    while (EXTRA->voicec&&EXTRA->voicev[EXTRA->voicec-1].levelenv.finished) EXTRA->voicec--; \
  }

/* Update from stored wave with no modulation or bend envelope.
 */
 
static void synth_voice_update_wave(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  for (;c-->0;v++) {
    voice->carp+=voice->cardp;
    float sample=EXTRA->wavefv[voice->carp>>SYNTH_WAVE_SHIFT];
    sample*=synth_env_update(&voice->levelenv);
    (*v)+=sample;
  }
}

CHUPDATE(wave)

/* Update from stored wave with pitch envelope.
 */
 
static void synth_voice_update_wave_bend(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  for (;c-->0;v++) {
  
    float cents=synth_env_update(&voice->pitchenv);
    uint32_t dp=voice->cardp;
    dp=(int32_t)((float)dp*powf(2.0f,cents/1200.0f));
    
    voice->carp+=dp;
    float sample=EXTRA->wavefv[voice->carp>>SYNTH_WAVE_SHIFT];
    sample*=synth_env_update(&voice->levelenv);
    (*v)+=sample;
  }
}

CHUPDATE(wave_bend)

/* FM with optional range envelope.
 * Relative modulator only.
 */
 
static void synth_voice_update_fm_basic(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  uint32_t moddp=(uint32_t)((float)voice->cardp*EXTRA->modrate);
  for (;c-->0;v++) {
    float sample=EXTRA->wavefv[voice->carp>>SYNTH_WAVE_SHIFT];
    float level=synth_env_update(&voice->levelenv);
    (*v)+=sample*level;
  
    uint32_t dp=voice->cardp;
    
    float mod=EXTRA->wavefv[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=moddp;
    float range=synth_env_update(&voice->rangeenv);
    dp=(int32_t)((float)dp+(float)dp*mod*range);
    voice->carp+=dp;
  }
}

CHUPDATE(fm_basic)

/* FM with absolute modulator and range envelope. No LFO or pitch env.
 */
 
static void synth_voice_update_fm_abs(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  for (;c-->0;v++) {
    float sample=EXTRA->wavefv[voice->carp>>SYNTH_WAVE_SHIFT];
    (*v)+=sample*synth_env_update(&voice->levelenv);
  
    uint32_t dp=voice->cardp;
    
    float mod=EXTRA->wavefv[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=EXTRA->moddp;
    float range=synth_env_update(&voice->rangeenv);
    dp=(int32_t)((float)dp+(float)dp*mod*range);
    voice->carp+=dp;
  }
}

CHUPDATE(fm_abs)

/* FM with every bell and whistle.
 * (in particular, LFO and pitch env always get this implementation).
 */
 
static void synth_voice_update_fm_full(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  const float *lfo=EXTRA->lfobuf;
  for (;c-->0;v++,lfo++) {
  
    float cents=synth_env_update(&voice->pitchenv);
    uint32_t dp=voice->cardp;
    dp=(int32_t)((float)dp*powf(2.0f,cents/1200.0f));
    uint32_t moddp;
    if (EXTRA->moddp) moddp=EXTRA->moddp;
    else moddp=(int32_t)((float)dp*EXTRA->modrate);
    
    float sample=EXTRA->wavefv[voice->carp>>SYNTH_WAVE_SHIFT];
    (*v)+=sample*synth_env_update(&voice->levelenv);
    
    float mod=EXTRA->wavefv[voice->modp>>SYNTH_WAVE_SHIFT];
    voice->modp+=moddp;
    float range=synth_env_update(&voice->rangeenv)*(*lfo);
    
    dp=(int32_t)((float)dp+(float)dp*mod*range);
    voice->carp+=dp;
  }
}

static void synth_channel_update_fm_full(float *v,int c,struct synth_channel *channel) {

  float *dst=EXTRA->lfobuf;
  int i=c;
  for (;i-->0;dst++) {
    *dst=EXTRA->wavefv[EXTRA->lfop>>SYNTH_WAVE_SHIFT]*EXTRA->lfoscale+EXTRA->lfobias;
    EXTRA->lfop+=EXTRA->lfodp;
  }

  struct synth_voice *voice=EXTRA->voicev;
  for (i=EXTRA->voicec;i-->0;voice++) {
    if (voice->levelenv.finished) continue;
    synth_voice_update_fm_full(v,c,voice,channel);
  }
  while (EXTRA->voicec&&EXTRA->voicev[EXTRA->voicec-1].levelenv.finished) EXTRA->voicec--;
}

#undef CHUPDATE

/* Helpers for decoding fields.
 * You must have (const uint8_t *src,int srcc,int srcp,int err).
 * We'll update (srcp) and may return in error cases.
 * Initialize scalar fields first; we'll leave them untouched after EOF.
 * Envelopes will overwrite always.
 */
 
#define FLD_U8(dst) { \
  if (srcp>=srcc) ; \
  else (dst)=src[srcp++]; \
}
#define FLD_U16(dst) { \
  if (srcp>=srcc) ; \
  else if (srcp>srcc-2) return -1; \
  else { \
    (dst)=(src[srcp]<<8)|src[srcp+1]; \
    srcp+=2; \
  } \
}
#define FLD_ENV(dst,defname,scale,bias) { \
  if (srcp>=srcc) synth_env_default_##defname(&dst,channel->rate); \
  else if (srcp>srcc-2) return -1; \
  else if (!src[srcp]&&!src[srcp+1]) { \
    synth_env_default_##defname(&dst,channel->rate); \
    srcp+=2; \
  } else { \
    if ((err=synth_env_decode(&dst,src+srcp,srcc-srcp,channel->rate))<0) return err; \
    srcp+=err; \
    synth_env_scale(&dst,scale); \
    synth_env_bias(&dst,bias); \
  } \
}

/* Init mode FM.
 */
 
int synth_channel_init_FM(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  EXTRA->wavefv=channel->synth->sine; // FM only uses sine waves.
  
  // Decode serial. See etc/doc/eau-format.md.
  int srcp=0,err;
  int modrate=0,modrange=0; // Both u8.8, read initially as integers.
  EXTRA->wheelrange=200;
  EXTRA->wheelm=1.0f;
  int lforate=0,lfodepth=0xff,lfophase=0;
  FLD_U16(modrate)
  FLD_U16(modrange)
  FLD_ENV(EXTRA->levelenv,level,1.0f/65535.0f,0.0f)
  FLD_ENV(EXTRA->rangeenv,range,(float)modrange/(256.0f*65535.0f),0.0f)
  if (EXTRA->rangeenv.flags&SYNTH_ENV_FLAG_DEFAULT) synth_env_scale(&EXTRA->rangeenv,(float)modrange/256.0f);
  FLD_ENV(EXTRA->pitchenv,pitch,1.0f,-32768.0f)
  FLD_U16(EXTRA->wheelrange)
  FLD_U16(lforate)
  FLD_U8(lfodepth)
  FLD_U8(lfophase)
  
  /* Any of (modrate,modrange,rangeenv) can be zero to nix the FM.
   * Detecting zeroes in the envelope is a bit tricky so I won't bother.
   * Note that the high bit of (modrate) is the "absolute" flag; it doesn't matter here.
   * In theory, a zero-rate LFO with full depth and phase 3/4 also nixes it. Not touching that.
   */
  if (!(modrate&0x7fff)||!modrange) {
    modrate=modrange=0;
    lforate=lfodepth=lfophase=0;
  }
  
  // No modulation: We have a simple "wave" implementation, with or without pitch envelope.
  if (!modrate) {
    if (EXTRA->pitchenv.flags&SYNTH_ENV_FLAG_DEFAULT) {
      channel->update_mono=synth_channel_update_wave;
    } else {
      channel->update_mono=synth_channel_update_wave_bend;
    }
    return 0;
  }
  
  if (modrate&0x8000) {
    float period=((float)(modrate&0x7fff)*(float)channel->tempo)/256.0f;
    if (period<=0.0f) period=0.001f;
    EXTRA->moddp=(uint32_t)(4294967296.0f/period);
    if (!EXTRA->moddp) EXTRA->moddp=1;
  } else {
    EXTRA->modrate=(float)modrate/256.0f;
  }
  
  // LFO or pitch envelope in play, use the full bells and whistles.
  if (lforate||!(EXTRA->pitchenv.flags&SYNTH_ENV_FLAG_DEFAULT)) {
    if (!(EXTRA->lfobuf=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT))) return -1;
    EXTRA->lfop=lfophase; EXTRA->lfop|=EXTRA->lfop<<8; EXTRA->lfop|=EXTRA->lfop<<16;
    float period=((float)lforate*(float)channel->tempo)/256.0f;
    if (period<=0.0f) period=0.001f;
    EXTRA->lfodp=(uint32_t)(4294967296.0f/period);
    if (!EXTRA->lfodp) EXTRA->lfodp=1;
    EXTRA->lfoscale=(float)lfodepth/512.0f; // 8-bit but also cut in half
    EXTRA->lfobias=1.0f-EXTRA->lfoscale;
    channel->update_mono=synth_channel_update_fm_full;
    return 0;
  }
  
  // FM with optional range envelope. The modest implementation.
  if (modrate&0x8000) channel->update_mono=synth_channel_update_fm_abs;
  else channel->update_mono=synth_channel_update_fm_basic;
  return 0;
}

/* Init mode HARSH.
 */
 
int synth_channel_init_HARSH(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  
  // Decode serial.
  int srcp=0,err;
  uint8_t shape=0;
  EXTRA->wheelrange=200;
  EXTRA->wheelm=1.0f;
  FLD_U8(shape)
  FLD_ENV(EXTRA->levelenv,level,1.0f/65535.0f,0.0f)
  FLD_ENV(EXTRA->pitchenv,pitch,1.0f,-32768.0f)
  FLD_U16(EXTRA->wheelrange)
  
  // Acquire wave. Unknown shapes default to sine.
  switch (shape) {
    case 0: EXTRA->wavefv=channel->synth->sine; break;
    case 1: EXTRA->wave=synth_wave_new_square(); break;
    case 2: EXTRA->wave=synth_wave_new_saw(); break;
    case 3: EXTRA->wave=synth_wave_new_triangle(); break;
    default: EXTRA->wavefv=channel->synth->sine; break;
  }
  if (!EXTRA->wavefv) {
    if (!EXTRA->wave) return -1;
    EXTRA->wavefv=EXTRA->wave->v;
  }
  
  // Update hook is the same as the non-modulated edge case of FM.
  if (EXTRA->pitchenv.flags&SYNTH_ENV_FLAG_DEFAULT) {
    channel->update_mono=synth_channel_update_wave;
  } else {
    channel->update_mono=synth_channel_update_wave_bend;
  }
  return 0;
}

/* Init mode HARM.
 */
 
int synth_channel_init_HARM(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  
  // Decode serial.
  int srcp=0,err,harmc=0;
  EXTRA->wheelrange=200;
  EXTRA->wheelm=1.0f;
  FLD_U8(harmc)
  if (!harmc) {
    EXTRA->wavefv=channel->synth->sine;
  } else {
    if (srcp>srcc-harmc*2) return -1;
    float coefv[256];
    int i=0; for (;i<harmc;i++) {
      coefv[i]=((src[srcp]<<8)|src[srcp+1])/65535.0f;
      srcp+=2;
    }
    if (!(EXTRA->wave=synth_wave_new_harmonics(channel->synth->sine,coefv,harmc))) return -1;
    EXTRA->wavefv=EXTRA->wave->v;
  }
  FLD_ENV(EXTRA->levelenv,level,1.0f/65535.0f,0.0f)
  FLD_ENV(EXTRA->pitchenv,pitch,1.0f,-32768.0f)
  FLD_U16(EXTRA->wheelrange)
  
  // Update hook is the same as the non-modulated edge case of FM.
  if (EXTRA->pitchenv.flags&SYNTH_ENV_FLAG_DEFAULT) {
    channel->update_mono=synth_channel_update_wave;
  } else {
    channel->update_mono=synth_channel_update_wave_bend;
  }
  return 0;
}

#undef FLD_U8
#undef FLD_U16
#undef FLD_ENV

/* Play note.
 */
 
int synth_channel_note_tuned(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  
  struct synth_voice *voice;
  if (EXTRA->voicec<SYNTH_CHANNEL_VOICE_LIMIT) {
    voice=EXTRA->voicev+EXTRA->voicec++;
  } else {
    voice=EXTRA->voicev;
    struct synth_voice *q=voice;
    int i=SYNTH_CHANNEL_VOICE_LIMIT;
    for (;i-->0;q++) {
      if (q->levelenv.finished) {
        voice=q;
        break;
      }
      if (q->playseq>voice->playseq) voice=q; // Prefer to evict older notes.
    }
  }
  
  voice->playseq=EXTRA->playseq++;
  synth_env_apply(&voice->levelenv,&EXTRA->levelenv,velocity,durframes);
  synth_env_apply(&voice->rangeenv,&EXTRA->rangeenv,velocity,durframes);
  synth_env_apply(&voice->pitchenv,&EXTRA->pitchenv,velocity,durframes);
  voice->carp=0;
  voice->cardp_base=channel->synth->rateiv[noteid];
  voice->cardp=(uint32_t)((float)voice->cardp_base*EXTRA->wheelm);
  voice->modp=0;
  return voice->holdid=synth_holdid_next(channel->synth);
}

/* Adjust wheel.
 */
 
void synth_channel_wheel_tuned(struct synth_channel *channel,int v) {
  if (!EXTRA->wheelrange) return;
  if (v==EXTRA->wheeli) return;
  EXTRA->wheeli=v;
  float cents=((float)v*(float)EXTRA->wheelrange)/512.0f;
  EXTRA->wheelm=powf(2.0f,cents/1200.0f);
  struct synth_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) {
    voice->cardp=(uint32_t)((float)voice->cardp_base*EXTRA->wheelm);
  }
}

/* Release one note.
 */
 
void synth_channel_release_one_tuned(struct synth_channel *channel,int holdid) {
  struct synth_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) {
    if (voice->holdid!=holdid) continue;
    synth_env_release(&voice->levelenv);
    synth_env_release(&voice->rangeenv);
    synth_env_release(&voice->pitchenv);
  }
}

/* Release all notes.
 */
 
void synth_channel_release_tuned(struct synth_channel *channel) {
  struct synth_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) {
    synth_env_release(&voice->levelenv);
    synth_env_release(&voice->rangeenv);
    synth_env_release(&voice->pitchenv);
  }
}
