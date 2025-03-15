#include "eggdev/eggdev_internal.h"

/* Read the build configuration if we don't have it yet.
 */
 
static int eggdev_config_require() {
  if (g.bcfga) return 0;
  if (!(g.bcfgv=malloc(sizeof(struct eggdev_bcfg)*32))) return -1;
  g.bcfga=32;
  
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/local/config.mk",g.sdkpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read Egg's build configuration.\n",path);
    return -2;
  }
  
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if ((linec>=7)&&!memcmp(line,"export ",7)) { linec-=7; line+=7; }
    if (!linec||(line[0]=='#')) continue;
    int sepp=0;
    while ((sepp<linec)&&(line[sepp]!=':')) sepp++;
    if (!sepp||(sepp>=linec-1)||(line[sepp+1]!='=')) continue;
    const char *k=line;
    int kc=sepp;
    const char *v=line+sepp+2;
    int vc=linec-sepp-2;
    if ((kc==7)&&!memcmp(k,"EGG_SDK",7)) continue; // EGG_SDK is delivered special at build time (we need to find the rest of them).
    
    if (g.bcfgc>=g.bcfga) {
      int na=g.bcfga+32;
      if (na>INT_MAX/sizeof(struct eggdev_bcfg)) {
        free(src);
        return -1;
      }
      void *nv=realloc(g.bcfgv,sizeof(struct eggdev_bcfg)*na);
      if (!nv) {
        free(src);
        return -1;
      }
      g.bcfgv=nv;
      g.bcfga=na;
    }
    struct eggdev_bcfg *bcfg=g.bcfgv+g.bcfgc++;
    memset(bcfg,0,sizeof(struct eggdev_bcfg));
    if (!(bcfg->k=malloc(kc+1))||!(bcfg->v=malloc(vc+1))) {
      if (bcfg->k) free(bcfg->k);
      g.bcfgc--;
      free(src);
      return -1;
    }
    memcpy(bcfg->k,k,kc);
    memcpy(bcfg->v,v,vc);
    bcfg->k[bcfg->kc=kc]=0;
    bcfg->v[bcfg->vc=vc]=0;
  }
  
  free(src);
  return 0;
}

/* Key by index.
 */
 
int eggdev_config_key_by_index(void *dstpp,int p) {
  if (p<0) return -1;
  if (!p--) { *(const void**)dstpp="EGG_SDK"; return 7; }
  eggdev_config_require();
  if (p>=g.bcfgc) return -1;
  *(const void**)dstpp=g.bcfgv[p].k;
  return g.bcfgv[p].kc;
}

/* Value by key.
 */
 
int eggdev_config_get(void *dstpp,const char *k,int kc) {
  *(const void**)dstpp="";
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  #define RTN(zs) { \
    const char *_result=(zs); \
    int c=0; while (_result[c]) c++; \
    *(const void**)dstpp=_result; \
    return c; \
  }
  if ((kc==7)&&!memcmp(k,"EGG_SDK",7)) RTN(g.sdkpath);
  eggdev_config_require();
  const struct eggdev_bcfg *bcfg=g.bcfgv;
  int i=g.bcfgc;
  for (;i-->0;bcfg++) {
    if (bcfg->kc!=kc) continue;
    if (memcmp(bcfg->k,k,kc)) continue;
    *(const void**)dstpp=bcfg->v;
    return bcfg->vc;
  }
  #undef RTN
  return 0;
}

/* Value by key, under a given target.
 */
 
int eggdev_config_get_sub(void *dstpp,const char *target,int targetc,const char *k,int kc) {
  *(const void**)dstpp="";
  if (!target) targetc=0; else if (targetc<0) { targetc=0; while (target[targetc]) targetc++; }
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  char fullk[256];
  int fullkc=snprintf(fullk,sizeof(fullk),"%.*s/%.*s",targetc,target,kc,k);
  if ((fullkc<1)||(fullkc>=sizeof(fullk))) return 0;
  return eggdev_config_get(dstpp,fullk,fullkc);
}

/* Dump configuration, main entry point.
 */
 
int eggdev_main_config() {
  if (g.srcpathc) {
    int i=0; for (;i<g.srcpathc;i++) {
      const char *v=0;
      int vc=eggdev_config_get(&v,g.srcpathv[i],-1);
      if (vc<0) {
        fprintf(stderr,"%s: Field '%s' not found.\n",g.exename,g.srcpathv[i]);
      } else {
        fprintf(stdout,"%.*s\n",vc,v);
      }
    }
  } else {
    int i=0; for (;;i++) {
      const char *k,*v;
      int kc=eggdev_config_key_by_index(&k,i);
      if (kc<=0) break;
      int vc=eggdev_config_get(&v,k,kc);
      if (vc<0) break;
      fprintf(stdout,"%.*s=%.*s\n",kc,k,vc,v);
    }
  }
  return 0;
}
