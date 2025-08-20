#include "eau.h"
#include <string.h>
#include <limits.h>

/* Advance file reader.
 */
 
int eau_file_reader_next(struct eau_file_chunk *chunk,struct eau_file_reader *reader) {
  if (reader->p>=reader->c) return 0;
  if (reader->p>reader->c-8) return -1;
  const uint8_t *SRC=reader->v;
  memcpy(chunk->id,SRC+reader->p,4);
  chunk->c=(SRC[reader->p+4]<<24)|(SRC[reader->p+5]<<16)|(SRC[reader->p+6]<<8)|SRC[reader->p+7];
  reader->p+=8;
  if ((chunk->c<0)||(reader->p>reader->c-chunk->c)) return -1;
  chunk->v=SRC+reader->p;
  reader->p+=chunk->c;
  return 1;
}

/* Decode channel header.
 */

int eau_file_channel_decode(struct eau_file_channel *channel,const void *src,int srcc) {
  memset(channel,0,sizeof(struct eau_file_channel));
  const uint8_t *SRC=src;
  int srcp=0;
  if (srcp>=srcc) channel->chid=0; else channel->chid=SRC[srcp++];
  if (srcp>=srcc) channel->trim=0x40; else channel->trim=SRC[srcp++];
  if (srcp>=srcc) channel->pan=0x80; else channel->pan=SRC[srcp++];
  if (srcp>=srcc) channel->mode=2; else channel->mode=SRC[srcp++];
  if (srcp>=srcc) {
    channel->modecfg=0;
    channel->modecfgc=0;
  } else if (srcp>srcc-2) {
    return -1;
  } else {
    channel->modecfgc=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (srcp>srcc-channel->modecfgc) return -1;
    channel->modecfg=SRC+srcp;
    srcp+=channel->modecfgc;
  }
  if (srcp>=srcc) {
    channel->post=0;
    channel->postc=0;
  } else if (srcp>srcc-2) {
    return -1;
  } else {
    channel->postc=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (srcp>srcc-channel->postc) return -1;
    channel->post=SRC+srcp;
    srcp+=channel->postc;
  }
  return 0;
}

/* Decode "\0EAU" chunk.
 */
 
static int eau_file_decode_header(struct eau_file *file,const uint8_t *src,int srcc) {
  if (file->tempo) return -1; // Multiple "\0EAU'.
  int srcp=0;
  if (srcp>=srcc) file->tempo=500;
  else if (srcp>srcc-2) return -1;
  else {
    if (!(file->tempo=(src[srcp]<<8)|src[srcp+1])) return -1;
    srcp+=2;
  }
  if (srcp>=srcc) file->loopp=0;
  else if (srcp>srcc-2) return -1;
  else {
    file->loopp=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
  }
  return 0;
}

/* Decode file.
 */

int eau_file_decode(struct eau_file *file,const void *src,int srcc) {
  memset(file,0,sizeof(struct eau_file));
  struct eau_file_reader reader={.v=src,.c=srcc};
  struct eau_file_chunk chunk;
  for (;;) {
    int err=eau_file_reader_next(&chunk,&reader);
    if (err<0) return err;
    if (!err) break;
    if (!memcmp(chunk.id,"\0EAU",4)) {
      if (eau_file_decode_header(file,chunk.v,chunk.c)<0) return -1;
    } else if (!memcmp(chunk.id,"CHDR",4)) {
      if (chunk.c<1) return -1;
      uint8_t chid=((uint8_t*)chunk.v)[0];
      if (chid<16) {
        if (eau_file_channel_decode(file->channelv+chid,chunk.v,chunk.c)<0) return -1;
      } // High chid are legal, ignore them.
    } else if (!memcmp(chunk.id,"EVTS",4)) {
      if (file->eventv) return -1; // Multiple EVTS
      file->eventv=chunk.v;
      file->eventc=chunk.c;
    } else if (!memcmp(chunk.id,"TEXT",4)) {
      if (file->textv) return -1; // Multiple TEXT
      file->textv=chunk.v;
      file->textc=chunk.c;
    } else {
      // Ignore unknown chunks.
    }
  }
  if (!file->tempo) return -1; // No header.
  if ((file->loopp<0)||(file->loopp>file->eventc)) return -1; // (loopp) must be within the size of the "EVTS" chunk.
  // Nothing else is strictly mandatory.
  return 0;
}

/* Advance event reader.
 * Combine delays and drop zero-length delays.
 */

int eau_event_reader_next(struct eau_event *event,struct eau_event_reader *reader) {
  int delay=0;
  while (reader->p<reader->c) {
    uint8_t lead=((uint8_t*)reader->v)[reader->p++];
    switch (lead&0xc0) {
      case 0x00: { // Short delay.
          delay+=lead;
        } break;
      case 0x40: { // Long delay.
          int ms=((lead&0x3f)+1)<<6;
          if (delay>INT_MAX-ms) { reader->p--; goto _ready_; } // unlikely
          delay+=ms;
        } break;
      case 0x80: { // Note.
          if (delay) { reader->p--; goto _ready_; }
          if (reader->p>reader->c-3) return -1;
          uint8_t a=((uint8_t*)reader->v)[reader->p++];
          uint8_t b=((uint8_t*)reader->v)[reader->p++];
          uint8_t c=((uint8_t*)reader->v)[reader->p++];
          event->type='n';
          event->note.chid=(lead>>2)&15;
          event->note.noteid=((lead&3)<<5)|(a>>3);
          event->note.velocity=((a&7)<<4)|(b>>4);
          event->note.durms=(((b&15)<<8)|c)<<2;
        } return 1;
      case 0xc0: { // Wheel.
          if (delay) { reader->p--; goto _ready_; }
          if (reader->p>=reader->c) return -1;
          uint8_t a=((uint8_t*)reader->v)[reader->p++];
          event->type='w';
          event->wheel.chid=(lead>>2)&15;
          event->wheel.v=(((lead&3)<<8)|a)-512;
        } return 1;
    }
  }
 _ready_:;
  if (delay) {
    event->type='d';
    event->delay=delay;
    return 1;
  }
  return 0;
}
