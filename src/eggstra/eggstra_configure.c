#include "eggstra.h"
#include "opt/serial/serial.h"

/* --help
 */
 
void eggstra_print_help(const char *topic,int topicc) {
  if (!topic) topicc=0; else if (topicc<0) { topicc=0; while (topic[topicc]) topicc++; }
  fprintf(stderr,"\nUsage: %s COMMAND [OPTIONS|FILES...]\n\n",eggstra.exename);
  
  if (topicc) {
    fprintf(stderr,"Unknown help topic '%.*s'.\n\n",topicc,topic);
  } else {
    fprintf(stderr,
      "OPTIONS:\n"
      "  --help                  Print this message.\n"
      "\n"
      "COMMANDS:\n"
      "  play SOUNDFILE [--repeat] [--rate=HZ] [--chanc=1|2] [--driver=NAME] [--device=NAME] [--buffer=INT]\n"
      "\n"
    );
    /* TODO Possible other commands:
     *   show IMAGEFILE [--zoom=INT] [--driver=NAME] [--device=NAME]
     *   input [--driver=NAME] [--device=NAME] [--headers] [--events] [--quiet-axes]
     *   drivers
     */
  }
}

/* Add positional and dashed arguments.
 */
 
static int eggstra_srcpathv_add(const char *src) {
  if (eggstra.srcpathc>=eggstra.srcpatha) {
    int na=eggstra.srcpatha+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eggstra.srcpathv,sizeof(void*)*na);
    if (!nv) return -1;
    eggstra.srcpathv=nv;
    eggstra.srcpatha=na;
  }
  eggstra.srcpathv[eggstra.srcpathc++]=src;
  return 0;
}

static int eggstra_optv_add(const char *src) {
  while (src[0]=='-') src++;
  if (eggstra.optc>=eggstra.opta) {
    int na=eggstra.opta+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(eggstra.optv,sizeof(void*)*na);
    if (!nv) return -1;
    eggstra.optv=nv;
    eggstra.opta=na;
  }
  eggstra.optv[eggstra.optc++]=src;
  return 0;
}

/* Receive command line, main entry point.
 */
 
int eggstra_configure(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggstra.exename=argv[0];
  else eggstra.exename="eggstra";
  int argi=1;
  while (argi<argc) {
    const char *arg=argv[argi++];
    if (!arg||!arg[0]) continue;
    
    if (!strcmp(arg,"--help")) {
      eggstra_print_help(0,0);
      eggstra.sigc=1;
      return 0;
    }
    if (!memcmp(arg,"--help=",7)) {
      eggstra_print_help(arg+7,-1);
      eggstra.sigc=1;
      return 0;
    }
    
    if (arg[0]=='-') {
      if (eggstra_optv_add(arg)<0) return -1;
      continue;
    }
    if (!eggstra.command) {
      eggstra.command=arg;
      continue;
    }
    if (eggstra_srcpathv_add(arg)<0) return -1;
  }
  return 0;
}

/* Get option.
 */
 
const char *eggstra_opt(const char *k,int kc) {
  if (!k) return 0;
  if (kc<0) { kc=0; while (k[kc]) kc++; }
  int i=0; for (;i<eggstra.optc;i++) {
    const char *opt=eggstra.optv[i];
    if (memcmp(opt,k,kc)) continue;
    if (!opt[kc]) return "";
    if (opt[kc]!='=') continue;
    return opt+kc+1;
  }
  return 0;
}

int eggstra_opti(const char *k,int kc,int fallback) {
  const char *src=eggstra_opt(k,kc);
  if (!src) return fallback;
  int v;
  if (sr_int_eval(&v,src,-1)<1) return fallback;
  return v;
}
