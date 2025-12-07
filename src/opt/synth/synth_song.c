#include "synth_internal.h"

/* Delete.
 */
 
void synth_song_del(struct synth_song *song) {
  if (!song) return;
  if (song->channelv) {
    while (song->channelc-->0) synth_channel_del(song->channelv[song->channelc]);
    synth_free(song->channelv);
  }
  synth_free(song);
}

/* Initialize one channel, during init.
 */
 
static int synth_song_initialize_channel(
  struct synth_song *song,
  uint8_t chid,uint8_t trim,uint8_t pan,uint8_t mode,
  const uint8_t *modecfg,int modecfgc,
  const uint8_t *post,int postc
) {
  if (chid>=16) return 0; // High chid are perfectly legal but meaningless to us.
  if (song->channel_by_chid[chid]) return -1; // Duplicate chid is an error.
  if (!mode) return 0; // NOOP, no sense doing any bookkeeping on it.
  if (song->channelc>=song->channela) {
    int na=song->channela+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=synth_realloc(song->channelv,sizeof(void*)*na);
    if (!nv) return -1;
    song->channelv=nv;
    song->channela=na;
  }
  struct synth_channel *channel=synth_channel_new(song,chid,trim,pan,mode,modecfg,modecfgc,post,postc);
  if (!channel) return -1;
  song->channelv[song->channelc++]=channel;
  song->channel_by_chid[chid]=channel;
  return 0;
}

/* Decode channel headers, during init.
 */
 
static int synth_song_decode_chhdr(struct synth_song *song,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return -1;
    uint8_t chid=src[srcp++];
    uint8_t trim=src[srcp++];
    uint8_t pan=src[srcp++];
    uint8_t mode=src[srcp++];
    int modecfglen=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-modecfglen) return -1;
    const uint8_t *modecfg=src+srcp;
    srcp+=modecfglen;
    if (srcp>srcc-2) return -1;
    int postlen=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-postlen) return -1;
    const uint8_t *post=src+srcp;
    srcp+=postlen;
    if (synth_song_initialize_channel(song,chid,trim,pan,mode,modecfg,modecfglen,post,postlen)<0) return -1;
  }
  return 0;
}

/* Decode fresh song.
 */
 
static int synth_song_decode(struct synth_song *song,const uint8_t *src,int srcc) {
  if (!src||(srcc<10)||__builtin_memcmp(src,"\0EAU",4)) return -1;
  int tempo=(src[4]<<8)|src[5];
  if (tempo<1) tempo=1;
  song->tempo=(float)tempo/1000.0f;
  int hdrlen=(src[6]<<24)|(src[7]<<16)|(src[8]<<8)|src[9];
  if ((hdrlen<0)||(10>srcc-hdrlen)) return -1;
  if (synth_song_decode_chhdr(song,src+10,hdrlen)<0) return -1;
  int srcp=10+hdrlen;
  if (srcp>srcc-4) return -1;
  int evtlen=(src[srcp]<<24)|(src[srcp+1]<<16)|(src[srcp+2]<<8)|src[srcp+3];
  srcp+=4;
  if ((evtlen<1)||(srcp>srcc-evtlen)) return -1;
  song->evtv=src+srcp;
  song->evtc=evtlen;
  return 0;
}

/* New.
 */
 
struct synth_song *synth_song_new(int chanc,const void *src,int srcc,float trim,float pan) {
  if (chanc<1) return 0;
  struct synth_song *song=synth_calloc(1,sizeof(struct synth_song));
  if (!song) return 0;
  song->chanc=chanc;
  song->trim=(trim<0.0f)?0.0f:(trim>1.0f)?1.0f:trim;
  song->pan=(pan<-1.0f)?-1.0f:(pan>1.0f)?1.0f:pan;
  if (synth_song_decode(song,src,srcc)<0) {
    synth_song_del(song);
    return 0;
  }
  return song;
}

/* Dispatch one event.
 * (chid) can't be 16 or greater.
 */
 
static void synth_song_note_off(struct synth_song *song,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return;
  synth_channel_note_off(channel,noteid,velocity);
}
 
static void synth_song_note_on(struct synth_song *song,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return;
  synth_channel_note_on(channel,noteid,velocity);
}
 
static void synth_song_note_once(struct synth_song *song,uint8_t chid,uint8_t noteid,uint8_t velocity,int durms) {
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return;
  synth_channel_note_once(channel,noteid,velocity,durms);
}
 
