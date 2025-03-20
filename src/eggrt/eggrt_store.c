/* eggrt_store.c
 * Manages the per-game persistence.
 * Everything gets written to one file.
 * No header.
 * Any number of:
 *  - u8 kc
 *  - u16 vc
 *  - ... k
 *  - ... v
 * (exactly the same format as metadata, but that coincidence is probably not helpful).
 */

#include "eggrt_internal.h"
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

/* When the store changes, wait a second or so before saving.
 * This mitigates faulty clients that save too often, at least we won't hit the disk every frame.
 */
#define EGGRT_STORE_DEBOUNCE_FRAMES 60

static int eggrt_store_save();

/* Quit.
 */
 
static void eggrt_store_field_cleanup(struct eggrt_store_field *field) {
  if (field->k) free(field->k);
  if (field->v) free(field->v);
}
 
void eggrt_store_quit() {
  if (eggrt.storedirty&&eggrt.storepath) eggrt_store_save();
  if (eggrt.storev) {
    while (eggrt.storec-->0) eggrt_store_field_cleanup(eggrt.storev+eggrt.storec);
    free(eggrt.storev);
  }
  eggrt.storev=0;
  eggrt.storec=0;
  eggrt.storea=0;
  if (eggrt.storepath) {
    free(eggrt.storepath);
    eggrt.storepath=0;
  }
}

/* Search.
 */
 
static int eggrt_store_search(const char *k,int kc) {
  int lo=0,hi=eggrt.storec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct eggrt_store_field *field=eggrt.storev+ck;
         if (kc<field->kc) hi=ck;
    else if (kc>field->kc) lo=ck+1;
    else {
      int cmp=memcmp(k,field->k,kc);
           if (cmp<0) hi=ck;
      else if (cmp>0) lo=ck+1;
      else return ck;
    }
  }
  return -lo-1;
}

/* Insert.
 * No validation.
 */
 
static struct eggrt_store_field *eggrt_store_insert(int p,const char *k,int kc,const char *v,int vc) {
  if (eggrt.storec>=eggrt.storea) {
    int na=eggrt.storea+16;
    if (na>INT_MAX/sizeof(struct eggrt_store_field)) return 0;
    void *nv=realloc(eggrt.storev,sizeof(struct eggrt_store_field)*na);
    if (!nv) return 0;
    eggrt.storev=nv;
    eggrt.storea=na;
  }
  char *nk=malloc(kc+1);
  char *nv=malloc(vc+1);
  if (!nk||!nv) {
    if (nk) free(nk);
    if (nv) free(nv);
    return 0;
  }
  memcpy(nk,k,kc);
  memcpy(nv,v,vc);
  nk[kc]=0;
  nv[vc]=0;
  struct eggrt_store_field *field=eggrt.storev+p;
  memmove(field+1,field,sizeof(struct eggrt_store_field)*(eggrt.storec-p));
  eggrt.storec++;
  field->k=nk;
  field->kc=kc;
  field->v=nv;
  field->vc=vc;
  return field;
}

/* Validate.
 * Empty values are an edge case. We call them Invalid here, since they are not allowed to be stored.
 * But to our client, they are perfectly valid. Set a field empty to delete it.
 */
 
static int eggrt_store_validate_key(const char *src,int srcc) {
  if (!src||(srcc<1)||(srcc>0xff)) return -1;
  for (;srcc-->0;src++) {
    if ((*src<0x20)||(*src>0x7e)) return -1;
  }
  return 0;
}

static int eggrt_store_validate_value(const char *src,int srcc) {
  if (!src||(srcc<1)||(srcc>0xffff)) return -1;
  int srcp=0,seqlen,codepoint;
  while (srcp<srcc) {
    if (!(src[srcp]&0x80)) { srcp++; continue; }
    if ((seqlen=sr_utf8_decode(&codepoint,src+srcp,srcc-srcp))<1) return -1;
    srcp+=seqlen;
  }
  return 0;
}

/* Load in memory.
 */
 
