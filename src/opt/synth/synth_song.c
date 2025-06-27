#include "synth_internal.h"

/* Delete.
 */
 
void synth_song_del(struct synth_song *song) {
  if (!song) return;
  free(song);
}

/* Add channel from decoded CHDR.
 */
 
static int synth_song_add_channel(
  struct synth_song *song,
  uint8_t chid,uint8_t trim,uint8_t pan,uint8_t mode,
  const uint8_t *modecfg,int modecfgc,
  const uint8_t *post,int postc
) {
  fprintf(stderr,"%s:%d:%s: TODO chid=%d\n",__FILE__,__LINE__,__func__,chid);//TODO
  return 0;
}

/* Receive CHDR chunk.
 */
 
static int synth_song_decode_CHDR(struct synth_song *song,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  uint8_t chid=src[0],trim=0x40,pan=0x80,mode=0;
  if (chid>=0x10) return 0; // High chid are legal but there's nothing for us to do with them.
  int srcp=1;
  if (srcp<srcc) trim=src[srcp++];
  if (srcp<srcc) pan=src[srcp++];
  if (srcp<srcc) mode=src[srcp++];
  const void *modecfg=0,*post=0;
  int modecfgc=0,postc=0;
  if (srcp<srcc-2) {
    modecfgc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-modecfgc) return -1;
    modecfg=src+srcp;
    srcp+=modecfgc;
  }
  if (srcp<srcc-2) {
    postc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-postc) return -1;
    post=src+srcp;
    srcp+=postc;
  }
  return synth_song_add_channel(song,chid,trim,pan,mode,modecfg,modecfgc,post,postc);
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
  song->terminated=1;
  song->delay=INT_MAX; // Prevent further event activity, if we're the current song.
  //TODO Release all notes.
}

/* Playhead.
 */

float synth_song_get_playhead(const struct synth_song *song) {
  return (double)song->phframes/(double)song->synth->rate;
}

void synth_song_set_playhead(struct synth_song *song,float s) {
  //TODO Release all notes.
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
  fprintf(stderr,"TODO %s chid=%d noteid=%d velocity=%.03f durframes=%d\n",__func__,chid,noteid,velocity,durframes);//TODO
}

void synth_song_wheel(struct synth_song *song,uint8_t chid,int v) {
  if (song->terminated) return;
  fprintf(stderr,"TODO %s chid=%d v=%d\n",__func__,chid,v);//TODO
}
