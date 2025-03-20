/* eggrt_rom.c
 * Linkage to the game ROM.
 * For now, games are always statically linked against us, so this is pretty trivial.
 * I might extend eggrt in the future to load ROM files.
 */
 
#include "eggrt_internal.h"
#include "opt/serial/serial.h"

extern const uint8_t _egg_embedded_rom[];
extern const int _egg_embedded_rom_size;

/* Quit.
 */

void eggrt_rom_quit() {
  eggrt.rom=0;
  eggrt.romc=0;
  if (eggrt.resv) free(eggrt.resv);
  eggrt.resv=0;
  eggrt.resc=0;
  eggrt.resa=0;
  memset(&eggrt.metadata,0,sizeof(eggrt.metadata));
}

/* Metadata bits.
 */
 
static int eggrt_eval_fb(int *w,int *h,const char *src,int srcc) {
  *w=*h=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*w)*=10;
    (*w)+=src[srcp++]-'0';
  }
  if ((srcp>=srcc)||(src[srcp++]!='x')) return -1;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*h)*=10;
    (*h)+=src[srcp++]-'0';
  }
  if (srcp<srcc) return *w=*h=-1;
  return 0;
}

static int eggrt_eval_players(int *lo,int *hi,const char *src,int srcc) {
  *lo=*hi=0;
  int srcp=0;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*lo)*=10;
    (*lo)+=src[srcp++]-'0';
  }
  if (srcp>=srcc) {
    *hi=*lo;
    return 0;
  }
  if ((srcp>srcc-2)||memcmp(src+srcp,"..",2)) return -1;
  srcp+=2;
  while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
    (*hi)*=10;
    (*hi)+=src[srcp++]-'0';
  }
  if (srcp<srcc) return *lo=*hi=-1;
  if (!*hi) *hi=8;
  return 0;
}

/* Read metadata:1 and populate eggrt.metadata.
 */
 
static int eggrt_rom_load_metadata() {

  eggrt.metadata.fbw=640;
  eggrt.metadata.fbh=360;
  eggrt.metadata.playerclo=1;
  eggrt.metadata.playerchi=1;
  
  /* metadata:1 must be the first resource, and it's required.
   */
  if ((eggrt.resc<1)||(eggrt.resv[0].tid!=EGG_TID_metadata)||(eggrt.resv[0].rid!=1)) {
    fprintf(stderr,"%s: ROM does not contain metadata:1\n",eggrt.exename);
    return -2;
  }
  struct metadata_reader reader;
  if (metadata_reader_init(&reader,eggrt.resv[0].v,eggrt.resv[0].c)<0) {
    fprintf(stderr,"%s: Malformed metadata.\n",eggrt.exename);
    return -2;
  }
  struct metadata_entry entry;
  while (metadata_reader_next(&entry,&reader)>0) {

    if ((entry.kc==2)&&!memcmp(entry.k,"fb",2)) {
      eggrt_eval_fb(&eggrt.metadata.fbw,&eggrt.metadata.fbh,entry.v,entry.vc);

    } else if ((entry.kc==5)&&!memcmp(entry.k,"title",5)) {
      eggrt.metadata.title=entry.v;
      eggrt.metadata.titlec=entry.vc;

    } else if ((entry.kc==6)&&!memcmp(entry.k,"title$",6)) {
      sr_int_eval(&eggrt.metadata.title_strix,entry.v,entry.vc);

    } else if ((entry.kc==9)&&!memcmp(entry.k,"iconImage",9)) {
      sr_int_eval(&eggrt.metadata.icon_imageid,entry.v,entry.vc);

    } else if ((entry.kc==7)&&!memcmp(entry.k,"players",7)) {
      eggrt_eval_players(&eggrt.metadata.playerclo,&eggrt.metadata.playerchi,entry.v,entry.vc);

    } else if ((entry.kc==4)&&!memcmp(entry.k,"lang",4)) {
      eggrt.metadata.lang=entry.v;
      eggrt.metadata.langc=entry.vc;

    } else if ((entry.kc==8)&&!memcmp(entry.k,"required",8)) { //TODO We're dutifully recording these... Not sure when we ought to validate.
      eggrt.metadata.required=entry.v;
      eggrt.metadata.requiredc=entry.vc;

    } else if ((entry.kc==8)&&!memcmp(entry.k,"optional",8)) {
      eggrt.metadata.optional=entry.v;
      eggrt.metadata.optionalc=entry.vc;

    }
  }
  return 0;
}

/* Acquire rom.
 */

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
  
  int err=eggrt_rom_load_metadata();
  if (err<0) return err;
  
  return 0;
}

/* Search resources.
 */

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
