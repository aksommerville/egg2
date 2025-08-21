#include "eggdev_internal.h"

struct g g={0};

/* Print one help topic.
 */
 
static int eggdev_print_help_topic(const char *src,int srcc,const char *topic,int topicc) {
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec;
  
  /* The special topic "topics" means print all the double-hash headers.
   */
  if ((topicc==6)&&!memcmp(topic,"topics",6)) {
    fprintf(stderr,"\n%s: Help topics:\n",g.exename);
    while ((linec=sr_decode_line(&line,&decoder))>0) {
      if ((linec>=3)&&!memcmp(line,"## ",3)) {
        line+=3;
        linec-=3;
        while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
        while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
        fprintf(stderr," - %.*s\n",linec,line);
      }
    }
    fprintf(stderr,"\n");
    return 0;
  }
  
  /* Anything else must be the content of a double-hash header.
   * Print everything below it, to the next header.
   */
  int matched=0;
  while ((linec=sr_decode_line(&line,&decoder))>0) {
    if ((linec>=3)&&!memcmp(line,"## ",3)) {
      if (matched) break;
      line+=3;
      linec-=3;
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
      if ((linec==topicc)&&!memcmp(line,topic,topicc)) matched=1;
    } else if (matched) {
      while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
      fprintf(stderr,"%.*s\n",linec,line);
    }
  }
  if (matched) return 0;
  
  fprintf(stderr,"\n%s: Unknown help topic '%.*s'. Try '--help=topics' for a list.\n\n",g.exename,topicc,topic);
  return 0;
}

/* --help
 */
 
void eggdev_print_help(const char *topic,int topicc) {
  if (!topic) topicc=0; else if (topicc<0) { topicc=0; while (topic[topicc]) topicc++; }
  if ((topicc==1)&&(topic[0]=='1')) topicc=0; // "=1" gets inserted generically for empty values, drop it.
  if (topicc) {
    char path[1024];
    int pathc=snprintf(path,sizeof(path),"%s/etc/doc/eggdev-cli.md",g.sdkpath);
    if ((pathc>0)&&(pathc<sizeof(path))) {
      char *src=0;
      int srcc=file_read(&src,path);
      if (srcc>=0) {
        int err=eggdev_print_help_topic(src,srcc,topic,topicc);
        free(src);
        if (err>=0) return;
      }
    }
  }
  // No topic, or one we didn't recognize.
  fprintf(stderr,"\nUsage: %s COMMAND [-oOUTPUT] [OPTIONS...] [INPUT...]\n\n",g.exename);
  fprintf(stderr,"Try '--help=topics' for more.\n\n");
}

/* Evaluate command.
 */
 
int eggdev_command_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return EGGDEV_COMMAND_##tag;
  EGGDEV_COMMAND_FOR_EACH
  #undef _
  return -1;
}

const char *eggdev_command_repr(int command) {
  switch (command) {
    #define _(tag) case EGGDEV_COMMAND_##tag: return #tag;
    EGGDEV_COMMAND_FOR_EACH
    #undef _
  }
  return "?";
}

/* Append to srcpathv.
 */
 
