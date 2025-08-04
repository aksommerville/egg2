#include "eggdev/eggdev_internal.h"
#include "eggdev_rom.h"

/* Cleanup.
 */
 
static void eggdev_rw_res_cleanup(struct eggdev_rw_res *res) {
  if (res->v) free(res->v);
}
 
void eggdev_rom_writer_cleanup(struct eggdev_rom_writer *writer) {
  if (writer->resv) {
    while (writer->resc-->0) eggdev_rw_res_cleanup(writer->resv+writer->resc);
    free(writer->resv);
  }
}

/* Search.
 */

int eggdev_rom_writer_search(const struct eggdev_rom_writer *writer,int tid,int rid) {
  int lo=0,hi=writer->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eggdev_rw_res *q=writer->resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Insert.
 */
 
struct eggdev_rw_res *eggdev_rom_writer_insert(struct eggdev_rom_writer *writer,int p,int tid,int rid) {
  if ((p<0)||(p>writer->resc)) return 0;
  if (writer->resc>=writer->resa) {
    int na=writer->resa+32;
    if (na>INT_MAX/sizeof(struct eggdev_rw_res)) return 0;
    void *nv=realloc(writer->resv,sizeof(struct eggdev_rw_res)*na);
    if (!nv) return 0;
    writer->resv=nv;
    writer->resa=na;
  }
  struct eggdev_rw_res *res=writer->resv+p;
  memmove(res+1,res,sizeof(struct eggdev_rw_res)*(writer->resc-p));
  writer->resc++;
  memset(res,0,sizeof(struct eggdev_rw_res));
  res->tid=tid;
  res->rid=rid;
  return res;
}

/* Set serial.
 */

int eggdev_rw_res_set_serial(struct eggdev_rw_res *res,const void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  void *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (res->v) free(res->v);
  res->v=nv;
  res->c=srcc;
  return 0;
}

int eggdev_rw_res_handoff_serial(struct eggdev_rw_res *res,void *src,int srcc) {
  if ((srcc<0)||(srcc&&!src)) return -1;
  if (res->v) free(res->v);
  res->v=src;
  res->c=srcc;
  return 0;
}

/* Encode.
 */

int eggdev_rom_writer_encode(struct sr_encoder *dst,const struct eggdev_rom_writer *writer) {
  if (sr_encode_raw(dst,"\0ERM",4)<0) return -1;
  const struct eggdev_rw_res *res=writer->resv;
  int i=writer->resc,tid=1,rid=1;
  for (;i-->0;res++) {
  
    if (!res->c) continue;
    
    // Advance tid if necessary.
    if (res->tid<tid) return -1;
    if (res->tid>0xff) return -1;
    if (res->tid>tid) {
      int d=res->tid-tid;
      while (d>0x3f) {
        if (sr_encode_u8(dst,0x3f)<0) return -1;
        d-=0x3f;
      }
      if (d>0) {
        if (sr_encode_u8(dst,d)<0) return -1;
      }
      tid=res->tid;
      rid=1;
    }
    
    // Advance rid if necessary.
    if (res->rid<rid) return -1;
    if (res->rid>0xffff) return -1;
    if (res->rid>rid) {
      int d=res->rid-rid;
      while (d>=0x4000) {
        if (sr_encode_raw(dst,"\x7f\xff",2)<0) return -1;
        d-=0x4000;
      }
      if (d>0) {
        d--;
        if (sr_encode_intbe(dst,0x4000|d,2)<0) return -1;
      }
      rid=res->rid;
    }
    
    // Emit resource.
    if (res->c>0x400000) return -1;
    int word=0x800000|(res->c-1);
    if (sr_encode_intbe(dst,word,3)<0) return -1;
    if (sr_encode_raw(dst,res->v,res->c)<0) return -1;
    rid++;
  }
  if (sr_encode_u8(dst,0)<0) return -1;
  return 0;
}

/* Reader.
 */
 
int eggdev_rom_reader_init(struct eggdev_rom_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<4)||memcmp(src,"\0ERM",4)) return -1;
  reader->v=src;
  reader->c=srcc;
  reader->p=4;
  reader->tid=1;
  reader->rid=1;
  return 0;
}

int eggdev_rom_reader_next(struct eggdev_res *res,struct eggdev_rom_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return -1;
    uint8_t lead=reader->v[reader->p++];
    if (!lead) return 0;
    switch (lead&0xc0) {
      case 0x00: {
          reader->tid+=lead;
          if (reader->tid>0xff) return -1;
          reader->rid=1;
        } break;
      case 0x40: {
          if (reader->p>reader->c-1) return -1;
          int d=(lead&0x3f)<<8;
          d|=reader->v[reader->p++];
          d++;
          reader->rid+=d;
          if (reader->rid>0xffff) return -1;
        } break;
      case 0x80: {
          if (reader->p>reader->c-2) return -1;
          int l=(lead&0x3f)<<16;
          l|=reader->v[reader->p++]<<8;
          l|=reader->v[reader->p++];
          l++;
          if (reader->p>reader->c-l) return -1;
          res->tid=reader->tid;
          res->rid=reader->rid;
          res->v=reader->v+reader->p;
          res->c=l;
          reader->p+=l;
          reader->rid++;
          if (reader->rid>0xffff) return -1;
        } return 1;
      default: return -1;
    }
  }
}
