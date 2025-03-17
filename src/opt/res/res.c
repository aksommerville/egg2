#include "res.h"

#define SIGCK(v,sig) { \
  const char *_v=(v); \
  if ((_v[0]!=(sig)[0])||(_v[1]!=(sig)[1])||(_v[2]!=(sig)[2])||(_v[3]!=(sig)[3])) return -1; \
}

/* ROM.
 */

int rom_reader_init(struct rom_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<4)) return -1;
  SIGCK(src,"\0ERM")
  reader->v=src;
  reader->c=srcc;
  reader->p=4;
  reader->tid=1;
  reader->rid=1;
  return 0;
}

int rom_reader_next(struct rom_entry *entry,struct rom_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return -1; // Explicit termination required.
    unsigned char lead=reader->v[reader->p];
    if (!lead) return 0;
    reader->p++;
    switch (lead&0xc0) {
    
      case 0x00: { // TID
          reader->tid+=lead;
          reader->rid=1;
          if (reader->tid>0xff) return -1;
        } break;
        
      case 0x40: { // RID
          if (reader->p>reader->c-1) return -1;
          int d=(lead&0x3f)<<8;
          d|=reader->v[reader->p++];
          d++;
          reader->rid+=d;
          if (reader->rid>0xffff) return -1;
        } break;
        
      case 0x80: { // RES
          if (reader->rid>0xffff) return -1;
          if (reader->p>reader->c-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=reader->v[reader->p++]<<8;
          len|=reader->v[reader->p++];
          len++;
          if (reader->p>reader->c-len) return -1;
          entry->tid=reader->tid;
          entry->rid=reader->rid;
          entry->v=reader->v+reader->p;
          entry->c=len;
          reader->p+=len;
          reader->rid++;
        } return 1;
        
      case 0xc0: { // RESERVED
          return -1;
        }
    }
  }
}

/* Metadata.
 */

int metadata_reader_init(struct metadata_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<4)) return -1;
  SIGCK(src,"\0EMD")
  reader->v=src;
  reader->p=4;
  reader->c=srcc;
  return 0;
}

int metadata_reader_next(struct metadata_entry *entry,struct metadata_reader *reader) {
  if (reader->p>=reader->c) return -1; // Explicit termination required.
  if (!(entry->kc=reader->v[reader->p++])) return 0;
  if (reader->p>=reader->c) return -1;
  entry->vc=reader->v[reader->p++];
  if (reader->p>reader->c-entry->vc-entry->kc) return -1;
  entry->k=(char*)(reader->v+reader->p); reader->p+=entry->kc;
  entry->v=(char*)(reader->v+reader->p); reader->p+=entry->vc;
  return 1;
}

/* Strings.
 */

int strings_reader_init(struct strings_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<4)) return -1;
  SIGCK(src,"\0EST")
  reader->v=src;
  reader->p=4;
  reader->c=srcc;
  reader->index=1;
  return 0;
}

int strings_reader_next(struct strings_entry *entry,struct strings_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return 0;
    if (reader->p>reader->c-2) return -1;
    int len=(reader->v[reader->p]<<8)|reader->v[reader->p+1];
    reader->p+=2;
    if (reader->p>reader->c-len) return -1;
    entry->index=reader->index++;
    if (len) {
      entry->v=(char*)(reader->v+reader->p);
      entry->c=len;
      reader->p+=len;
      return 1;
    }
  }
}

/* Cmdlist.
 */

int cmdlist_reader_init(struct cmdlist_reader *reader,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  reader->v=src;
  reader->p=0;
  reader->c=srcc;
  return 0;
}

int cmdlist_reader_next(struct cmdlist_entry *entry,struct cmdlist_reader *reader) {
  if (reader->p>=reader->c) return 0;
  entry->opcode=reader->v[reader->p++];
  if (!entry->opcode) return -1; // Opcode zero is illegal (NB not a terminator as in similar formats).
  int paylen=0;
  switch (entry->opcode&0xe0) {
    case 0x00: break;
    case 0x20: paylen=2; break;
    case 0x40: paylen=4; break;
    case 0x60: paylen=8; break;
    case 0x80: paylen=12; break;
    case 0xa0: paylen=16; break;
    case 0xc0: paylen=20; break;
    case 0xe0: {
        if (reader->p>=reader->c) return -1;
        paylen=reader->v[reader->p++];
      } break;
  }
  if (reader->p>reader->c-paylen) return -1;
  entry->arg=reader->v+reader->p;
  entry->argc=paylen;
  reader->p+=paylen;
  return 1;
}

int sprite_reader_init(struct cmdlist_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<4)) return -1;
  SIGCK(src,"\0ESP")
  return cmdlist_reader_init(reader,(char*)src+4,srcc-4);
}

/* Map.
 */

int map_res_decode(struct map_res *map,const void *src,int srcc) {
  if (!src||(srcc<6)) return -1;
  SIGCK(src,"\0EMP")
  const unsigned char *SRC=src;
  map->w=SRC[4];
  map->h=SRC[5];
  if (!map->w||!map->h) return -1;
  int cellslen=map->w*map->h;
  int srcp=6;
  if (srcp>srcc-cellslen) return -1;
  map->v=SRC+srcp;
  srcp+=cellslen;
  map->cmd=SRC+srcp;
  map->cmdc=srcc-srcp;
  return 0;
}
