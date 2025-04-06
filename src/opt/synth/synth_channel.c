#include "synth_internal.h"

/* Delete.
 */
 
void synth_channel_del(struct synth_channel *channel) {
  if (!channel) return;
  if (channel->del) channel->del(channel);
  if (channel->stagev) {
    while (channel->stagec-->0) synth_stage_del(channel->stagev[channel->stagec]);
    free(channel->stagev);
  }
  free(channel);
}

/* Transfer functions.
 * These run after post.
 * Apply gain (and implicitly pan), and expand or condense to the master channel count.
 */
 
static void synth_channel_xfer_mono_mono(float *dst,const float *src,int framec,struct synth_channel *channel) {
  for (;framec-->0;dst++,src++) {
    (*dst)+=(*src)*channel->gainl;
  }
}
 
static void synth_channel_xfer_multi_mono(float *dst,const float *src,int framec,struct synth_channel *channel) {
  int dd=channel->synth->chanc;
  for (;framec-->0;dst+=dd,src++) {
    dst[0]+=(*src)*channel->gainl;
    dst[1]+=(*src)*channel->gainr;
  }
}
 
static void synth_channel_xfer_mono_stereo(float *dst,const float *src,int framec,struct synth_channel *channel) {
  for (;framec-->0;dst++,src+=2) {
    (*dst)+=(src[0]*channel->gainl+src[1]*channel->gainr)*0.5f;
  }
}
 
static void synth_channel_xfer_multi_stereo(float *dst,const float *src,int framec,struct synth_channel *channel) {
  int dd=channel->synth->chanc;
  for (;framec-->0;dst+=dd,src+=2) {
    dst[0]+=src[0]*channel->gainl;
    dst[1]+=src[1]*channel->gainr;
  }
}

/* Init post pipe.
 */
 
static int synth_channel_init_post(struct synth_channel *channel,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-2) return -1;
    uint8_t stageid=src[srcp++];
    uint8_t len=src[srcp++];
    if (srcp>srcc-len) return -1;
    if (channel->stagec>=channel->stagea) {
      int na=channel->stagea+8;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(channel->stagev,sizeof(void*)*na);
      if (!nv) return -1;
      channel->stagev=nv;
      channel->stagea=na;
    }
    struct synth_stage *stage=synth_stage_new(channel->synth,channel->chanc,stageid,src+srcp,len);
    if (!stage) return -1;
    srcp+=len;
    channel->stagev[channel->stagec++]=stage;
  }
  return 0;
}

/* New.
 */
 
struct synth_channel *synth_channel_new(struct synth *synth,const struct eau_channel_entry *src) {

  /* Allocate blank object of the appropriate size.
   */
  if (!synth||!src) return 0;
  struct synth_channel *channel=0;
  switch (src->mode) {
    case EAU_CHANNEL_MODE_DRUM: channel=calloc(1,sizeof(struct synth_channel_drum)); break;
    case EAU_CHANNEL_MODE_FM: channel=calloc(1,sizeof(struct synth_channel_fm)); break;
    case EAU_CHANNEL_MODE_SUB: channel=calloc(1,sizeof(struct synth_channel_sub)); break;
  }
  if (!channel) return 0;
  
  /* Prepare the basic context properties, importantly the gains.
   */
  channel->synth=synth;
  channel->mode=src->mode;
  channel->chid=src->chid;
  if (synth->chanc==1) {
    channel->gainl=channel->gainr=src->trim/255.0f;
  } else {
    float trim=src->trim/255.0f;
    float pan=(src->pan-0x80)/127.0f;
    if (pan<=-1.0f) {
      channel->gainl=trim;
      channel->gainr=0.0f;
    } else if (pan>=1.0f) {
      channel->gainl=0.0f;
      channel->gainr=trim;
    } else if (pan<0.0f) {
      channel->gainl=trim;
      channel->gainr=(1.0f+pan)*trim;
    } else if (pan>0.0f) {
      channel->gainl=(1.0f-pan)*trim;
      channel->gainr=trim;
    } else {
      channel->gainl=channel->gainr=trim;
    }
  }
  channel->chanc=1;
  
