#include "opt/stdlib/egg-stdlib.h"
#include "eau.h"

/* Split file.
 */
 
int eau_file_decode(struct eau_file *file,const void *src,int srcc) {
  if (!file||!src||(srcc<8)||memcmp(src,"\0EAU",4)) return -1;
  const uint8_t *SRC=src;
  if (!(file->tempo=(SRC[4]<<8)|SRC[5])) return -1;
  file->loopp=(SRC[6]<<8)|SRC[7];
  int srcp=8;
  file->chhdrv=SRC+srcp;
  while (srcp<srcc) {
    if (SRC[srcp]==0xff) { srcp++; break; }
    if (srcp>srcc-8) return -1;
    srcp+=4;
    int len=(SRC[srcp]<<8)|SRC[srcp+1]; // payload...
    srcp+=2;
    if (srcp>srcc-len-2) return -1;
    srcp+=len;
    len=(SRC[srcp]<<8)|SRC[srcp+1]; // post...
    srcp+=2;
    if (srcp>srcc-len) return -1;
    srcp+=len;
  }
  file->chhdrc=srcp-8;
  file->evtv=SRC+srcp;
  file->evtc=srcc-srcp;
  return 0;
}

/* Next channel header.
 */

int eau_channel_reader_next(struct eau_channel_entry *entry,struct eau_channel_reader *reader) {
  if (reader->p>=reader->c) return 0;
  if (reader->p>reader->c-8) return -1;
  entry->chid=reader->v[reader->p++];
  entry->trim=reader->v[reader->p++];
  entry->pan=reader->v[reader->p++];
  entry->mode=reader->v[reader->p++];
  entry->payloadc=(reader->v[reader->p]<<8)|reader->v[reader->p+1];
  reader->p+=2;
  if (reader->p>reader->c-entry->payloadc) return -1;
  entry->payload=reader->v+reader->p;
  reader->p+=entry->payloadc;
  if (reader->p>reader->c-2) return -1;
  entry->postc=(reader->v[reader->p]<<8)|reader->v[reader->p+1];
  reader->p+=2;
  if (reader->p>reader->c-entry->postc) return -1;
  entry->post=reader->v+reader->p;
  reader->p+=entry->postc;
  return 1;
}

/* Next event.
 */
 
int eau_event_decode(struct eau_event *event,const void *src,int srcc) {
  if (!src||(srcc<1)) return -1;
  const uint8_t *SRC=src;
  uint8_t lead=SRC[0];
  if (!lead) return 0; // Explicit EOF.
  if (!(lead&0x80)) {
    event->type='d';
    event->delay=lead;
    return 1;
  }
  switch (lead&0xf0) {
    case 0x80: { // LONG DELAY
        event->type='d';
        event->delay=((lead&0x0f)+1)<<7;
        return 1;
      }
    case 0x90: // SHORT NOTE
    case 0xa0: // MEDIUM NOTE
    case 0xb0: { // LONG NOTE
        if (srcc<3) return -1;
        event->type='n';
        event->chid=lead&0x0f;
        event->noteid=SRC[1]>>1;
        event->velocity=((SRC[1]&0x01)<<3)|(SRC[2]>>5);
        event->delay=(SRC[2]&0x1f);
        switch (lead&0xf0) {
          case 0x90: break;
          case 0xa0: event->delay=(event->delay+1)<<5; break;
          case 0xb0: event->delay=(event->delay+1)<<10; break;
        }
        return 3;
      }
    case 0xc0: { // WHEEL
        if (srcc<2) return -1;
        event->type='w';
        event->chid=lead&0x0f;
        event->velocity=SRC[1];
        return 2;
      }
  }
  // Anything else is Reserved and illegal.
  return -1;
}

/* Get the longest possible note duration for a channel header.
 * It's in two pieces: (pre) before sustain and (post) after sustain.
 * Non-sustaining channels must leave (post) zero.
 */
 