static int eggdev_append_srcpathv(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (g.srcpathc>=g.srcpatha) {
    int na=g.srcpatha+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(g.srcpathv,sizeof(void*)*na);
    if (!nv) return -1;
    g.srcpathv=nv;
    g.srcpatha=na;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  g.srcpathv[g.srcpathc++]=nv;
  return 0;
}

/* Append to htdocsv.
 */
 
static int eggdev_append_htdocs(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (g.htdocsc>=g.htdocsa) {
    int na=g.htdocsa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(g.htdocsv,sizeof(void*)*na);
    if (!nv) return -1;
    g.htdocsv=nv;
    g.htdocsa=na;
  }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  g.htdocsv[g.htdocsc++]=nv;
  return 0;
}

/* Set string argument.
 */
 
static int eggdev_set_string(char **dst,const char *src,int srcc,const char *k,int kc) {
  if (*dst) {
    fprintf(stderr,"%s: Multiple values for '%.*s'\n",g.exename,kc,k);
    return -2;
  }
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=malloc(srcc+1);
  if (!nv) return -1;
  memcpy(nv,src,srcc);
  nv[srcc]=0;
  *dst=nv;
  return 0;
}

/* Key=value argument.
 */
 
static int eggdev_argv_kv(const char *k,int kc,const char *v,int vc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  if (!v) vc=0; else if (vc<0) { vc=0; while (v[vc]) vc++; }
  
  if (!v) {
    if ((kc>=3)&&!memcmp(k,"no-",3)) {
      k+=3;
      kc-=3;
      v="0";
      vc=1;
    } else {
      v="1";
      vc=1;
    }
  }
  int vn=0;
  sr_int_eval(&vn,v,vc);
  
  if ((kc==4)&&!memcmp(k,"help",4)) {
    eggdev_print_help(v,vc);
    g.terminate=1;
    return 0;
  }
  
  if (kc==1) switch (k[0]) {
    case 'o': return eggdev_set_string(&g.dstpath,v,vc,k,kc);
    case 'f': return eggdev_set_string(&g.format,v,vc,k,kc);
  }
  
  if ((kc==6)&&!memcmp(k,"dstfmt",6)) return eggdev_set_string(&g.dstfmt,v,vc,k,kc);
  if ((kc==6)&&!memcmp(k,"srcfmt",6)) return eggdev_set_string(&g.srcfmt,v,vc,k,kc);
  if ((kc==4)&&!memcmp(k,"port",4)) { g.port=vn; return 0; }
  if ((kc==15)&&!memcmp(k,"unsafe-external",15)) { g.unsafe_external=vn; return 0; }
  if ((kc==9)&&!memcmp(k,"writeable",9)) return eggdev_set_string(&g.writeable,v,vc,k,kc);
  if ((kc==7)&&!memcmp(k,"project",7)) return eggdev_set_string(&g.project,v,vc,k,kc);
  if ((kc==6)&&!memcmp(k,"htdocs",6)) return eggdev_append_htdocs(v,vc);
  if ((kc==4)&&!memcmp(k,"lang",4)) return eggdev_set_string(&g.lang,v,vc,k,kc);
  if ((kc==8)&&!memcmp(k,"verbatim",8)) { g.verbatim=vn; return 0; }
  if ((kc==6)&&!memcmp(k,"format",6)) return eggdev_set_string(&g.format,v,vc,k,kc);
  if ((kc==5)&&!memcmp(k,"strip",5)) { g.strip=vn; return 0; }
  if ((kc==4)&&!memcmp(k,"rate",4)) { g.rate=vn; return 0; }
  if ((kc==5)&&!memcmp(k,"chanc",5)) { g.chanc=vn; return 0; }
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",g.exename,kc,k,vc,v);
  return -2;
}

/* Positional argument.
 */
 
static int eggdev_argv_positional(const char *src) {
  if (!g.command) {
    if ((g.command=eggdev_command_eval(src,-1))<1) {
      fprintf(stderr,"%s: Unknown command '%s'. Try '--help'?\n",g.exename,src);
      return -2;
    }
  } else {
    if (eggdev_append_srcpathv(src,-1)<0) return -1;
  }
  return 0;
}

/* Configure from command line, main entry point.
 */
 
int eggdev_configure(int argc,char **argv) {
  int err;
  
  g.sdkpath=EGG_SDK;
  
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  else g.exename="eggdev";
  
  int argi=1; while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    // No dashes, or a single dash alone, is positional.
    if ((arg[0]!='-')||!arg[1]) {
      if ((err=eggdev_argv_positional(arg))<0) return err;
      continue;
    }
    
    // Single dash: '-kVV' or '-k V'.
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=eggdev_argv_kv(&k,1,v,-1))<0) return err;
      continue;
    }
    
    // Double dash alone, or >2 dashes, reserved.
    if (!arg[2]||(arg[2]=='-')) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",g.exename,arg);
      return -2;
    }
    
    // Double dash: '--kk=vv' or '--kk vv'.
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=eggdev_argv_kv(k,kc,v,-1))<0) return err;
  }
  return 0;
}