static void synth_song_wheel(struct synth_song *song,uint8_t chid,int v/*0..8192..16383*/) {
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return;
  float fv=(v-0x2000)/8192.0f;
  synth_channel_set_wheel(channel,fv);
}

/* Dispatch any events if ready, and pay out delays.
 * Returns the frame count until the next event but never more than (limit).
 * Whatever we return we will have just dropped from the delay.
 * Returns <=0 on errors or EOF.
 */
 
static int synth_song_update_events(struct synth_song *song,int limit) {
  for (;;) {
  
    // If we have a delay in progress, that trumps all.
    if (song->delay) {
      int updc=song->delay;
      if (updc>limit) updc=limit;
      song->delay-=updc;
      return updc;
    }
    
    /* End of stream, either schedule termination or repeat.
     * When repeating, report a very small artificial delay.
     * Without that, a maliciously crafted song (no delays) could put us in an infinite loop.
     */
    if (song->evtp>=song->evtc) {
      if (song->repeat) {
        song->evtp=song->loopp;
        song->phframes=song->loopframes;
        return 1;
      }
      synth_song_stop(song);
      return limit;
    }
    
    /* If the next event is a delay (high bit unset), collect multiple delays then report.
     * It is very likely to have multiple delay events back to back, since there are two different granularities.
     * There is such a thing as zero delay; we'll skip them.
     */
    int delay=0;
    while ((song->evtp<song->evtc)&&!(song->evtv[song->evtp]&0x80)) {
      int d1=song->evtv[song->evtp++];
      if (d1&0x40) d1=((d1&0x3f)+1)<<6; // Long Delay. (Short Delay is just ms verbatim)
      delay+=d1;
    }
    if (delay) {
      song->delay=synth_frames_from_ms(delay);
      continue;
    }
    if (song->evtp>=song->evtc) continue; // A zero delay could put us beyond EOF; go back to capa in that case.
    
    /* Read and dispatch a real event.
     * These are all distinguished by the top 4 bits.
     */
    uint8_t lead=song->evtv[song->evtp++];
    switch (lead&0xf0) {
      case 0x80: { // Note Off
          if (song->evtp>song->evtc-2) return -1;
          uint8_t noteid=song->evtv[song->evtp++];
          uint8_t velocity=song->evtv[song->evtp++];
          synth_song_note_off(song,lead&0x0f,noteid,velocity);
        } break;
      case 0x90: { // Note On
          if (song->evtp>song->evtc-2) return -1;
          uint8_t noteid=song->evtv[song->evtp++];
          uint8_t velocity=song->evtv[song->evtp++];
          synth_song_note_on(song,lead&0x0f,noteid,velocity);
        } break;
      case 0xa0: { // Note Once
          if (song->evtp>song->evtc-3) return -1;
          uint8_t a=song->evtv[song->evtp++];
          uint8_t b=song->evtv[song->evtp++];
          uint8_t c=song->evtv[song->evtp++];
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&1)<<6)|(b>>2);
          int durms=(((b&3)<<8)|c)<<4;
          synth_song_note_once(song,lead&0x0f,noteid,velocity,durms);
        } break;
      case 0xe0: { // Wheel
          if (song->evtp>song->evtc-2) return -1;
          uint8_t a=song->evtv[song->evtp++];
          uint8_t b=song->evtv[song->evtp++];
          synth_song_wheel(song,lead&0x0f,a|(b<<7));
        } break;
      case 0xf0: switch (lead&0x0f) { // Marker...
          case 0x00: song->loopp=song->evtp; song->loopframes=song->phframes; break; // Loop Point
          default: return -1;
        } break;
      default: return -1; // 0xb0,0xc0,0xd0 reserved and illegal.
    }
  }
}

/* Update.
 */
 
int synth_song_update(float *dstl,float *dstr,struct synth_song *song,int framec) {
  if (song->terminated) return 0;
  while (framec>0) {
  
    // Process ready events and capture time to next one.
    int updc=synth_song_update_events(song,framec);
    if (updc<=0) return updc;
    
    // Generate signal.
    struct synth_channel **p=song->channelv;
    int i=song->channelc;
    if (dstr&&(song->chanc>=2)) {
      for (;i-->0;p++) synth_channel_update_stereo(dstl,dstr,*p,updc);
      dstr+=updc;
    } else {
      for (;i-->0;p++) synth_channel_update_mono(dstl,*p,updc);
    }
    
    // Advance clocks.
    song->phframes+=updc;
    dstl+=updc;
    framec-=updc;
    if (song->deathclock) {
      if ((song->deathclock-=updc)<=0) {
        song->deathclock=0;
        song->terminated=1;
        return 0;
      }
    }
  }
  return song->terminated?0:1;
}

