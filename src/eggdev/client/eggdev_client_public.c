#include "eggdev_client_internal.h"

/* Search namespaces.
 * Assume they are never sorted, which is usually true.
 */
 
static struct eggdev_ns *eggdev_client_find_ns(int nstype,const char *name,int namec) {
  struct eggdev_ns *ns=g.client.nsv;
  int i=g.client.nsc;
  for (;i-->0;ns++) {
    if (ns->nstype!=nstype) continue;
    if (ns->namec!=namec) continue;
    if (memcmp(ns->name,name,namec)) continue;
    return ns;
  }
  return 0;
}

static struct eggdev_sym *eggdev_client_find_sym_by_name(const struct eggdev_ns *ns,const char *k,int kc) {
  struct eggdev_sym *sym=ns->symv;
  int i=ns->symc;
  for (;i-->0;sym++) {
    if (sym->kc!=kc) continue;
    if (memcmp(sym->k,k,kc)) continue;
    return sym;
  }
  return 0;
}

static struct eggdev_sym *eggdev_client_find_sym_by_value(const struct eggdev_ns *ns,int v) {
  struct eggdev_sym *sym=ns->symv;
  int i=ns->symc;
  for (;i-->0;sym++) {
    if (sym->v!=v) continue;
    return sym;
  }
  return 0;
}

/* Add namespaces or symbols.
 */
 
struct eggdev_ns *eggdev_client_ns_intern(int nstype,const char *name,int namec) {
  if (!name) namec=0; else if (namec<0) { namec=0; while (name[namec]) namec++; }
  struct eggdev_ns *ns=eggdev_client_find_ns(nstype,name,namec);
  if (ns) return ns;
  if (g.client.nsc>=g.client.nsa) {
    int na=g.client.nsa+16;
    if (na>INT_MAX/sizeof(struct eggdev_ns)) return 0;
    void *nv=realloc(g.client.nsv,sizeof(struct eggdev_ns)*na);
    if (!nv) return 0;
    g.client.nsv=nv;
    g.client.nsa=na;
  }
  char *nv=malloc(namec+1);
  if (!nv) return 0;
  memcpy(nv,name,namec);
  nv[namec]=0;
  ns=g.client.nsv+g.client.nsc++;
  memset(ns,0,sizeof(struct eggdev_ns));
  ns->nstype=nstype;
  ns->name=nv;
  ns->namec=namec;
  return ns;
}

struct eggdev_sym *eggdev_client_sym_intern(struct eggdev_ns *ns,const char *k,int kc,int v) {
  if (!ns) return 0;
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!kc) return 0;
  struct eggdev_sym *sym=ns->symv;
  int i=ns->symc;
  for (;i-->0;sym++) {
    if (sym->v!=v) continue;
    if (sym->kc!=kc) continue;
    if (memcmp(sym->k,k,kc)) continue;
    return sym; // Already have it, both key and value.
  }
  if (ns->symc>=ns->syma) {
    int na=ns->syma+32;
    if (na>INT_MAX/sizeof(struct eggdev_sym)) return 0;
    void *nv=realloc(ns->symv,sizeof(struct eggdev_sym)*na);
    if (!nv) return 0;
    ns->symv=nv;
    ns->syma=na;
  }
  char *nv=malloc(kc+1);
  if (!nv) return 0;
  memcpy(nv,k,kc);
  nv[kc]=0;
  sym=ns->symv+ns->symc++;
  memset(sym,0,sizeof(struct eggdev_sym));
  sym->v=v;
  sym->k=nv;
  sym->kc=kc;
  return sym;
}

/* Generic symbol lookup.
 */
 
int eggdev_client_resolve_value(int *v,int nstype,const char *ns,int nsc,const char *k,int kc) {
  if (!ns) nsc=0; else if (nsc<0) { nsc=0; while (ns[nsc]) nsc++; }
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct eggdev_ns *nso=eggdev_client_find_ns(nstype,ns,nsc);
  if (!nso) return -1;
  struct eggdev_sym *sym=eggdev_client_find_sym_by_name(nso,k,kc);
  if (!sym) return -1;
  *v=sym->v;
  return 0;
}

