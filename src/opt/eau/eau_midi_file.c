#include "eau.h"
#include "opt/serial/serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Delete.
 */
 
void midi_file_del(struct midi_file *file) {
  if (!file) return;
  if (file->keepalive) free(file->keepalive);
  if (file->trackv) free(file->trackv);
  free(file);
}

/* Replace (spertick) for a given tempo in us/qnote, and the stored division.
 */
 
static void midi_file_set_tempo(struct midi_file *file,int usperqnote) {
  double sperqnote=(double)usperqnote/1000000.0;
  if (sperqnote<0.000001) sperqnote=0.500;
  file->spertick=sperqnote/(double)file->division;
}

/* Receive MThd chunk.
 */
 
static int midi_file_receive_MThd(struct midi_file *file,const uint8_t *src,int srcc) {
  if (file->division) return -1; // Multiple MThd.
  if (srcc<6) return -1;
  file->format=(src[0]<<8)|src[1];
  file->track_count=(src[2]<<8)|src[3];
  file->division=(src[4]<<8)|src[5];
  if ((file->division<1)||(file->division>0x7fff)) return -1; // Division must be nonzero, and we don't support SMPTE format.
  // Could assert (format==1) but meh, let it slide.
  midi_file_set_tempo(file,500000); // Per spec, default is 120 bpm.
  return 0;
}

/* Receive MTrk chunk.
 * For tie-breaking purposes, it's important to keep (trackv) in the order discovered, that's the order they appear in the file.
 * That is of course the obvious way to do it anyway, just saying it's not by accident.
 */
 
static int midi_file_receive_MTrk(struct midi_file *file,const uint8_t *src,int srcc) {
  if (file->trackc>=file->tracka) {
    int na=file->tracka+8;
    if (na>INT_MAX/sizeof(struct midi_track)) return -1;
    void *nv=realloc(file->trackv,sizeof(struct midi_track)*na);
    if (!nv) return -1;
    file->trackv=nv;
    file->tracka=na;
  }
  struct midi_track *track=file->trackv+file->trackc++;
  memset(track,0,sizeof(struct midi_track));
  track->v=src;
  track->c=srcc;
  track->delay=-1;
  track->chpfx=0xff;
  return 0;
}

/* Decode file. (src) must be (file->keepalive), we're going to borrow from it.
 */
 
static int midi_file_decode(struct midi_file *file,const uint8_t *src,int srcc) {
  file->division=0;
  file->trackc=0;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-8) return -1;
    const void *chunkid=src+srcp;
    int chunklen=(src[srcp+4]<<24)|(src[srcp+5]<<16)|(src[srcp+6]<<8)|src[srcp+7];
    srcp+=8;
    if ((chunklen<0)||(srcp>srcc-chunklen)) return -1;
    const void *chunk=src+srcp;
    srcp+=chunklen;
    int err=0;
    if (!memcmp(chunkid,"MThd",4)) err=midi_file_receive_MThd(file,chunk,chunklen);
    else if (!memcmp(chunkid,"MTrk",4)) err=midi_file_receive_MTrk(file,chunk,chunklen);
    if (err<0) return err;
  }
  if (!file->division) return -1; // MThd required.
  if (!file->trackc) return -1; // At least one MTrk required.
  return 0;
}

/* New reader.
 */
 
struct midi_file *midi_file_new(const void *src,int srcc) {
  if (!src||(srcc<0)) return 0;
  struct midi_file *file=calloc(1,sizeof(struct midi_file));
  if (!file) return 0;
  if (!(file->keepalive=malloc(srcc))) {
    free(file);
    return 0;
  }
  memcpy(file->keepalive,src,srcc);
  if (midi_file_decode(file,file->keepalive,srcc)<0) {
    midi_file_del(file);
    return 0;
  }
  return file;
}

/* Reset.
 */
 
void midi_file_reset(struct midi_file *file) {
  file->ph=0.0;
  midi_file_set_tempo(file,500000);
  struct midi_track *track=file->trackv;
  int i=file->trackc;
  for (;i-->0;track++) {
    track->p=0;
    track->delay=-1;
    track->status=0;
    track->chpfx=0xff;
  }
}

/* Process event locally, eg Set Tempo and MIDI Channel Prefix.
 * We're not allowed to modify or filter events.
 */
 
