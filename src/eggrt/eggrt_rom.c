/* eggrt_rom.c
 * Linkage to the game ROM.
 * For now, games are always statically linked against us, so this is pretty trivial.
 * I might extend eggrt in the future to load ROM files.
 */
 
#include "eggrt_internal.h"

extern const uint8_t _egg_embedded_rom[];
extern const int _egg_embedded_rom_size;

void eggrt_rom_quit() {
  eggrt.rom=0;
  eggrt.romc=0;
  if (eggrt.resv) free(eggrt.resv);
  eggrt.resv=0;
  eggrt.resc=0;
  eggrt.resa=0;
}

int eggrt_rom_init() {
  eggrt.rom=(void*)_egg_embedded_rom;
  eggrt.romc=_egg_embedded_rom_size;
  
  struct rom_reader reader;
  if (rom_reader_init(&reader,eggrt.rom,eggrt.romc)<0) return -1;
  for (;;) {
    struct rom_entry res;
    int err=rom_reader_next(&res,&reader);
    if (err<0) return -1;
    if (!err) break;
    if (eggrt.resc>=eggrt.resa) {
      int na=eggrt.resa+128;
      if (na>INT_MAX/sizeof(struct rom_entry)) return -1;
      void *nv=realloc(eggrt.resv,sizeof(struct rom_entry)*na);
      if (!nv) return -1;
      eggrt.resv=nv;
      eggrt.resa=na;
    }
    eggrt.resv[eggrt.resc++]=res;
  }
  
  return 0;
}

int eggrt_rom_search(int tid,int rid) {
  int lo=0,hi=eggrt.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_entry *res=eggrt.resv+ck;
         if (tid<res->tid) hi=ck;
    else if (tid>res->tid) lo=ck+1;
    else if (rid<res->rid) hi=ck;
    else if (rid>res->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}
