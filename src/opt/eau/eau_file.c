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