static int eggrt_store_load_inner(const uint8_t *src,int srcc,const char *path) {
  int srcp=0;
  while (srcp<srcc) {
    uint8_t kc=src[srcp++];
    if (!kc) return -1; // Keys must not be empty.
    if (srcp>srcc-2) return -1;
    int vc=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-vc-kc) return -1;
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    if (!vc) continue; // Empty value is invalid but let it slide.
    // Don't validate content here; it's not actually a technical requirement.
    int p=eggrt_store_search(k,kc);
    if (p>=0) return -1; // Duplicate key.
    p=-p-1;
    if (!eggrt_store_insert(p,k,kc,v,vc)) return -1;
  }
  return 0;
}

/* Save in memory.
 */
 
static int eggrt_store_save_inner(struct sr_encoder *dst) {
  const struct eggrt_store_field *field=eggrt.storev;
  int i=eggrt.storec;
  for (;i-->0;field++) {
    if ((field->kc<1)||(field->kc>0xff)||(field->vc<0)||(field->vc>0xffff)) return -1;
    if (sr_encode_u8(dst,field->kc)<0) return -1;
    if (sr_encode_intbe(dst,field->vc,2)<0) return -1;
    if (sr_encode_raw(dst,field->k,field->kc)<0) return -1;
    if (sr_encode_raw(dst,field->v,field->vc)<0) return -1;
  }
  return 0;
}

/* Load.
 */
 
static int eggrt_store_load() {
  if (!eggrt.storepath) return -1;
  void *src=0;
  int srcc=file_read(&src,eggrt.storepath);
  if (srcc<0) return -1;
  while (eggrt.storec>0) {
    eggrt.storec--;
    eggrt_store_field_cleanup(eggrt.storev+eggrt.storec);
  }
  int err=eggrt_store_load_inner(src,srcc,eggrt.storepath);
  free(src);
  eggrt.storedirty=0;
  return err;
}

/* Save.
 */
 
static int eggrt_store_save() {
  if (!eggrt.storepath) return -1;
  struct sr_encoder dst={0};
  int err=eggrt_store_save_inner(&dst);
  if (err<0) {
    sr_encoder_cleanup(&dst);
    return err;
  }
  if (file_write(eggrt.storepath,dst.v,dst.c)<0) {
    sr_encoder_cleanup(&dst);
    return -1;
  }
  sr_encoder_cleanup(&dst);
  eggrt.storedirty=0;
  return 0;
}

/* Concatenate strings for save path.
 */
 
static char *eggrt_store_append_suffix(const char *a,int ac,const char *b,int bc) {
  if (!a||(ac<1)||!b||(bc<1)) return 0;
  int nc=ac+bc;
  char *nv=malloc(nc+1);
  if (!nv) return 0;
  memcpy(nv,a,ac);
  memcpy(nv+ac,b,bc);
  nv[nc]=0;
  return nv;
}

/* Generate default store path. HANDOFF.
 */
 
static char *eggrt_store_generate_path() {

  // If we ever do external loading of Wasm ROMs, the preferred path will be adjacent to the ROM.
  
  /* Measure the executable's path length and position of first slash.
   * Note that this is the name launched by. If we're on the PATH, it should be just the basename.
   */
  int slashp=-1,exenamec=0;
  for (;eggrt.exename[exenamec];exenamec++) {
    if ((slashp<0)&&(eggrt.exename[exenamec]=='/')) slashp=exenamec;
  }
  
  /* If it contains ".app/" anywhere, save adjacent to that bit.
   * For MacOS, we don't want to save inside the app bundle.
   */
  if (slashp>=0) {
    int p=0;
    while (p<exenamec) {
      if (eggrt.exename[p]=='/') { p++; continue; }
      const char *base=eggrt.exename+p;
      int basec=0;
      while ((p<exenamec)&&(eggrt.exename[p++]!='/')) basec++;
      if ((basec>4)&&!memcmp(base+basec-4,".app",4)) {
        return eggrt_store_append_suffix(eggrt.exename,(base-eggrt.exename)+basec-4,".save",5);
      }
    }
  }
  
  /* If our executable path is relative, save adjacent to the executable.
   */
  if (slashp>0) return eggrt_store_append_suffix(eggrt.exename,exenamec,".save",5);
  
  /* Likewise if the path is absolute but doesn't look like a global install place,
   * adjacent to the executable.
   */
  if (slashp==0) {
    if ((exenamec>=6)&&!memcmp(eggrt.exename,"/home/",6)) return eggrt_store_append_suffix(eggrt.exename,exenamec,".save",5);
    if ((exenamec>=7)&&!memcmp(eggrt.exename,"/Users/",7)) return eggrt_store_append_suffix(eggrt.exename,exenamec,".save",5);
  }
  
  /* Final component of the executable name, plus ".save", in the working directory.
   */
  const char *base=eggrt.exename+exenamec;
  int basec=0;
  while ((basec<exenamec)&&(base[-1]!='/')) { base--; basec++; }
  if (basec) return eggrt_store_append_suffix(base,basec,".save",5);
  
  return 0;
}

