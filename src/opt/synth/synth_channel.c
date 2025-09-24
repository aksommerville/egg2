#include "synth_internal.h"

int synth_channel_init_DRUM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc);
int synth_channel_init_FM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc);
int synth_channel_init_HARSH(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc);
int synth_channel_init_HARM(struct synth_channel *channel,const uint8_t *modecfg,int modecfgc);
void synth_channel_note_drum(struct synth_channel *channel,uint8_t noteid,float velocity);
int synth_channel_note_tuned(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes);
void synth_channel_wheel_tuned(struct synth_channel *channel,int v);
void synth_channel_release_tuned(struct synth_channel *channel);
void synth_channel_release_one_tuned(struct synth_channel *channel,int holdid);

/* Delete.
 */
 
void synth_channel_del(struct synth_channel *channel) {
  if (!channel) return;
  if (channel->del) channel->del(channel);
  synth_pipe_del(channel->post);
  free(channel);
}

/* New.
 */

struct synth_channel *synth_channel_new(struct synth *synth,int chanc,int tempo,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  struct synth_channel *channel=calloc(1,sizeof(struct synth_channel));
  if (!channel) return 0;
  channel->synth=synth;
  channel->rate=synth->rate;
  channel->chanc=chanc;
  channel->tempo=tempo;
  
  // Read the front matter.
  channel->chid=src[0];
  uint8_t trim=0x40,pan=0x80;
  channel->mode=2;
  int srcp=1;
  if (srcp<srcc) {
    trim=src[srcp++];
    if (srcp<srcc) {
      pan=src[srcp++];
      if (srcp<srcc) {
        channel->mode=src[srcp++];
      }
    }
  }
  if (!channel->mode) return channel; // Mode still noop, we can return and the caller should drop it.
  if (!trim) { channel->mode=0; return channel; } // Trim zero, go noop and return. This would not be OK if trim is mutable, something I'm considering for the future.
  channel->pan=(pan-0x80)/127.0f;
  if (channel->chanc==1) channel->triml=channel->trimr=1.0f;
  else synth_apply_pan(&channel->triml,&channel->trimr,1.0f,channel->pan);
  channel->trim=trim/255.0f;
  
  // Split out modecfg and post.
  const void *modecfg=0,*postserial=0;
  int modecfgc=0,postserialc=0;
  if (srcp<=srcc-2) {
    modecfgc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-modecfgc) {
      synth_channel_del(channel);
      return 0;
    }
    modecfg=src+srcp;
    srcp+=modecfgc;
  }
  if (srcp<=srcc-2) {
    postserialc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-postserialc) {
      synth_channel_del(channel);
      return 0;
    }
    postserial=src+srcp;
    srcp+=postserialc;
  }
  
  // Mode-specific initialization.
  int err=0;
  switch (channel->mode) {
    case 1: err=synth_channel_init_DRUM(channel,modecfg,modecfgc); break;
    case 2: err=synth_channel_init_FM(channel,modecfg,modecfgc); break;
    case 3: err=synth_channel_init_HARSH(channel,modecfg,modecfgc); break;
    case 4: err=synth_channel_init_HARM(channel,modecfg,modecfgc); break;
    default: channel->mode=0; // Unknown modes are legal but noop.
  }
  if (err<0) {
    synth_channel_del(channel);
    return 0;
  }
  if (!channel->update_mono) channel->mode=0; // Voice mode is required to set update_mono.
  if (!channel->mode) return channel; // Mode-specific initializer may render us noop.
  if (channel->chanc==1) channel->update_stereo=0; // update_stereo must only be set if we're going to use it.
  
  // Prepare post.
  if (postserialc) {
    if (!(channel->post=synth_pipe_new(channel->synth,channel->chanc,channel->tempo,postserial,postserialc))) {
      synth_channel_del(channel);
      return 0;
    }
  }
  
  return channel;
}

/* Stereo from mono in place, with precalculated normal trims.
 */
 
static void synth_channel_expand_stereo(float *v,int framec,struct synth_channel *channel) {
  const float *src=v+framec;
  float *dst=v+framec*2;
  for (;framec-->0;) {
    src--;
    float sample=*src;
    dst--; *dst=sample*channel->trimr;
    dst--; *dst=sample*channel->triml;
  }
}

/* Update, outer.
 */

void synth_channel_update(float *v,int framec,struct synth_channel *channel) {
  
  // If we're stereo and the voice mode produces stereo, do that. (update_stereo) is never set when our output is mono.
  if (channel->update_stereo) {
    memset(channel->tmp,0,sizeof(float)*framec*2);
    channel->update_stereo(channel->tmp,framec,channel);
    
  // Otherwise, update voices mono and then expand to stereo if appropriate.
  } else {
    memset(channel->tmp,0,sizeof(float)*framec);
    channel->update_mono(channel->tmp,framec,channel);
    if (channel->chanc>1) {
      synth_channel_expand_stereo(channel->tmp,framec,channel);
    }
  }
  
  // Run post in (tmp).
  if (channel->post) {
    synth_pipe_update(channel->tmp,framec,channel->post);
  }
  
  // Trim and mix.
  float *dst=v;
  const float *src=channel->tmp;
  int i=framec;
  if (channel->chanc>1) {
    for (;i-->0;dst+=channel->chanc,src+=2) {
      dst[0]+=src[0]*channel->trim;
      dst[1]+=src[1]*channel->trim;
    }
  } else {
    for (;i-->0;dst++,src++) (*dst)+=(*src)*channel->trim;
  }
}

/* Note.
 */

int synth_channel_note(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  switch (channel->mode) {
    case 1: synth_channel_note_drum(channel,noteid,velocity); return 0; // Drums are not sustainable.
    case 2: case 3: case 4: return synth_channel_note_tuned(channel,noteid,velocity,durframes);
  }
  return 0;
}

/* Release.
 */
 
void synth_channel_release(struct synth_channel *channel,int holdid) {
  switch (channel->mode) {
    case 2: case 3: case 4: synth_channel_release_one_tuned(channel,holdid); break;
  }
}

/* Wheel.
 */
 
void synth_channel_wheel(struct synth_channel *channel,int v) {
  switch (channel->mode) {
    case 2: case 3: case 4: synth_channel_wheel_tuned(channel,v); break;
  }
}

/* Release all notes.
 */
 
void synth_channel_release_all(struct synth_channel *channel) {
  switch (channel->mode) {
    case 2: case 3: case 4: synth_channel_release_tuned(channel); break;
  }
}