int eggdev_client_resolve_name(void *kpp,int nstype,const char *ns,int nsc,int v) {
  if (!ns) nsc=0; else if (nsc<0) { nsc=0; while (ns[nsc]) nsc++; }
  struct eggdev_ns *nso=eggdev_client_find_ns(nstype,ns,nsc);
  if (!nso) return -1;
  struct eggdev_sym *sym=eggdev_client_find_sym_by_value(nso,v);
  if (!sym) return -1;
  *(const void**)kpp=sym->k;
  return sym->kc;
}

/* Evaluate tid.
 */
 
int eggdev_tid_eval_standard(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return EGG_TID_##tag;
  EGG_TID_FOR_EACH
  #undef _
  return 0;
}
 
int eggdev_tid_eval(const char *src,int srcc) {

  /* Easiest case: Empty is invalid, and plain integers 1..255 are valid.
   * No other valid tid may be a lexical integer.
   */
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (!srcc) return 0;
  int v;
  if (sr_int_eval(&v,src,srcc)>=2) return ((v>0)&&(v<0x100))?v:0;
  
  /* Also very easy: Standard types, known at eggdev's compile time.
   */
  if ((v=eggdev_tid_eval_standard(src,srcc))>0) return v;
  
  /* For the rest, we need the project context.
   */
  if (eggdev_client_require()<0) return 0;
  if (eggdev_client_resolve_value(&v,EGGDEV_NSTYPE_RESTYPE,0,0,src,srcc)>=0) {
    if ((v>0)&&(v<0x100)) return v;
  }
  
  return 0;
}

/* Represent tid.
 */
 
int eggdev_tid_repr(char *dst,int dsta,int tid) {

  /* Standard types are easy.
   */
  const char *src=0;
  int srcc=0;
  switch (tid) {
    #define _(tag) case EGG_TID_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    EGG_TID_FOR_EACH
    #undef _
  }
  if (srcc>0) {
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  
  /* Check custom types if it's at least in range.
   */
  if ((tid>0)&&(tid<0x100)&&(eggdev_client_require()>=0)) {
    if ((srcc=eggdev_client_resolve_name(&src,EGGDEV_NSTYPE_RESTYPE,0,0,tid))>0) {
      if (srcc<=dsta) {
        memcpy(dst,src,srcc);
        if (srcc<dsta) dst[srcc]=0;
      }
      return srcc;
    }
  }

  /* Anything else, even illegal tids, represent as a decimal integer.
   */
  return sr_decsint_repr(dst,dsta,tid);
}

/* Evaluate or represent general symbol, public forms.
 */
 
int eggdev_symbol_eval(int *dst,const char *src,int srcc,int nstype,const char *ns,int nsc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (nstype==EGGDEV_NSTYPE_RESTYPE) {
    *dst=eggdev_tid_eval(src,srcc);
    return (*dst>0)?0:-1;
  }
  if (ns) eggdev_client_require();
  if (eggdev_client_resolve_value(dst,nstype,ns,nsc,src,srcc)>=0) return 0;
  if (sr_int_eval(dst,src,srcc)>=2) return 0;
  
  // Accept language qualified integers for EGGDEV_NSTYPE_RES.
  if (nstype==EGGDEV_NSTYPE_RES) {
    if ((srcc>=4)&&(src[0]>='a')&&(src[0]<='z')&&(src[1]>='a')&&(src[1]<='z')&&(src[2]=='-')) {
      int subrid=0;
      if ((sr_int_eval(&subrid,src+3,srcc-3)>=2)&&(subrid>0)&&(subrid<0x40)) {
        *dst=((src[0]-'a'+1)<<11)|((src[1]-'a'+1)<<6)|subrid;
        return 0;
      }
    }
  }
  
  return -1;
}
 
int eggdev_symbol_repr(char *dst,int dsta,int v,int nstype,const char *ns,int nsc) {
  if (nstype==EGGDEV_NSTYPE_RESTYPE) return eggdev_tid_repr(dst,dsta,v);
  if (ns) eggdev_client_require();
  const char *src=0;
  int srcc=eggdev_client_resolve_name(&src,nstype,ns,nsc,v);
  if (srcc<1) return sr_decsint_repr(dst,dsta,v);
  if (srcc<=dsta) {
    memcpy(dst,src,srcc);
    if (srcc<dsta) dst[srcc]=0;
  }
  return srcc;
}
