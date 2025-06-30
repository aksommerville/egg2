#include "synth_internal.h"

/* Delete.
 */
 
void synth_song_del(struct synth_song *song) {
  if (!song) return;
  if (song->channelv) {
    while (song->channelc-->0) synth_channel_del(song->channelv[song->channelc]);
    free(song->channelv);
  }
  free(song);
}

/* Receive CHDR chunk.
 */
 
static int synth_song_decode_CHDR(struct synth_song *song,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (src[0]>=0x10) return 0; // Ignore unaddressable channels.
  if (song->channel_by_chid[src[0]]) return 0; // Duplicate chid. Illegal but allow it, whatever.
  if (song->channelc>=song->channela) {
    int na=song->channela+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(song->channelv,sizeof(void*)*na);
    if (!nv) return -1;
    song->channelv=nv;
    song->channela=na;
  }
  struct synth_channel *channel=synth_channel_new(song->synth,song->chanc,song->tempo,src,srcc);
  if (!channel) return -1;
  if (channel->mode==0) { // MODE_NOOP, either naturally or faked by the channel decoder, we can drop it.
    synth_channel_del(channel);
    return 0;
  }
  song->channelv[song->channelc++]=channel;
  song->channel_by_chid[src[0]]=channel;
  return 0;
}

/* Decode new song.
 */
 
static int synth_song_decode(struct synth_song *song,const uint8_t *src,int srcc) {
  if (!src) return -1;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const uint8_t *chunkid=src+srcp;
    int chunklen=(src[srcp+4]<<24)|(src[srcp+5]<<16)|(src[srcp+6]<<8)|src[srcp+7];
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const uint8_t *chunk=src+srcp;
    srcp+=chunklen;
    
    if (!memcmp(chunkid,"\0EAU",4)) {
      if (chunklen<2) {
        song->tempo=synth_frames_from_ms(song->synth,500);
        song->loopp=0;
      } else {
        if ((song->tempo=synth_frames_from_ms(song->synth,(chunk[0]<<8)|chunk[1]))<1) return -1;
        if (chunklen<4) {
          song->loopp=0;
        } else {
          song->loopp=(chunk[2]<<8)|chunk[3];
        }
      }
      
    } else if (!memcmp(chunkid,"CHDR",4)) {
      if (!song->tempo) return -1; // "\0EAU" required before "CHDR".
      if (synth_song_decode_CHDR(song,chunk,chunklen)<0) return -1;
      
    } else if (!memcmp(chunkid,"EVTS",4)) {
      if (song->v) return -1; // Multiple EVTS.
      song->v=chunk;
      song->c=chunklen;
    }
  }
  if (!song->c) { // An empty song is technically legal. Replace it with a substantial delay.
    song->v="\x7f"; // 4096 ms
    song->c=1;
  }
  if (!song->tempo) return -1; // No "\0EAU"
  if ((song->loopp<0)||(song->loopp>song->c)) return -1; // Invalid loop point.
  return 0;
}

/* New.
 */
 
struct synth_song *synth_song_new(struct synth *synth,const void *v,int c,int repeat,int chanc) {
  if (!synth) return 0;
  struct synth_song *song=calloc(1,sizeof(struct synth_song));
  if (!song) return 0;
  song->synth=synth;
  song->repeat=repeat;
  song->chanc=chanc;
  if (synth_song_decode(song,v,c)<0) {
    synth_song_del(song);
    return 0;
  }
  return song;
}

/* Terminate.
 */
 
void synth_song_terminate(struct synth_song *song) {
  if (song->terminated) return;
  song->fade=1.0f;
  song->dfade=-1.0f/(SYNTH_SONG_FADE_TIME*(float)song->synth->rate);
  song->terminated=1;
  song->delay=INT_MAX; // Prevent further event activity, if we're the current song.
  memset(song->channel_by_chid,0,sizeof(song->channel_by_chid)); // Another way to prevent further events.
  int i=song->channelc;
  while (i-->0) synth_channel_release_all(song->channelv[i]);
}

/* Playhead.
 */

float synth_song_get_playhead(const struct synth_song *song) {
  return (double)song->phframes/(double)song->synth->rate;
}

void synth_song_set_playhead(struct synth_song *song,float s) {
  int i=song->channelc;
  while (i-->0) synth_channel_release_all(song->channelv[i]);
  int ms=(int)(s*1000.0f);
  song->delay=0;
  song->p=0;
  song->phframes=0;
  /* Read events, discarding real ones, until (song->delay) exceeds (ms) or we reach EOS.
   * Add all delay events into (song->delay), in ms, not frames.
   */
  while (song->delay<ms) {
    if (song->p>=song->c) break;
    uint8_t lead=song->v[song->p++];
    switch (lead&0xc0) {
      case 0x00: song->delay+=lead; break;
      case 0x40: song->delay+=((lead&0x3f)+1)<<6; break;
      case 0x80: song->p+=3; break;
      case 0xc0: song->p+=1; break;
    }
  }
  if (song->delay<ms) { // Didn't reach the requested time. Park here at EOS.
    song->phframes=synth_frames_from_ms(song->synth,song->delay);
    song->delay=0;
  } else { // Set playhead to exactly (ms), subtract that much from (delay), and convert to frames.
    song->phframes=synth_frames_from_ms(song->synth,ms);
    song->delay-=ms;
    song->delay=synth_frames_from_ms(song->synth,song->delay);
  }
}