static int midi_file_local_event(struct midi_file *file,struct midi_track *track,const struct midi_event *event) {
  // We're only interested in Meta events. (that's not a hard rule but I think will always be so).
  if (event->type!='b') return 0;
  if (event->block.opcode!=0xff) return 0;
  const uint8_t *v=event->block.v;
  switch (event->block.type) {
    case 0x20: { // MIDI Channel Prefix
        if (event->block.c>=1) {
          track->chpfx=v[0];
        }
      } break;
    case 0x2f: { // End Of Track
        track->p=track->c;
        track->delay=0;
      } break;
    case 0x51: { // Set Tempo
        if (event->block.c>=3) {
          int usperqnote=(v[0]<<16)|(v[1]<<8)|v[2];
          midi_file_set_tempo(file,usperqnote);
        }
      } break;
  }
  return 0;
}

/* Read delay for a track.
 */
 
static int midi_track_read_delay(struct midi_track *track) {
  if (track->p>=track->c) {
    track->delay=0;
    return 0;
  }
  int err=sr_vlq_decode(&track->delay,track->v+track->p,track->c-track->p);
  if ((err<=0)||(track->delay<0)) return -1;
  track->p+=err;
  return 0;
}

/* Read event from a track and update its status.
 */
 
static int midi_track_read_event(struct midi_event *event,struct midi_track *track,struct midi_file *file) {
  if (track->p>=track->c) return -1;
  track->delay=-1;
  event->trackid=track-file->trackv;
  
  uint8_t lead;
  if (track->v[track->p]&0x80) lead=track->v[track->p++];
  else if (track->status) lead=track->status;
  else return -1;
  
  // These are only true for channel voice events. Let the block events replace later.
  track->status=lead;
  event->type='c';
  event->cv.opcode=lead&0xf0;
  event->cv.chid=lead&0x0f;
  
  switch (lead&0xf0) {
    case 0x90: { // Note On can become Note Off under the right circumstances.
        if (track->p>track->c-2) return -1;
        event->cv.a=track->v[track->p++];
        event->cv.b=track->v[track->p++];
        if (!event->cv.b) {
          event->cv.opcode=0x80;
          event->cv.b=0x40;
        }
      } break;
    case 0x80: case 0xa0: case 0xb0: case 0xe0: { // Note Off, Note Adjust, Control Change, Wheel have two data bytes.
        if (track->p>track->c-2) return -1;
        event->cv.a=track->v[track->p++];
        event->cv.b=track->v[track->p++];
      } break;
    case 0xc0: case 0xd0: { // Program Change and Channel Pressure have just one data byte.
        if (track->p>=track->c) return -1;
        event->cv.a=track->v[track->p++];
        event->cv.b=0;
      } break;
    case 0xf0: { // Meta and Sysex are essentially the same to us, just Meta has an extra "type" byte.
        track->status=0;
        event->type='b';
        event->block.opcode=lead;
        event->block.chid=track->chpfx;
        switch (lead) {
          case 0xff: {
              if (track->p>=track->c) return -1;
              event->block.type=track->v[track->p++];
            } break;
          case 0xf0: case 0xf7: event->block.type=0; break;
          default: return -1;
        }
        int seqlen=0;
        if ((seqlen=sr_vlq_decode(&event->block.c,track->v+track->p,track->c-track->p))<1) return -1;
        if (track->p>track->c-seqlen) return -1;
        track->p+=seqlen;
        event->block.v=track->v+track->p;
        track->p+=event->block.c;
      } break;
    default: return -1;
  }
  return 0;
}

/* Next event.
 */
 
int midi_file_next(struct midi_event *event,struct midi_file *file) {
  struct midi_track *track;
  int i;
  
  // Acquire delays where we need them, and note the shortest.
  int shortest=0x10000000; // Not encodable as VLQ.
  for (track=file->trackv,i=file->trackc;i-->0;track++) {
    if (track->delay<0) {
      if (midi_track_read_delay(track)<0) return -1;
    }
    if (track->p>=track->c) continue;
    if (track->delay<shortest) shortest=track->delay;
  }
  
  // If the shortest delay is still invalid, we're done.
  if (shortest>=0x10000000) return 0;
  
  // Drop all delays by (shortest), for tracks that aren't finished yet.
  // Note the first one that hits zero; there must be at least one.
  struct midi_track *etrack=0;
  for (track=file->trackv,i=file->trackc;i-->0;track++) {
    if (track->p>=track->c) continue;
    track->delay-=shortest;
    if (!etrack&&!track->delay) etrack=track;
  }
  if (!etrack) return -1;
  
  // If the delay we just applied was nonzero, report it as an event.
  if (shortest) {
    event->trackid=etrack-file->trackv;
    event->type='d';
    event->delay.ticks=shortest;
    event->delay.s=(double)shortest*file->spertick;
    return 1;
  }
  
  // Read, process, and report one event off that track.
  if (midi_track_read_event(event,etrack,file)<0) return -1;
  if (midi_file_local_event(file,etrack,event)<0) return -1;
  return 1;
}
