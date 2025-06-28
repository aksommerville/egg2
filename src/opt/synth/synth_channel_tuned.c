/* synth_channel_tuned.c
 * Modes FM, HARSH, and HARM, which are all pretty alike.
 */

#include "synth_internal.h"

#define SYNTH_CHANNEL_VOICE_LIMIT 16

struct synth_tuned_extra {
  // One voice object that suits all three modes. The modes may use different bits.
  struct synth_voice {
    //XXX very simple synth for testing, all temporary
    int ttl;
    uint32_t p;
    uint32_t dp;
    float level;
  } voicev[SYNTH_CHANNEL_VOICE_LIMIT];
  int voicec;
};

#define EXTRA ((struct synth_tuned_extra*)channel->extra)

/* Cleanup.
 */
 
static void synth_channel_cleanup_tuned(struct synth_channel *channel) {
  if (!channel->extra) return;
  free(channel->extra);
}

/* Update HARSH. Mono.
 */
 
static void synth_voice_update_HARSH(float *v,int c,struct synth_voice *voice,struct synth_channel *channel) {
  for (;c-->0;v++) {
    if (--(voice->ttl)<=0) return;
    voice->p+=voice->dp;
    if (voice->p&0x80000000) (*v)+=voice->level;
    else (*v)-=voice->level;
  }
}
 
static void synth_channel_update_HARSH(float *v,int framec,struct synth_channel *channel) {
  struct synth_voice *voice=EXTRA->voicev;
  int i=EXTRA->voicec;
  for (;i-->0;voice++) {
    if (voice->ttl<1) continue;
    synth_voice_update_HARSH(v,framec,voice,channel);
  }
  while (EXTRA->voicec&&(EXTRA->voicev[EXTRA->voicec-1].ttl<1)) EXTRA->voicec--;
}

/* Init mode FM.
 */
 
int synth_channel_init_FM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc) {
  fprintf(stderr,"TODO %s chid=%d modecfgc=%d\n",__func__,channel->chid,modecfgc);//TODO
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  
  //TODO read modecfg
  
  channel->update_mono=synth_channel_update_HARSH;//XXX
  return 0;
}

/* Init mode HARSH.
 */
 
int synth_channel_init_HARSH(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc) {
  fprintf(stderr,"TODO %s chid=%d modecfgc=%d\n",__func__,channel->chid,modecfgc);//TODO
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  
  //TODO read modecfg
  
  channel->update_mono=synth_channel_update_HARSH;
  return 0;
}

/* Init mode HARM.
 */
 
int synth_channel_init_HARM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc) {
  fprintf(stderr,"TODO %s chid=%d modecfgc=%d\n",__func__,channel->chid,modecfgc);//TODO
  if (!(channel->extra=calloc(1,sizeof(struct synth_tuned_extra)))) return -1;
  channel->del=synth_channel_cleanup_tuned;
  
  //TODO read modecfg
  
  channel->update_mono=synth_channel_update_HARSH;//XXX
  return 0;
}

/* Play note.
 */
 
void synth_channel_note_tuned(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  
  struct synth_voice *voice;
  if (EXTRA->voicec<SYNTH_CHANNEL_VOICE_LIMIT) {
    voice=EXTRA->voicev+EXTRA->voicec++;
  } else {
    voice=EXTRA->voicev;
    struct synth_voice *q=voice;
    int i=SYNTH_CHANNEL_VOICE_LIMIT;
    for (;i-->0;q++) {
      if (q->ttl<voice->ttl) {
        voice=q;
        if (voice->ttl<1) break;
      }
    }
  }
  
  //XXX highly temporary
  voice->ttl=durframes+300;
  voice->p=0;
  voice->dp=channel->synth->rateiv[noteid];
  voice->level=0.100f+0.100f*velocity;
}

/* Adjust wheel.
 */
 
void synth_channel_wheel_tuned(struct synth_channel *channel,int v) {
  //TODO
}

/* Release all notes.
 */
 
void synth_channel_release_tuned(struct synth_channel *channel) {
  //TODO
}
