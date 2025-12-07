/* synth_channel.c
 * Generic channel layer.
 * We manage trim, pan, and post.
 * We massage events for dispatch to the typed implementation.
 * We have no concept of voices or notes; that belongs to the implementation.
 */

#include "synth_internal.h"

/* Type registry.
 */
 
const struct synth_channel_type *synth_channel_type_for_mode(uint8_t mode) {
  switch (mode) {
    case 0x01: return &synth_channel_type_trivial;
    case 0x02: return &synth_channel_type_fm;
    case 0x03: return &synth_channel_type_sub;
    case 0x04: return &synth_channel_type_drum;
  }
  return 0;
}

/* Delete.
 */
 
void synth_channel_del(struct synth_channel *channel) {
  if (!channel) return;
  if (channel->type->del) channel->type->del(channel);
  synth_pipe_del(channel->post);
  if (channel->bufl) synth_free(channel->bufl);
  if (channel->bufr) synth_free(channel->bufr);
  synth_free(channel);
}

/* New.
 */
 
struct synth_channel *synth_channel_new(
  struct synth_song *owner,
  uint8_t chid,uint8_t trim,uint8_t pan,uint8_t mode,
  const uint8_t *modecfg,int modecfgc,
  const uint8_t *post,int postc
) {
  if (!owner) return 0;
  const struct synth_channel_type *type=synth_channel_type_for_mode(mode);
  if (!type||!type->init) return 0;
  struct synth_channel *channel=synth_calloc(1,type->objlen);
  if (!channel) return 0;
  channel->type=type;
  channel->chid=chid; // Advisory only.
  channel->mode=mode;
  channel->song=owner;
  synth_channel_set_trim(channel,channel->trim0=(float)trim/255.0f);
  synth_channel_set_pan(channel,(float)(pan-0x80)/127.0f);
  if ((type->init(channel,modecfg,modecfgc)<0)||!channel->update_mono) {
    synth_channel_del(channel);
    return 0;
  }
  if (postc) {
    if (!(channel->post=synth_pipe_new(owner,post,postc))) {
      synth_channel_del(channel);
      return 0;
    }
  }
  if (!(channel->bufl=synth_malloc(sizeof(float)*synth.buffer_frames))) {
    synth_channel_del(channel);
    return 0;
  }
  if (owner->chanc>=2) {
    if (!(channel->bufr=synth_malloc(sizeof(float)*synth.buffer_frames))) {
      synth_channel_del(channel);
      return 0;
    }
  }
  return channel;
}

/* Set trim, pan, and wheel, via public properties.
 * Trim and pan apply the owning song's too.
 * This wheel setter is also triggered by song events.
 */
 
void synth_channel_set_trim(struct synth_channel *channel,float trim) {
  if (trim>1.0f) trim=1.0f; // Ensure we can't exceed song's trim.
  trim*=channel->song->trim;
  if (trim<0.0f) trim=0.0f;
  channel->trim=trim;
}

void synth_channel_set_pan(struct synth_channel *channel,float pan) {
  pan+=channel->song->pan;
  if (pan<=-1.0f) channel->pan=-1.0f;
  else if (pan>=1.0f) channel->pan=1.0f;
  else channel->pan=pan;
}

void synth_channel_set_wheel(struct synth_channel *channel,float v) {
  if (v<=-1.0f) channel->wheelf=-1.0f;
  else if (v>=1.0f) channel->wheelf=1.0f;
  else channel->wheelf=v;
  if (channel->type->wheel_changed) channel->type->wheel_changed(channel);
}

/* Release All.
 */
 
void synth_channel_release_all(struct synth_channel *channel) {
  if (channel->type->release_all) channel->type->release_all(channel);
}

/* Note Off.
 */
 
void synth_channel_note_off(struct synth_channel *channel,uint8_t noteid,uint8_t velocity) {
  if (channel->defunct) return;
  // (velocity) is not used.
  if (channel->type->note_off) {
    channel->type->note_off(channel,noteid);
  }
}

/* Note On.
 */
 
void synth_channel_note_on(struct synth_channel *channel,uint8_t noteid,uint8_t velocity) {
  if (channel->defunct) return;
  float fvel;
  if (velocity<=0x00) fvel=0.0f;
  else if (velocity>=0x7f) fvel=1.0f;
  else fvel=(float)velocity/127.0f;
  if (channel->type->note_on) {
    channel->type->note_on(channel,noteid,fvel);
  } else if (channel->type->note_once) {
    channel->type->note_once(channel,noteid,fvel,0);
  }
}

