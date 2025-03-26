#include "synth_internal.h"

#define CHANNEL ((struct synth_channel_drum*)channel)

/* Cleanup.
 */
 
static void synth_drum_note_cleanup(struct synth_drum_note *note) {
  synth_pcm_del(note->pcm);
}
 
static void _drum_del(struct synth_channel *channel) {
  if (CHANNEL->notev) {
    while (CHANNEL->notec-->0) synth_drum_note_cleanup(CHANNEL->notev+CHANNEL->notec);
    free(CHANNEL->notev);
  }
  while (CHANNEL->pcmplayc-->0) synth_pcmplay_cleanup(CHANNEL->pcmplayv+CHANNEL->pcmplayc);
}

/* Update.
 */
 
static void synth_channel_drum_update(float *v,int c,struct synth_channel *channel) {
  memset(v,0,sizeof(float)*c*channel->chanc);
  int i=CHANNEL->pcmplayc;
  while (i-->0) {
    struct synth_pcmplay *pcmplay=CHANNEL->pcmplayv+i;
    if (synth_pcmplay_update(v,c,pcmplay)<1) {
      synth_pcmplay_cleanup(pcmplay);
      CHANNEL->pcmplayc--;
      memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(CHANNEL->pcmplayc-i));
    }
  }
}

/* Fetch or add a note.
 */
 
static struct synth_drum_note *synth_channel_drum_get_note(struct synth_channel *channel,uint8_t noteid,int add) {
  if ((noteid>=CHANNEL->noteid0)&&(noteid<CHANNEL->noteid0+CHANNEL->notea)) return CHANNEL->notev+noteid-CHANNEL->noteid0;
  // Could reject here if (noteid&0x80) but I'm going to allow that because who knows what we might do in the future.
  if (!add) return 0;
  int np=CHANNEL->noteid0;
  if (!CHANNEL->notec) np=noteid;
  if (noteid<np) np=noteid&~0xf;
  int nz=CHANNEL->noteid0+CHANNEL->notec;
  if (nz<=noteid) nz=(noteid+0x10)&~0xf;
  int nc=nz-np;
  if (nc>CHANNEL->notea) {
    void *nv=realloc(CHANNEL->notev,sizeof(struct synth_drum_note)*nc);
    if (!nv) return 0;
    CHANNEL->notev=nv;
    CHANNEL->notea=nc;
  }
  if (np<CHANNEL->noteid0) {
    int d=CHANNEL->noteid0-np;
    memmove(CHANNEL->notev+d,CHANNEL->notev,sizeof(struct synth_drum_note)*CHANNEL->notec);
    CHANNEL->noteid0=np;
  } else if (np>CHANNEL->noteid0) {
    if (CHANNEL->notec) return 0;
    CHANNEL->noteid0=np;
  }
  if (CHANNEL->notec<nc) {
    memset(CHANNEL->notev+CHANNEL->notec,0,sizeof(struct synth_drum_note)*(nc-CHANNEL->notec));
    CHANNEL->notec=nc;
  }
  return CHANNEL->notev+noteid-CHANNEL->noteid0;
}

/* Decode and register one note.
 * Return length consumed.
 */
 
static int synth_channel_drum_add_note(struct synth_channel *channel,const uint8_t *src,int srcc) {
  if (srcc<6) return -1;
  uint8_t noteid=src[0];
  int len=(src[4]<<8)|src[5];
  if (6+len>srcc) return -1;
  if (!len) return 6; // Empty body, ignore it.
  if (!src[1]&&!src[2]) return 6+len; // Both trims zero, ignore it.
  struct synth_drum_note *note=synth_channel_drum_get_note(channel,noteid,1);
  if (!note) return -1;
  note->trimlo=src[1]/255.0f;
  note->trimhi=src[2]/255.0f;
  note->pan=src[3]/255.0f;
  note->src=src+6;
  note->srcc=len;
  return 6+len;
}

/* Init.
 */
 
int synth_channel_drum_init(struct synth_channel *channel,const uint8_t *src,int srcc) {
  channel->del=_drum_del;
  // We can do mono or stereo, and in fact we don't care which, except right here:
  if (channel->synth->chanc==1) {
    channel->chanc=1;
  } else {
    channel->chanc=2;
  }
  channel->update=synth_channel_drum_update;
  int srcp=0;
  while (srcp<srcc) {
    int err=synth_channel_drum_add_note(channel,src+srcp,srcc-srcp);
    if (err<1) {
      synth_channel_del(channel);
      return -1;
    }
    srcp+=err;
  }
  return 0;
}

/* Terminate.
 */
 
void synth_channel_drum_terminate(struct synth_channel *channel) {
  const struct synth_pcmplay *pcmplay=CHANNEL->pcmplayv;
  int i=CHANNEL->pcmplayc;
  for (;i-->0;pcmplay++) {
    if (!pcmplay->pcm) continue;
    int ttl=pcmplay->pcm->c-pcmplay->p;
    if (ttl>channel->ttl) channel->ttl=ttl;
  }
  if (channel->ttl<1) channel->ttl=1;
}

/* Get a pcmplay from a new note.
 * If none is available, we'll evict something old.
 */
 
static struct synth_pcmplay *synth_channel_drum_get_pcmplay(struct synth_channel *channel) {
  struct synth_pcmplay *pcmplay;
  if (CHANNEL->pcmplayc<SYNTH_DRUM_PCMPLAY_LIMIT) {
    pcmplay=CHANNEL->pcmplayv+CHANNEL->pcmplayc++;
    memset(pcmplay,0,sizeof(struct synth_pcmplay));
    return pcmplay;
  }
  pcmplay=CHANNEL->pcmplayv;
  struct synth_pcmplay *q=pcmplay;
  int i=SYNTH_DRUM_PCMPLAY_LIMIT;
  for (;i-->0;q++) {
    if (q->p>pcmplay->p) pcmplay=q;
  }
  synth_pcmplay_cleanup(pcmplay);
  return pcmplay;
}

/* Note.
 */
 
void synth_channel_drum_note(struct synth_channel *channel,uint8_t noteid,float velocity) {
  struct synth_drum_note *note=synth_channel_drum_get_note(channel,noteid,0);
  if (!note||!note->srcc) {
    if (!note->warned) {
      //fprintf(stderr,"  note 0x%02x {\n    0 0x80 0x80 fm {\n    }\n    events {\n      note 0 0x%02x 15 200\n    }\n  }\n",noteid,noteid);
      fprintf(stderr,"WARNING: Drum 0x%02x on channel %d not found.\n",noteid,channel->chid);
      note->warned=1;
    }
    return;
  }
  if (!note->pcm) {
    if (!(note->pcm=synth_begin_print(channel->synth,note->src,note->srcc))) {
      note->srcc=0;
      return;
    }
  }
  struct synth_pcmplay *pcmplay=synth_channel_drum_get_pcmplay(channel);
  if (!pcmplay) return;
  double trim=note->trimlo*(1.0f-velocity)+note->trimhi*velocity;
  synth_pcmplay_setup(pcmplay,channel->chanc,note->pcm,trim,note->pan);
}
