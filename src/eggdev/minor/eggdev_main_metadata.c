#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"

/* Temporary container.
 */
 
struct meta {
  struct meta_field {
    const char *k,*v; // Point directly into the rom.
    int kc,vc;
  } *v;
  int c,a;
};

static void meta_cleanup(struct meta *meta) {
  if (meta->v) free(meta->v);
}

static int meta_add(struct meta *meta,const char *k,int kc,const char *v,int vc) {

  // If keys were requested globally and this isn't one of them, drop it.
  // That's more efficient than filtering later, and we definitely don't need the unrequested ones.
  if (g.srcpathc>1) {
    int match=0,i=1;
    for (;i<g.srcpathc;i++) {
      if (memcmp(g.srcpathv[i],k,kc)) continue;
      if (g.srcpathv[i][kc]) continue;
      match=1;
      break;
    }
    if (!match) return 0;
  }

  if (meta->c>=meta->a) {
    int na=meta->a+32;
    if (na>INT_MAX/sizeof(struct meta_field)) return -1;
    void *nv=realloc(meta->v,sizeof(struct meta_field)*na);
    if (!nv) return -1;
    meta->v=nv;
    meta->a=na;
  }
  struct meta_field *field=meta->v+meta->c++;
  field->k=k;
  field->kc=kc;
  field->v=v;
  field->vc=vc;
  return 0;
}

/* Extract from strings resource by index.
 */
 
static int eggdev_meta_strings_get(void *dstpp,const uint8_t *src,int srcc,int index) {
  if (index<1) return 0;
  if ((srcc<4)||memcmp(src,"\0EST",4)) return 0;
  int srcp=4,stopp=srcc-2;
  while (srcp<stopp) {
    int len=src[srcp++]<<8;
    len|=src[srcp++];
    if (srcp>srcc-len) break;
    if (!--index) {
      *(const void**)dstpp=src+srcp;
      return len;
    }
    srcp+=len;
  }
  return 0;
}

/* Print multi-language field, text.
 */
 
static void eggdev_metadata_print_all_langs_text(const struct meta_field *field,const void *rom,int romc) {
  int index,gotc=0;
  if ((sr_int_eval(&index,field->v,field->vc)<2)||(index<1)||(index>1024)) {
    fprintf(stdout,"%.*s=%.*s\n",field->kc,field->k,field->vc,field->v);
    return;
  }
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,rom,romc)>=0) {
    struct eggdev_res res;
    while (eggdev_rom_reader_next(&res,&reader)>0) {
      if (res.tid>EGG_TID_strings) break;
      if (res.tid<EGG_TID_strings) continue;
      if ((res.rid&0x3f)!=1) continue; // We want "1", with any language in the high 10.
      const char *v=0;
      int vc=eggdev_meta_strings_get(&v,res.v,res.c,index);
      if (vc<1) continue;
      char hi='a'+((res.rid>>11)-1);
      char lo='a'+(((res.rid>>6)&0x1f)-1);
      if ((hi<'a')||(hi>'z')||(lo<'a')||(lo>'z')) continue;
      fprintf(stdout,"%.*s[%c%c]=%.*s\n",field->kc,field->k,hi,lo,vc,v);
      gotc++;
    }
  }
  if (!gotc) {
    fprintf(stdout,"%.*s=%.*s\n",field->kc,field->k,field->vc,field->v);
  }
}

/* Print language field, text.
 */
 
static void eggdev_metadata_print_lang_text(const struct meta_field *field,const void *rom,int romc,int lang) {
  int index,gotc=0;
  if ((sr_int_eval(&index,field->v,field->vc)<2)||(index<1)||(index>1024)) {
    fprintf(stdout,"%.*s=%.*s\n",field->kc,field->k,field->vc,field->v);
    return;
  }
  char langrepr[3]={
    'a'+((lang>>5)-1),
    'a'+((lang&0x1f)-1),
    0,
  };
  int rid=(lang<<6)|1;
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,rom,romc)>=0) {
    struct eggdev_res res;
    while (eggdev_rom_reader_next(&res,&reader)>0) {
      if (res.tid>EGG_TID_strings) break;
      if (res.tid<EGG_TID_strings) continue;
      if (res.rid>rid) break;
      if (res.rid<rid) continue;
      const char *v=0;
      int vc=eggdev_meta_strings_get(&v,res.v,res.c,index);
      if (vc<1) continue;
      fprintf(stdout,"%.*s[%s]=%.*s\n",field->kc,field->k,langrepr,vc,v);
      gotc++;
    }
  }
  if (!gotc) {
    fprintf(stdout,"%.*s=%.*s\n",field->kc,field->k,field->vc,field->v);
  }
}

/* Print, text format.
 */
 