/* End playback gently.
 */
 
void synth_song_stop(struct synth_song *song) {
  if (song->terminated) return;
  if (song->deathclock) return;
  song->songid=0;
  int framec=(int)(SYNTH_FADEOUT_TIME_S*(float)synth.rate);
  if (framec<1) framec=1;
  song->deathclock=framec;
  int i=song->channelc;
  struct synth_channel **p=song->channelv;
  for (;i-->0;p++) synth_channel_fade_out(*p,framec);
}

/* Calculate duration.
 */
 
int synth_song_get_duration_frames(struct synth_song *song) {
  if (!song->durframes) {
    int p=0,ms=0;
    for (;p<song->evtc;) {
      uint8_t lead=song->evtv[p++];
      if (!(lead&0x80)) {
        if (lead&0x40) ms+=((lead&0x3f)+1)<<6;
        else ms+=lead;
      } else switch (lead&0xf0) {
        case 0x80: p+=2; break;
        case 0x90: p+=2; break;
        case 0xa0: p+=3; break;
        case 0xe0: p+=2; break;
        case 0xf0: break;
        default: return -1;
      }
    }
    song->durframes=synth_frames_from_ms(ms);
  }
  return song->durframes;
}

/* Get playhead.
 */

float synth_song_get_playhead(const struct synth_song *song) {
  return (float)song->phframes/(float)synth.rate;
}

/* Set playhead.
 */

void synth_song_set_playhead(struct synth_song *song,float s) {

  // Release held notes.
  struct synth_channel **p=song->channelv;
  int i=song->channelc;
  for (;i-->0;p++) synth_channel_release_all(*p);
  
  /* Take target position in ms, reset song's position to the start, and consume delays until we reach the target.
   * We will always land either at the very end, or right after a delay event, and we don't try to slice those delays to hit (s) exactly.
   * As our docs say, setting playhead is noticeably messy and should only be used coarsely, eg resume playback where you left off, not for quick precise movements.
   */
  int targetms=(int)(s*1000.0f);
  song->evtp=0;
  song->delay=0;
  song->phframes=0;
  int delaysum=0;
  while ((delaysum<targetms)&&(song->evtp<song->evtc)) {
    uint8_t lead=song->evtv[song->evtp++];
    if (!(lead&0x80)) {
      if (lead&0x40) delaysum+=((lead&0x3f)+1)<<6;
      else delaysum+=lead;
    } else switch (lead&0xf0) {
      case 0x80: song->evtp+=2; break;
      case 0x90: song->evtp+=2; break;
      case 0xa0: song->evtp+=3; break;
      case 0xe0: song->evtp+=2; break;
      case 0xf0: switch (lead&0x0f) {
          // Record the loop position, in case we haven't got it yet.
          case 0x00: song->loopp=song->evtp; song->loopframes=synth_frames_from_ms(delaysum); break;
        } break;
      default: return;
    }
  }
  // Granularity of frames calculation is different than when playing. I don't think it will matter.
  song->phframes=synth_frames_from_ms(delaysum);
}

/* Set trim or pan.
 */
 
void synth_song_set_trim(struct synth_song *song,float trim) {
  if (trim<=0.0f) song->trim=0.0f;
  else if (trim>=1.0f) song->trim=1.0f;
  else song->trim=trim;
  struct synth_channel **p=song->channelv;
  int i=song->channelc;
  for (;i-->0;p++) synth_channel_set_trim(*p,(*p)->trim0*song->trim);
}
 
void synth_song_set_pan(struct synth_song *song,float pan) {
  if (pan<=-1.0f) song->pan=-1.0f;
  else if (pan>=1.0f) song->pan=1.0f;
  else song->pan=pan;
  struct synth_channel **p=song->channelv;
  int i=song->channelc;
  for (;i-->0;p++) synth_channel_set_pan(*p,(*p)->pan);
}

/* Derive useful values from tempo.
 */
 
uint32_t synth_song_get_tempo_step(const struct synth_song *song,float qnotes) {
  float s=song->tempo*qnotes;
  if (s<=0.0f) return 0;
  return (uint32_t)(4294967296.0f/((float)synth.rate*s));
}

int synth_song_get_tempo_frames(const struct synth_song *song,float qnotes) {
  float s=song->tempo*qnotes;
  int framec=(int)(s*(float)synth.rate);
  if (framec<0) return 0;
  return framec;
}