int synth_song_frames_for_bytes(const struct synth_song *song,int dstp) {
  int p=0,ms=0;
  while (p<dstp) {
    if (p>=song->c) break;
    uint8_t lead=song->v[p++];
    switch (lead&0xc0) {
      case 0x00: ms+=lead; break;
      case 0x40: ms+=((lead&0x3f)+1)<<6; break;
      case 0x80: p+=3; break;
      case 0xc0: p+=1; break;
    }
  }
  return synth_frames_from_ms(song->synth,ms);
}

int synth_song_measure_frames(const struct synth_song *song) {
  return synth_song_frames_for_bytes(song,song->c);
}

/* Receive events.
 */

void synth_song_note(struct synth_song *song,uint8_t chid,uint8_t noteid,float velocity,int durframes) {
  if (song->terminated) return;
  if (noteid&0x80) return;
  if (chid&0xf0) return;
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return; //TODO Spec mandates a default.
  synth_channel_note(channel,noteid,velocity,durframes);
}

void synth_song_wheel(struct synth_song *song,uint8_t chid,int v) {
  if (song->terminated) return;
  if (chid&0xf0) return;
  struct synth_channel *channel=song->channel_by_chid[chid];
  if (!channel) return; //TODO Spec mandates a default.
  synth_channel_wheel(channel,v);
}

/* Update song, signal only.
 * Add to (v).
 */
  
static void synth_song_update_signal(float *v,int framec,struct synth_song *song) {
  if ((song->delay-=framec)<0) song->delay=0;
  song->phframes+=framec;
  
  /* When fading out, we painstakingly adjust trims and update channels one frame at a time.
   * Once fade reaches zero, we're done.
   * Could be done more efficiently if we implemented in channel, but that feels risky to me.
   */
  if (song->terminated) {
    for (;framec-->0;v+=song->chanc) {
      if ((song->fade+=song->dfade)<=0.0f) return;
      struct synth_channel **p=song->channelv;
      int i=song->channelc;
      for (;i-->0;p++) {
        struct synth_channel *channel=*p;
        float l0=channel->triml;
        float r0=channel->trimr;
        channel->triml*=song->fade;
        channel->trimr*=song->fade;
        synth_channel_update(v,1,*p);
        channel->triml=l0;
        channel->trimr=r0;
      }
    }
  
  } else {
    struct synth_channel **p=song->channelv;
    int i=song->channelc;
    for (;i-->0;p++) {
      synth_channel_update(v,framec,*p);
    }
  }
}

/* Update song, events only.
 * Caller provides a positive (limit) in frames. We return 1..limit, or <0 to bail out.
 * This generally will set (delay) and will never advance time.
 */
 
static int synth_song_update_events(struct synth_song *song,int limit) {
  for (;;) {
  
    if (song->delay>0) {
      if (song->delay<limit) return song->delay;
      return limit;
    }
    
    if (song->p>=song->c) {
      if (song->repeat) {
        if (song->loopp) {
          song->p=song->loopp;
          song->phframes=synth_song_frames_for_bytes(song,song->loopp);
        } else {
          song->p=0;
          song->phframes=0;
        }
        song->delay=1;
      } else {
        synth_song_terminate(song);
      }
      return song->delay;
    }
    
    uint8_t lead=song->v[song->p++];
    if (!(lead&0x80)) { // Both delay events have the high bit unset, and no others do. Collect multiple delays.
      for (;;) {
        if (lead&0x40) song->delay+=synth_frames_from_ms(song->synth,((lead&0x3f)+1)<<6);
        else song->delay+=synth_frames_from_ms(song->synth,lead);
        if ((song->p>=song->c)||(song->v[song->p]&0x80)) {
          if (song->delay<limit) return song->delay;
          return limit;
        }
        lead=song->v[song->p++];
      }
    }
    switch (lead&0xc0) {
      case 0x80: {
          if (song->p>song->c-3) return -1;
          uint8_t a=song->v[song->p++];
          uint8_t b=song->v[song->p++];
          uint8_t c=song->v[song->p++];
          uint8_t chid=(lead>>2)&15;
          uint8_t noteid=((lead&3)<<5)|(a>>3);
          float velocity=(((a&7)<<4)|(b>>4))/127.0f;
          int dur=synth_frames_from_ms(song->synth,(((b&15)<<8)|c)<<2);
          synth_song_note(song,chid,noteid,velocity,dur);
        } break;
      case 0xc0: {
          if (song->p>song->c-1) return -1;
          int w=song->v[song->p++];
          w|=(lead&3)<<8;
          w-=512;
          uint8_t chid=(lead>>2)&15;
          synth_song_wheel(song,chid,w);
        } break;
    }
  }
}

/* Update song, main entry point.
 * Add to (v).
 * Return >0 to stay alive.
 */
  
int synth_song_update(float *v,int framec,struct synth_song *song) {
  if (!song) return 0;
  while (framec>0) {
    int updframec=synth_song_update_events(song,framec);
    if (updframec<=0) return -1;
    if (updframec>framec) updframec=framec;
    synth_song_update_signal(v,updframec,song);
    framec-=updframec;
    v+=updframec*song->chanc;
  }
  if (song->terminated&&(song->fade<=0.0f)) return 0;
  return 1;
}