static void eau_estimate_channel_duration(int *pre,int *post,uint8_t mode,const uint8_t *src,int srcc) {
  switch (mode) {
    // Drums are a pain. We have to measure each of them and report the longest. There is never sustain.
    case EAU_CHANNEL_MODE_DRUM: {
        int srcp=0;
        for (;;) {
          if (srcp>srcc-6) break;
          srcp+=4;
          int paylen=(src[srcp]<<8)|src[srcp+1];
          srcp+=2;
          if (srcp>srcc-paylen) break;
          int q=eau_estimate_duration(src+srcp,paylen);
          if (q>*pre) *pre=q;
          srcp+=paylen;
        }
      } break;
    
    // All tuned modes begin with their level envelope. (I arranged it like that, specifically to facilitate this case).
    case EAU_CHANNEL_MODE_FM:
    case EAU_CHANNEL_MODE_SUB: {
        int srcp=0;
        if (srcp>=srcc) {
          // Empty envelope is legal, for a default. See synth_env.c:synth_env_default().
          *pre=40;
          *post=125;
          return;
        }
        uint8_t flags=src[srcp++];
        if (flags&2) { // Initials
          srcp+=2;
          if (flags&1) { // Velocity
            srcp+=2;
          }
        }
        int susp=256;
        if (flags&4) { // Sustain
          if (srcp>=srcc) return;
          susp=src[srcp++];
        }
        if (srcp>=srcc) return;
        int ptc=src[srcp++];
        // We're going to take the longer option at each leg.
        // That is of course not correct, but our job is to find the worst case, overestimating is ok.
        int ptlen=(flags&1)?8:4;
        if (srcp>srcc-ptlen*ptc) return;
        int pti=0; for (;pti<ptc;pti++) {
          int legdur=(src[srcp]<<8)|src[srcp+1];
          srcp+=4;
          if (flags&1) {
            int q=(src[srcp]<<8)|src[srcp+1];
            srcp+=4;
            if (q>legdur) legdur=q;
          }
          if (pti<=susp) (*pre)+=legdur;
          else (*post)+=legdur;
        }
        // (post) zero signals No Sustain. But in theory, it could sustain and stop.
        // That would not be ok acoustically, but the format allows it.
        // So if we're sustainable but don't have any post length, bump it to one.
        if ((flags&4)&&!*post) *post=1;
      } break;
  }
}

/* Estimate duration.
 */
 
int eau_estimate_duration(const void *src,int srcc) {

  /* Record the longest possible duration for each channel.
   * If it doesn't sustain, (postdur) will be zero and (predur) the whole thing.
   */
  struct chinfo {
    int predur,postdur;
  } chinfov[16]={0};
  struct eau_file file;
  if (eau_file_decode(&file,src,srcc)<0) return -1;
  struct eau_channel_reader reader={.v=file.chhdrv,.c=file.chhdrc};
  struct eau_channel_entry channel;
  while (eau_channel_reader_next(&channel,&reader)>0) {
    if (channel.chid>=0x10) continue;
    eau_estimate_channel_duration(&chinfov[channel.chid].predur,&chinfov[channel.chid].postdur,channel.mode,channel.payload,channel.payloadc);
  }
  
  /* Walk events and track current time, and the high waterline for note end times.
   */
  int now=0,noteend=0,evtp=0,err;
  while (evtp<file.evtc) {
    struct eau_event event;
    if ((err=eau_event_decode(&event,file.evtv+evtp,file.evtc-evtp))<1) break;
    evtp+=err;
    switch (event.type) {
      case 'd': now+=event.delay; break;
      case 'n': {
          const struct chinfo *chinfo=chinfov+event.chid;
          int end=now;
          if (chinfo->postdur) {
            if (chinfo->predur>event.delay) end+=chinfo->predur;
            else end+=event.delay;
            end+=chinfo->postdur;
          } else {
            end+=chinfo->predur;
          }
          if (end>noteend) noteend=end;
        } break;
    }
  }
  if (noteend>now) now=noteend;
  
  return now;
}
