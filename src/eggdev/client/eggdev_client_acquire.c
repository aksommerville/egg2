#include "eggdev_client_internal.h"

/* Drop cached data.
 */
  
static void eggdev_sym_cleanup(struct eggdev_sym *sym) {
  if (sym->k) free(sym->k);
  if (sym->comment) free(sym->comment);
}
  
static void eggdev_ns_cleanup(struct eggdev_ns *ns) {
  if (ns->name) free(ns->name);
  if (ns->symv) {
    while (ns->symc-->0) eggdev_sym_cleanup(ns->symv+ns->symc);
    free(ns->symv);
  }
}
 
void eggdev_client_dirty() {
  while (g.client.nsc>0) eggdev_ns_cleanup(g.client.nsv+(--(g.client.nsc)));
  g.client.ready=0;
}

/* Replace client root.
 */
 
int eggdev_client_set_root(const char *path,int pathc) {
  eggdev_client_dirty();
  if (!path) pathc=0; else if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  char *nv=0;
  if (pathc) {
    if (!(nv=malloc(pathc+1))) return -1;
    memcpy(nv,path,pathc);
    nv[pathc]=0;
  }
  if (g.client.root) free(g.client.root);
  g.client.root=nv;
  g.client.rootc=pathc;
  return 0;
}

/* Define a symbol.
 */
 
static int eggdev_client_define(int nstype,const char *nsname,int nsnamec,const char *k,int kc,int v,const char *comment,int commentc) {
  if (!nsname) nsnamec=0; else if (nsnamec<0) { nsnamec=0; while (nsname[nsnamec]) nsnamec++; }
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  struct eggdev_ns *ns=eggdev_client_ns_intern(nstype,nsname,nsnamec);
  if (!ns) return -1;
  struct eggdev_sym *sym=eggdev_client_sym_intern(ns,k,kc,v);
  if (!sym) return -1;
  if (comment) {
    if (commentc<0) { commentc=0; while (comment[commentc]) commentc++; }
    if (commentc>0) {
      if (sym->comment=malloc(commentc+1)) {
        memcpy(sym->comment,comment,commentc);
        sym->comment[commentc]=0;
        sym->commentc=commentc;
      }
    }
  }
  return 0;
}

/* Load symbols from data directory, bottom level.
 */
 
static int eggdev_client_load_restoc_bottom(const char *path,const char *base,char ftype,void *userdata) {
  const char *tname=userdata;
  
  /* Take apart the basename: [LANG-]RID[-NAME][[.COMMENT].FORMAT]
   * We only need LANG, RID, and NAME, and noop if NAME is empty.
   */
  int lang=0,basep=0;
  if (
    (base[0]>='a')&&(base[0]<='z')&&
    (base[1]>='a')&&(base[1]<='z')&&
    (base[2]=='-')
  ) {
    basep=3;
    lang=((base[0]-'a'+1)<<5)|(base[1]-'a'+1);
  }
  if ((base[basep]<'0')||(base[basep]>'9')) {
    fprintf(stderr,"%s: Malformed file name in resources.\n",path);
    return -2;
  }
  int rid=0;
  while ((base[basep]>='0')&&(base[basep]<='9')) {
    rid*=10;
    rid+=base[basep++]-'0';
    if (rid>0xffff) {
      fprintf(stderr,"%s: Malformed file name in resources.\n",path);
      return -2;
    }
  }
  if (base[basep]!='-') return 0; // No name.
  basep++;
  const char *name=base+basep;
  int namec=0;
  while (name[namec]&&(name[namec]!='.')) namec++;
  if (lang) {
    if (rid>0x3f) {
      fprintf(stderr,"%s: Malformed file name in resources.\n",path);
      return -2;
    }
    //rid|=lang<<6; // For naming purposes, use the plain 6-bit rid.
  }

  return eggdev_client_define(EGGDEV_NSTYPE_RES,tname,-1,name,namec,rid,0,0);
}

/* Load symbols from data directory, top level.
 */
 
static int eggdev_client_load_restoc_top(const char *path,const char *base,char ftype,void *userdata) {
  int basec=0;
  while (base[basec]) basec++;
  // Two top-level data files are special, and no need to do anything about them. They are both anonymous.
  if ((basec==8)&&!memcmp(base,"metadata",8)) return 0;
  if ((basec==9)&&!memcmp(base,"code.wasm",9)) return 0;
  // Everything else must be a directory. If dirent doesn't provide that, just move on. But fail if it gave a concrete wrong answer.
  if (ftype&&(ftype!='d')) {
    fprintf(stderr,"%s: Unexpected file type '%c' in data root.\n",path,ftype);
    return -2;
  }
  
  /* If it's an identifier and not a standard type, define it in EGGDEV_NSTYPE_RESTYPE.
   * Values are initially zero.
   * Once all the custom types are discovered, we'll sweep them again, sort, and assign proper IDs.
   */
  if ((base[0]>='0')&&(base[0]<='9')) ;
  else if (eggdev_tid_eval_standard(base,basec)>0) ;
  else {
    if (eggdev_client_define(EGGDEV_NSTYPE_RESTYPE,0,0,base,basec,0,0,0)<0) return -1;
  }
  
  // Enter directory to list named resources.
  return dir_read(path,eggdev_client_load_restoc_bottom,(void*)base);
}

/* Compare symbols by strcmp, for assigning custom TID.
 */
 
static int eggdev_sym_cmp(const void *a,const void *b) {
  const struct eggdev_sym *A=a,*B=b;
  return strcmp(A->k,B->k);
}

/* Load resource TOC by reading project's data directory.
 */
 
