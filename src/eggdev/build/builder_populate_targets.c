#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Fetch one string.
 */
 
static int builder_target_get_string(void *dstpp,struct builder_target *target,const char *k) {
  *(const void**)dstpp="";
  char fullk[256];
  int fullkc=snprintf(fullk,sizeof(fullk),"%.*s_%s",target->namec,target->name,k);
  if ((fullkc<1)||(fullkc>=sizeof(fullk))) return 0;
  int err=eggdev_config_get(dstpp,fullk,fullkc);
  if (err<0) return 0;
  return err;
}

/* Add one target by name.
 */
 
static int builder_add_target(struct builder *builder,const char *name,int namec) {
  if (builder->targetc>=builder->targeta) {
    int na=builder->targeta+8;
    if (na>INT_MAX/sizeof(struct builder_target)) return -1;
    void *nv=realloc(builder->targetv,sizeof(struct builder_target)*na);
    if (!nv) return -1;
    builder->targetv=nv;
    builder->targeta=na;
  }
  struct builder_target *target=builder->targetv+builder->targetc++;
  memset(target,0,sizeof(struct builder_target));
  target->name=name;
  target->namec=namec;
  target->optc=builder_target_get_string(&target->opt,target,"OPT_ENABLE");
  target->ccc=builder_target_get_string(&target->cc,target,"CC");
  target->ldc=builder_target_get_string(&target->ld,target,"LD");
  target->ldpostc=builder_target_get_string(&target->ldpost,target,"LDPOST");
  target->pkgc=builder_target_get_string(&target->pkg,target,"PACKAGING");
  target->exesfxc=builder_target_get_string(&target->exesfx,target,"EXESFX");
  return 0;
}

/* Populate targets.
 */
 
int builder_populate_targets(struct builder *builder) {
  const char *src=0;
  int srcc=eggdev_config_get(&src,"EGG_TARGETS",11);
  if (srcc>0) {
    int srcp=0;
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
      const char *token=src+srcp;
      int tokenc=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
      if (builder_add_target(builder,token,tokenc)<0) return -1;
    }
  }
  if (builder->targetc<1) return builder_error(builder,"%s: No build targets configured.\n",g.exename);
  
  /**
  {
    fprintf(stderr,"%s: Acquired %d targets:\n",__func__,builder->targetc);
    struct builder_target *target=builder->targetv;
    int i=builder->targetc;
    for (;i-->0;target++) {
      fprintf(stderr,"  %.*s:\n",target->namec,target->name);
      #define FLD(tag) fprintf(stderr,"    %s: %s\n",#tag,target->tag);
      FLD(opt)
      FLD(cc)
      FLD(ld)
      FLD(ldpost)
      FLD(pkg)
      FLD(exesfx)
      #undef FLD
    }
  }
  /**/
  
  return 0;
}