  /* Let the mode-specific controller do its thing.
   */
  fprintf(stderr,"%s: chid=%d mode=%d payloadc=%d\n",__func__,src->chid,src->mode,src->payloadc);
  int err=-1;
  switch (src->mode) {
    case EAU_CHANNEL_MODE_DRUM: err=synth_channel_drum_init(channel,src->payload,src->payloadc); break;
    case EAU_CHANNEL_MODE_FM: err=synth_channel_fm_init(channel,src->payload,src->payloadc); break;
    case EAU_CHANNEL_MODE_SUB: err=synth_channel_sub_init(channel,src->payload,src->payloadc); break;
  }
  if ((err<0)||!channel->update) {
    synth_channel_del(channel);
    return 0;
  }
  
  /* Channel controllers are free to change (chanc) but the only options are 1 and 2.
   */
  if ((channel->chanc!=1)&&(channel->chanc!=2)) {
    synth_channel_del(channel);
    return 0;
  }
  
  /* Stand the post pipe if we have one.
   */
  if (src->postc&&(synth_channel_init_post(channel,src->post,src->postc)<0)) {
    synth_channel_del(channel);
    return 0;
  }
  
  /* Select the appropriate transfer function based on channel counts.
   */
  if (channel->chanc==1) {
    if (synth->chanc==1) channel->xfer=synth_channel_xfer_mono_mono;
    else channel->xfer=synth_channel_xfer_multi_mono;
  } else {
    if (synth->chanc==1) channel->xfer=synth_channel_xfer_mono_stereo;
    else channel->xfer=synth_channel_xfer_multi_stereo;
  }
  
  return channel;
}

/* Terminate.
 */

void synth_channel_terminate(struct synth_channel *channel) {
  if (!channel) return;
  if (channel->ttl>0) return;
  // Make the channel globally unaddressable, if it's not already.
  if (channel->synth->channel_by_chid[channel->chid]==channel) {
    channel->synth->channel_by_chid[channel->chid]=0;
  }
  /* Let the specific controller do its thing and set its own ttl.
   * If it doesn't, set ttl to 1 and we'll wrap up next update.
   */
  switch (channel->mode) {
    case EAU_CHANNEL_MODE_DRUM: synth_channel_drum_terminate(channel); break;
    case EAU_CHANNEL_MODE_FM: synth_channel_fm_terminate(channel); break;
    case EAU_CHANNEL_MODE_SUB: synth_channel_sub_terminate(channel); break;
  }
  if (channel->ttl<=0) channel->ttl=1;
}

/* Update.
 */

int synth_channel_update(float *v,int framec,struct synth_channel *channel) {
  int result=1;
  if (channel->ttl>0) {
    if (channel->ttl<=framec) {
      result=0;
      framec=channel->ttl;
      channel->ttl=1;
    } else {
      channel->ttl-=framec;
    }
  }
  
  /* Let the controller update into a scratch buffer at full volume, and usually mono.
   */
  channel->update(channel->synth->scratch,framec,channel);
  
  /* Run post if there is one.
   */
  struct synth_stage **stage=channel->stagev;
  int i=channel->stagec;
  for (;i-->0;stage++) (*stage)->update(channel->synth->scratch,framec,*stage);
  
  /* Our transfer function manages copying to the output.
   */
  channel->xfer(v,channel->synth->scratch,framec,channel);
  
  return result;
}

/* Event dispatch.
 */
 
void synth_channel_note(struct synth_channel *channel,uint8_t noteid,float velocity,int durframes) {
  //fprintf(stderr,"%s %d noteid=0x%02x velocity=%f dur=%d\n",__func__,channel->chid,noteid,velocity,durframes);
  if (channel->ttl) return; // Don't start new notes if we're winding down.
  switch (channel->mode) {
    case EAU_CHANNEL_MODE_DRUM: synth_channel_drum_note(channel,noteid,velocity); break;
    case EAU_CHANNEL_MODE_FM: synth_channel_fm_note(channel,noteid,velocity,durframes); break;
    case EAU_CHANNEL_MODE_SUB: synth_channel_sub_note(channel,noteid,velocity,durframes); break;
  }
}

void synth_channel_wheel(struct synth_channel *channel,uint8_t v) {
  fprintf(stderr,"%s %d 0x%02x\n",__func__,channel->chid,v);
  switch (channel->mode) {
    case EAU_CHANNEL_MODE_FM: synth_channel_fm_wheel(channel,v); break;
  }
}