static int eggdev_client_load_restoc() {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/src/data",g.client.rootc,g.client.root);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  int err=dir_read(path,eggdev_client_load_restoc_top,0);
  if (err<0) return err;
  
  /* EGGDEV_NSTYPE_RESTYPE was populated with dummy values.
   * Sort its symbols by strcmp(), then assign tid starting at 32.
   * These become the permanent tids in the ROM so it's important that the process be repeatable.
   */
  struct eggdev_ns *ns=g.client.nsv;
  int i=g.client.nsc;
  for (;i-->0;ns++) {
    if (ns->nstype!=EGGDEV_NSTYPE_RESTYPE) continue;
    if (ns->symc>96) {
      fprintf(stderr,"%.*s: Too many custom resource types. %d, limit 96.\n",g.client.rootc,g.client.root,ns->symc);
      return -2;
    }
    qsort(ns->symv,ns->symc,sizeof(struct eggdev_sym),eggdev_sym_cmp);
    struct eggdev_sym *sym=ns->symv;
    int si=ns->symc;
    int tid=32;
    for (;si-->0;sym++,tid++) sym->v=tid;
    break;
  }
  
  return 0;
}

/* Load a string symbol. (EGGDEV_*)
 * (src) is the remainder of the line, may contain space and comment.
 */
 
static int eggdev_client_load_string(const char *k,int kc,const char *src,int srcc) {
  if ((kc>7)&&!memcmp(k,"EGGDEV_",7)) {
    k+=7;
    kc-=7;
  }
  if (kc<1) return -1;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if ((srcp>=srcc)||(src[srcp]!='"')) return -1;
  int len=sr_string_measure(src+srcp,srcc-srcp,0);
  if (len<1) return -1;
  char tmp[256];
  int tmpc=sr_string_eval(tmp,sizeof(tmp),src+srcp,len);
  if ((tmpc<0)||(tmpc>sizeof(tmp))) return -1;
  if (g.client.stringc>=g.client.stringa) {
    int na=g.client.stringa+16;
    if (na>INT_MAX/sizeof(struct eggdev_config_string)) return -1;
    void *nv=realloc(g.client.stringv,sizeof(struct eggdev_config_string)*na);
    if (!nv) return -1;
    g.client.stringv=nv;
    g.client.stringa=na;
  }
  char *nk=malloc(kc+1);
  if (!nk) return -1;
  char *nv=malloc(tmpc+1);
  if (!nv) { free(nk); return -1; }
  memcpy(nk,k,kc);
  nk[kc]=0;
  memcpy(nv,tmp,tmpc);
  nv[tmpc]=0;
  struct eggdev_config_string *string=g.client.stringv+g.client.stringc++;
  string->k=nk;
  string->kc=kc;
  string->v=nv;
  string->vc=tmpc;
  return 0;
}

/* Load symbols from shared_symbols.h, in memory.
 */
 
static int eggdev_client_load_symbols_from_text(const char *src,int srcc,const char *path) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec) continue;
    int linep=0,tokenc=0;
    const char *token=line;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    if ((tokenc!=7)||memcmp(token,"#define",7)) continue;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    token=line+linep;
    tokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    if ((tokenc>7)&&!memcmp(token,"EGGDEV_",7)) {
      if (eggdev_client_load_string(token,tokenc,line+linep,linec-linep)<0) {
        fprintf(stderr,"%s:%d:WARNING: Ignoring symbol '%.*s' due to unspecified error.\n",path,lineno,tokenc,token);
      }
      continue;
    }
    int nstype;
    if ((tokenc>=3)&&!memcmp(token,"NS_",3)) { nstype=EGGDEV_NSTYPE_NS; token+=3; tokenc-=3; }
    else if ((tokenc>=4)&&!memcmp(token,"CMD_",4)) { nstype=EGGDEV_NSTYPE_CMD; token+=4; tokenc-=4; }
    else continue;
    const char *ns=token;
    int nsc=0;
    while ((nsc<tokenc)&&(ns[nsc]!='_')) nsc++;
    if (nsc>=tokenc) {
      fprintf(stderr,"%s:%d:WARNING: Ignoring malformed symbol. Expected another underscore.\n",path,lineno);
      continue;
    }
    const char *k=token+nsc+1;
    int kc=tokenc-nsc-1;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *vsrc=line+linep;
    int vsrcc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) vsrcc++;
    int v=0;
    if (sr_int_eval(&v,vsrc,vsrcc)<0) {
      fprintf(stderr,"%s:%d:WARNING: Ignoring symbol '%.*s' in namespace '%.*s' due to malformed value '%.*s'.\n",path,lineno,kc,k,nsc,ns,vsrcc,vsrc);
      continue;
    }
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *comment=0;
    int commentc=0;
    if ((linep<linec-4)&&!memcmp(line+linep,"/*",2)&&!memcmp(line+linec-2,"*/",2)) {
      linep+=2;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      comment=line+linep;
      commentc=linec-linep-2;
      while (commentc&&((unsigned char)comment[commentc-1]<=0x20)) commentc--;
    }
    if (eggdev_client_define(nstype,ns,nsc,k,kc,v,comment,commentc)<0) return -1;
  }
  return 0;
}

/* Load symbols from project's shared_symbols.h.
 */
 
static int eggdev_client_load_symbols() {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/src/game/shared_symbols.h",g.client.rootc,g.client.root);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return 0; // no harm no foul
  int err=eggdev_client_load_symbols_from_text(src,srcc,path);
  free(src);
  return err;
}

/* Require client context.
 */

int eggdev_client_require() {
  if (g.client.ready) return 0;
  if (g.client.rootc) {
    eggdev_client_load_restoc();
    eggdev_client_load_symbols();
  }
  return 0;
}
