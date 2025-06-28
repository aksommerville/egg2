#include "synth_internal.h"

#define SYNTH_DRUM_PCMPLAY_LIMIT 16

struct synth_drum_extra {
  struct drum_note {
    float trimlo,trimhi;
    float pan;
    struct synth_pcm *pcm; // STRONG, LAZY
    const void *src;
    int srcc;
  } notev[128];
  struct synth_pcmplay pcmplayv[SYNTH_DRUM_PCMPLAY_LIMIT];
  int pcmplayc;
};

#define EXTRA ((struct synth_drum_extra*)channel->extra)

/* Cleanup.
 */
 
static void synth_channel_cleanup_drum(struct synth_channel *channel) {
  if (!channel->extra) return;
  struct drum_note *note=EXTRA->notev;
  int i=128;
  for (;i-->0;note++) {
    if (note->pcm) synth_pcm_del(note->pcm);
  }
  struct synth_pcmplay *pcmplay=EXTRA->pcmplayv;
  for (i=EXTRA->pcmplayc;i-->0;pcmplay++) synth_pcmplay_cleanup(pcmplay);
  free(channel->extra);
}

/* Update.
 * We actually use the same hook for stereo and mono -- the pcmplay instance get the channel count at setup.
 */
 
static void synth_channel_update_drum(float *v,int c,struct synth_channel *channel) {
  int i=EXTRA->pcmplayc;
  struct synth_pcmplay *pcmplay=EXTRA->pcmplayv+i-1;
  for (;i-->0;pcmplay--) {
    if (synth_pcmplay_update(v,c,pcmplay)<=0) {
      synth_pcmplay_cleanup(pcmplay);
      EXTRA->pcmplayc--;
      memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(EXTRA->pcmplayc-i));
    }
  }
}

/* Initialize drum channel.
 * Set (mode=0) if we determine it's noop.
 */
 
int synth_channel_init_DRUM(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (!(channel->extra=calloc(1,sizeof(struct synth_drum_extra)))) return -1;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return -1;
    uint8_t noteid=src[srcp++];
    uint8_t trimlo=src[srcp++];
    uint8_t trimhi=src[srcp++];
    uint8_t pan=src[srcp++];
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return -1;
    const void *body=src+srcp;
    srcp+=len;
    if (noteid>=0x80) continue;
    struct drum_note *note=EXTRA->notev+noteid;
    note->trimlo=trimlo/255.0f;
    note->trimhi=trimhi/255.0f;
    note->pan=(pan-0x80)/128.0f;
    note->src=body;
    note->srcc=len;
  }
  channel->update_stereo=synth_channel_update_drum;
  channel->update_mono=synth_channel_update_drum;
  return 0;
}

/* Play note.
 */
 
void synth_channel_note_drum(struct synth_channel *channel,uint8_t noteid,float velocity) {
  if (noteid>=0x80) return;
  
  struct drum_note *note=EXTRA->notev+noteid;
  if (!note->pcm) {
    if (note->srcc<=1) return;
    if (!(note->pcm=synth_begin_print(channel->synth,note->src,note->srcc))) return;
  }
  
  struct synth_pcmplay *pcmplay;
  if (EXTRA->pcmplayc<SYNTH_DRUM_PCMPLAY_LIMIT) {
    pcmplay=EXTRA->pcmplayv+EXTRA->pcmplayc++;
  } else {
    pcmplay=EXTRA->pcmplayv;
    struct synth_pcmplay *q=pcmplay;
    int i=SYNTH_DRUM_PCMPLAY_LIMIT;
    for (;i-->0;q++) {
      if (!q->pcm) {
        pcmplay=q;
        break;
      }
      if (q->p>pcmplay->p) {
        pcmplay=q;
      }
    }
    synth_pcmplay_cleanup(pcmplay);
  }
  
  float trim;
  if (velocity<=0.0f) trim=note->trimlo;
  else if (velocity>=1.0f) trim=note->trimhi;
  else trim=note->trimlo*(1.0f-velocity)+note->trimhi*velocity;
  int chanc=channel->chanc;
  if (chanc>2) chanc=2;
  if (synth_pcmplay_init(pcmplay,note->pcm,chanc,trim,note->pan)<0) return;
}