/* Init.
 */
 
int eggrt_store_init() {
  int err;
  
  if (!eggrt.store_req||!strcmp(eggrt.store_req,"default")) {
    if (!(eggrt.storepath=eggrt_store_generate_path())) {
      fprintf(stderr,"%s:WARNING: Failed to generate default store path. Saving will not be available.\n",eggrt.exename);
      return 0;
    }
  } else if (!strcmp(eggrt.store_req,"none")) {
    return 0;
  } else {
    if (!(eggrt.storepath=strdup(eggrt.store_req))) return -1;
  }
  
  if (!eggrt.storepath) return 0;
  if ((err=eggrt_store_load())<0) {
    if (err!=-2) fprintf(stderr,"%s: Failed to load store.\n",eggrt.storepath);
  }
  return 0;
}

/* Update.
 */
 
int eggrt_store_update() {
  if (eggrt.storedirty&&eggrt.storepath) {
    if (--(eggrt.storedebounce)<=0) {
      eggrt_store_save();
      // Even if saving fails, don't try again.
      // We will try again next time the client changes something.
      eggrt.storedirty=0;
    }
  }
  return 0;
}

/* Get field.
 */
 
int eggrt_store_get(char *v,int va,const char *k,int kc) {
  if (!v||(va<0)) va=0;
  if (!k||(kc<1)) return 0;
  int p=eggrt_store_search(k,kc);
  if (p<0) return 0;
  const struct eggrt_store_field *field=eggrt.storev+p;
  if (field->vc<=va) {
    memcpy(v,field->v,field->vc);
    if (field->vc<va) v[field->vc]=0;
  }
  return field->vc;
}

/* Set field.
 */
 
int eggrt_store_set(const char *k,int kc,const char *v,int vc) {
  if (!eggrt.storepath) return -1; // Saving disabled.
  if (!k||(kc<1)) return -1;
  if ((vc<0)||(vc&&!v)) return -1;
  int p=eggrt_store_search(k,kc);
  if (p<0) {
    if (!vc) return 0;
    p=-p-1;
    if (eggrt_store_validate_key(k,kc)<0) return -1;
    if (eggrt_store_validate_value(v,vc)<0) return -1;
    struct eggrt_store_field *field=eggrt_store_insert(p,k,kc,v,vc);
    if (!field) return -1;
  } else {
    struct eggrt_store_field *field=eggrt.storev+p;
    if ((vc==field->vc)&&!memcmp(field->v,v,vc)) return 0;
    if (vc) {
      if (eggrt_store_validate_value(v,vc)<0) return -1;
      char *nv=malloc(vc+1);
      if (!nv) return -1;
      memcpy(nv,v,vc);
      nv[vc]=0;
      if (field->v) free(field->v);
      field->v=nv;
      field->vc=vc;
    } else {
      eggrt_store_field_cleanup(field);
      eggrt.storec--;
      memmove(field,field+1,sizeof(struct eggrt_store_field)*(eggrt.storec-p));
    }
  }
  if (!eggrt.storedirty) {
    eggrt.storedirty=1;
    eggrt.storedebounce=EGGRT_STORE_DEBOUNCE_FRAMES;
  }
  return 0;
}

/* Key by index.
 */
 
int eggrt_store_key_by_index(char *k,int ka,int p) {
  if (!k||(ka<0)) ka=0;
  if ((p<0)||(p>=eggrt.storec)) return 0;
  const struct eggrt_store_field *field=eggrt.storev+p;
  if (field->kc<=ka) {
    memcpy(k,field->k,field->kc);
    if (field->kc<ka) k[field->kc]=0;
  }
  return field->kc;
}
