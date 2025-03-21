#include <stdint.h>
#include <string.h>
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