/* Note Once.
 */
 
void synth_channel_note_once(struct synth_channel *channel,uint8_t noteid,uint8_t velocity,int durms) {
  if (channel->defunct) return;
  float fvel;
  if (velocity<=0x00) fvel=0.0f;
  else if (velocity>=0x7f) fvel=1.0f;
  else fvel=(float)velocity/127.0f;
  if (channel->type->note_once) {
    int durframes=synth_frames_from_ms(durms);
    channel->type->note_once(channel,noteid,fvel,durframes);
  } else if (channel->type->note_on) {
    channel->type->note_on(channel,noteid,fvel);
    if (channel->type->note_off) {
      channel->type->note_off(channel,noteid);
    }
  }
}

/* Begin fade-out.
 */

void synth_channel_fade_out(struct synth_channel *channel,int framec) {
  if (channel->defunct) return;
  if (channel->fadeout>0.0f) return; // Already fading out, let it ride.
  if (framec<1) framec=1;
  channel->fadeout=1.0f;
  channel->fadeoutd=-1.0f/(float)framec;
}

/* Update.
 */

void synth_channel_update_stereo(float *dstl,float *dstr,struct synth_channel *channel,int framec) {
  if (channel->defunct) return;

  // Zero buffers.
  if (!channel->bufr) return;
  __builtin_memset(channel->bufl,0,sizeof(float)*framec);
  __builtin_memset(channel->bufr,0,sizeof(float)*framec);
  
  // Generate the full-level signal, and if mono, expand to stereo. Do not apply trim yet.
  if (channel->update_stereo) {
    channel->update_stereo(channel->bufl,channel->bufr,channel,framec);
  } else {
    channel->update_mono(channel->bufl,channel,framec);
    float *lp=channel->bufl,*rp=channel->bufr;
    int i=framec;
    if (channel->pan<0.0f) {
      float trimr=1.0f+channel->pan;
      for (;i-->0;lp++,rp++) *rp=(*lp)*trimr;
    } else if (channel->pan>0.0f) {
      float triml=1.0f-channel->pan;
      for (;i-->0;lp++,rp++) {
        *rp=*lp;
        (*lp)*=triml;
      }
    } else {
      for (;i-->0;lp++,rp++) *rp=*lp;
    }
  }
  
  // Run post.
  if (channel->post) {
    synth_pipe_update_stereo(channel->bufl,channel->bufr,channel->post,framec);
  }
  
  // If fading out, apply that.
  if (channel->fadeout>0.0f) {
    float *vl=channel->bufl,*vr=channel->bufr;
    int i=framec;
    for (;i-->0;vl++,vr++) {
      (*vl)*=channel->fadeout;
      (*vr)*=channel->fadeout;
      if ((channel->fadeout+=channel->fadeoutd)<=0.0f) {
        channel->fadeout=0.0f;
        channel->defunct=1;
      }
    }
  }
  
  // Apply trim and add to output.
  const float *srcl=channel->bufl,*srcr=channel->bufr;
  int i=framec;
  for (;i-->0;srcl++,srcr++,dstl++,dstr++) {
    (*dstl)+=(*srcl)*channel->trim;
    (*dstr)+=(*srcr)*channel->trim;
  }
}

void synth_channel_update_mono(float *dst,struct synth_channel *channel,int framec) {
  if (channel->defunct) return;

  // Generate the initial signal.
  __builtin_memset(channel->bufl,0,sizeof(float)*framec);
  channel->update_mono(channel->bufl,channel,framec);
  
  // Run post.
  if (channel->post) {
    synth_pipe_update_mono(channel->bufl,channel->post,framec);
  }
  
  // If fading out, apply that.
  if (channel->fadeout>0.0f) {
    float *vl=channel->bufl;
    int i=framec;
    for (;i-->0;vl++) {
      (*vl)*=channel->fadeout;
      if ((channel->fadeout+=channel->fadeoutd)<=0.0f) {
        channel->fadeout=0.0f;
        channel->defunct=1;
      }
    }
  }
  
  // Apply trim and add to output.
  const float *src=channel->bufl;
  int i=framec;
  for (;i-->0;dst++,src++) (*dst)+=(*src)*channel->trim;
}
