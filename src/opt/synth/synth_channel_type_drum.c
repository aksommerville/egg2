/* synth_channel_type_drum.c
 * Generates and plays PCM dumps per noteid.
 * Unlike most channel modes, we normally operate in stereo.
 */
 
#include "synth_internal.h"

// No cleanup necessary; pcmplay is all weak.
struct synth_voice_drum {
  struct synth_pcmplay pcmplay;
};

struct synth_drum {
  float trimlo,trimhi,pan;
  struct synth_pcm *pcm;
  const void *serial; // WEAK, points into original song.
  int serialc;
};

struct synth_channel_drum {
  struct synth_channel hdr;
  struct synth_voice_drum *voicev;
  int voicec,voicea;
  struct synth_drum drumv[128];
};

#define CHANNEL ((struct synth_channel_drum*)channel)

/* Cleanup.
 */
 
static void _drum_del(struct synth_channel *channel) {
  if (CHANNEL->voicev) synth_free(CHANNEL->voicev);
  struct synth_drum *drum=CHANNEL->drumv;
  int i=128;
  for (;i-->0;drum++) {
    if (drum->pcm) synth_pcm_del(drum->pcm);
  }
}

/* Update.
 */
 
static void _drum_update_mono(float *dst,struct synth_channel *channel,int framec) {
  int i=CHANNEL->voicec;
  struct synth_voice_drum *voice=CHANNEL->voicev+i-1;
  for (;i-->0;voice--) {
    if (synth_pcmplay_update(dst,0,&voice->pcmplay,framec)<=0) {
      CHANNEL->voicec--;
      __builtin_memmove(voice,voice+1,sizeof(struct synth_voice_drum)*(CHANNEL->voicec-i));
    }
  }
}

static void _drum_update_stereo(float *dstl,float *dstr,struct synth_channel *channel,int framec) {
  int i=CHANNEL->voicec;
  struct synth_voice_drum *voice=CHANNEL->voicev+i-1;
  for (;i-->0;voice--) {
    if (synth_pcmplay_update(dstl,dstr,&voice->pcmplay,framec)<=0) {
      CHANNEL->voicec--;
      __builtin_memmove(voice,voice+1,sizeof(struct synth_voice_drum)*(CHANNEL->voicec-i));
    }
  }
}

/* Initialize.
 */
 
static int _drum_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return -1;
    uint8_t noteid=src[srcp++];
    uint8_t trimlo=src[srcp++];
    uint8_t trimhi=src[srcp++];
    uint8_t pan=src[srcp++];
    int serialc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-serialc) return -1;
    const void *serial=src+srcp;
    srcp+=serialc;
    if (noteid&0x80) continue;
    if (!serialc) continue; // Don't bother tracking empties.
    struct synth_drum *drum=CHANNEL->drumv+noteid;
    if (drum->serialc) continue; // Duplicate. Illegal but whatever. Keep the first.
    drum->trimlo=(float)trimlo/255.0f;
    drum->trimhi=(float)trimhi/255.0f;
    drum->pan=(float)(pan-0x80)/128.0f;
    drum->serial=serial;
    drum->serialc=serialc;
  }
  channel->update_mono=_drum_update_mono;
  channel->update_stereo=_drum_update_stereo;
  return 0;
}

/* Begin note.
 */

static void _drum_note_on(struct synth_channel *channel,uint8_t noteid,float velocity) {
  if (noteid&0x80) return;
  struct synth_drum *drum=CHANNEL->drumv+noteid;
  if (!drum->serialc) return;
  
  // Ask synth to print it if we don't have it yet.
  // Exactly the same as synth_play_sound(). If it fails, try to create one sample of silence, and don't actually play those.
  if (!drum->pcm) {
    if (!(drum->pcm=synth_begin_print(drum->serial,drum->serialc))) {
      if (!(drum->pcm=synth_pcm_new(1))) return;
    }
  }
  if (drum->pcm->c<=1) return;
  
  // Acquire a voice. We keep the voice list packed.
  if (CHANNEL->voicec>=CHANNEL->voicea) {
    int na=CHANNEL->voicea+16;
    if (na>INT_MAX/sizeof(struct synth_voice_drum)) return;
    void *nv=synth_realloc(CHANNEL->voicev,sizeof(struct synth_voice_drum)*na);
    if (!nv) return;
    CHANNEL->voicev=nv;
    CHANNEL->voicea=na;
  }
  struct synth_voice_drum *voice=CHANNEL->voicev+CHANNEL->voicec++;
  
  // Determine trim, and start playing.
  float trim=drum->trimlo*(1.0f-velocity)+drum->trimhi*velocity;
  if (synth_pcmplay_init(&voice->pcmplay,drum->pcm,trim,drum->pan)<0) {
    CHANNEL->voicec--;
  }
}

/* Type definition.
 */
 
const struct synth_channel_type synth_channel_type_drum={
  .name="drum",
  .objlen=sizeof(struct synth_channel_drum),
  .del=_drum_del,
  .init=_drum_init,
  .note_on=_drum_note_on,
};