static void eggdev_metadata_print_text(const struct meta *meta,const void *rom,int romc,int lang) {
  const struct meta_field *field=meta->v;
  int i=meta->c;
  for (;i-->0;field++) {
    if (lang&&(field->kc>0)&&(field->k[field->kc-1]=='$')) {
      if (lang<0) eggdev_metadata_print_all_langs_text(field,rom,romc);
      else eggdev_metadata_print_lang_text(field,rom,romc,lang);
    } else {
      fprintf(stdout,"%.*s=%.*s\n",field->kc,field->k,field->vc,field->v);
    }
  }
}

/* Dump metadata, having located the resource.
 */
 
static int eggdev_metadata_dump(struct meta *meta,const uint8_t *src,int srcc,const char *path,const void *rom,int romc) {
  if ((srcc<4)||memcmp(src,"\0EMD",4)) {
    fprintf(stderr,"%s: Signature mismatch.\n",path);
    return -2;
  }
  int srcp=4;
  for (;;) {
    if (srcp>=srcc) {
      fprintf(stderr,"%s: Missing required terminator.\n",path);
      return -2;
    }
    uint8_t kc=src[srcp++];
    if (!kc) break;
    if (srcp>=srcc) {
      fprintf(stderr,"%s: Unexpected EOF.\n",path);
      return -2;
    }
    uint8_t vc=src[srcp++];
    if (srcp>srcc-vc-kc) {
      fprintf(stderr,"%s: Unexpected EOF.\n",path);
      return -2;
    }
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    if (meta_add(meta,k,kc,v,vc)<0) return -1;
  }
  
  int lang=0;
  if (g.lang) {
    if (!strcmp(g.lang,"all")) lang=-1;
    else if ((g.lang[0]<'a')||(g.lang[0]>'z')||(g.lang[1]<'a')||(g.lang[1]>'z')||g.lang[2]) {
      fprintf(stderr,"%s: Invalid language '%s'. Expected two lowercase letters or 'all'.\n",g.exename,g.lang);
      return -2;
    } else lang=((g.lang[0]-'a'+1)<<5)|(g.lang[1]-'a'+1);
  }
  
  eggdev_metadata_print_text(meta,rom,romc,lang);
  return 0;
}

/* Metadata, main entry point.
 */
 
int eggdev_main_metadata() {

  // Acquire input.
  if (g.srcpathc<1) {
    fprintf(stderr,"%s: Input path required.\n",g.exename);
    return -2;
  }
  void *src=0;
  int srcc=eggdev_read_input(&src,g.srcpathv[0]);
  if (srcc<0) {
    if (srcc!=2) fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  
  // If it's not an Egg ROM, convert it.
  int srcfmt=eggdev_fmt_by_signature(src,srcc);
  if (!srcfmt) srcfmt=eggdev_fmt_by_path(g.srcpathv[0],-1);
  if (!srcfmt) srcfmt=EGGDEV_FMT_exe; // "exe" for a generic search thru the file for embedded rom.
  if (srcfmt!=EGGDEV_FMT_egg) {
    sr_convert_fn convert=eggdev_get_converter(EGGDEV_FMT_egg,srcfmt);
    if (!convert) {
      free(src);
      fprintf(stderr,"%s: Not an Egg ROM and no converter found.\n",g.srcpathv[0]);
      return -2;
    }
    struct sr_encoder dst={0};
    struct sr_convert_context ctx={
      .dst=&dst,
      .src=src,
      .srcc=srcc,
      .refname=g.srcpathv[0],
    };
    int err=convert(&ctx);
    if (err<0) {
      sr_encoder_cleanup(&dst);
      free(src);
      if (err!=-2) fprintf(stderr,"%s: Unspecified error converting ROM.\n",g.srcpathv[0]);
      return -2;
    }
    free(src);
    src=dst.v;
    srcc=dst.c;
  }
  
  // Locate metadata:1. We could cheat and just assume that it starts 7 bytes in, but let's set a good example.
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,src,srcc)<0) {
    free(src);
    fprintf(stderr,"%s: Malformed ROM.\n",g.srcpathv[0]);
    return -2;
  }
  struct eggdev_res res;
  int err,done=0;
  while ((err=eggdev_rom_reader_next(&res,&reader))>0) {
    if ((res.tid==EGG_TID_metadata)&&(res.rid==1)) {
      struct meta meta={0};
      err=eggdev_metadata_dump(&meta,res.v,res.c,g.srcpathv[0],src,srcc);
      meta_cleanup(&meta);
      done=1;
      break;
    }
  }
  if (err<0) {
    free(src);
    if (err!=-2) fprintf(stderr,"%s: Malformed ROM.\n",g.srcpathv[0]);
    return -2;
  }
  if (!done) {
    free(src);
    fprintf(stderr,"%s: metadata:1 not found (ROM is invalid).\n",g.srcpathv[0]);
    return -2;
  }
  
  free(src);
  return 0;
}
