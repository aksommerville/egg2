#include "eau.h"
#include <string.h>
#include <stdint.h>

/* Decode EAU file, top-level framing.
 */

int eau_file_decode(struct eau_file *file,const void *src,int srcc) {
  if (!src||(srcc<6)||memcmp(src,"\0EAU",4)) return -1;
  memset(file,0,sizeof(struct eau_file));
  const uint8_t *SRC=src;
  int srcp=4;
  file->tempo=(SRC[srcp]<<8)|SRC[srcp+1];
  srcp+=2;
  
  if (srcp>srcc-4) return 0;
  file->chdrc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((file->chdrc<0)||(srcp>srcc-file->chdrc)) return -1;
  file->chdr=SRC+srcp;
  srcp+=file->chdrc;
  
  if (srcp>srcc-4) return 0;
  file->evtsc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((file->evtsc<0)||(srcp>srcc-file->evtsc)) return -1;
  file->evts=SRC+srcp;
  srcp+=file->evtsc;
  
  if (srcp>srcc-4) return 0;
  file->textc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((file->textc<0)||(srcp>srcc-file->textc)) return -1;
  file->text=SRC+srcp;
  srcp+=file->textc;
  
  return 0;
}

/* Iterate Channel Headers.
 */

int eau_chdr_reader_next(struct eau_chdr_entry *entry,struct eau_chdr_reader *reader) {
  if (reader->p>=reader->c) return 0;
  if (reader->p>reader->c-6) return -1;
  const uint8_t *SRC=reader->v;
  entry->v=SRC+reader->p;
  int p0=reader->p;
  entry->chid=SRC[reader->p++];
  entry->trim=SRC[reader->p++];
  entry->pan=SRC[reader->p++];
  entry->mode=SRC[reader->p++];
  entry->modecfgc=(SRC[reader->p]<<8)|SRC[reader->p+1];
  reader->p+=2;
  if (reader->p>reader->c-entry->modecfgc) return -1;
  entry->modecfg=SRC+reader->p;
  reader->p+=entry->modecfgc;
  if (reader->p>reader->c-2) return -1;
  entry->postc=(SRC[reader->p]<<8)|SRC[reader->p+1];
  reader->p+=2;
  if (reader->p>reader->c-entry->postc) return -1;
  entry->post=SRC+reader->p;
  reader->p+=entry->postc;
  entry->c=reader->p-p0;
  return 1;
}

/* Iterate events.
 */
 
int eau_event_reader_next(struct eau_event *event,struct eau_event_reader *reader) {
  if (reader->p>=reader->c) return 0;
  const uint8_t *SRC=reader->v;
  uint8_t lead=SRC[reader->p++];
  
  if (!(lead&0x80)) { // Coalesce delays.
    event->opcode=0;
    event->chid=0xff;
    event->delay=0;
    event->c=0;
    for (;;) {
      if (lead&0x40) event->delay+=((lead&0x3f)+1)<<6;
      else event->delay+=lead;
      if (reader->p>=reader->c) break;
      if (SRC[reader->p]&0x80) break;
      lead=SRC[reader->p++];
    }
    return 1;
  }
  
  event->opcode=lead&0xf0;
  event->chid=lead&0x0f;
  switch (lead&0xf0) {
    case 0x80: case 0x90: case 0xe0: {
        if (reader->p>reader->c-2) return -1;
        memcpy(event->v,SRC+reader->p,2);
        event->c=2;
        reader->p+=2;
      } return 1;
    case 0xa0: {
        if (reader->p>reader->c-2) return -1;
        memcpy(event->v,SRC+reader->p,3);
        event->c=3;
        reader->p+=3;
      } return 1;
    case 0xf0: {
        event->chid=0xff;
        event->v[0]=lead&0x0f;
        event->c=1;
      } return 1;
  }
  return -1;
}

/* Iterate a text chunk.
 */
 
int eau_text_reader_next(struct eau_text_entry *entry,struct eau_text_reader *reader) {
  if (reader->p>=reader->c) return 0;
  if (reader->p>=reader->c-3) return -1;
  const uint8_t *SRC=reader->v;
  entry->chid=SRC[reader->p++];
  entry->noteid=SRC[reader->p++];
  entry->length=SRC[reader->p++];
  if (reader->p>reader->c-entry->length) return -1;
  entry->v=(char*)(SRC+reader->p);
  reader->p+=entry->length;
  return 1;
}

/* Estimate file duration.
 */
 
int eau_estimate_duration(const void *src,int srcc) {
  struct eau_file file={0};
  if (eau_file_decode(&file,src,srcc)<0) return -1;
  struct eau_event_reader reader={.v=file.evts,.c=file.evtsc};
  struct eau_event event;
  int ms=0;
  while (eau_event_reader_next(&event,&reader)>0) {
    if (!event.opcode) ms+=event.delay;
  }
  return ms;
}
