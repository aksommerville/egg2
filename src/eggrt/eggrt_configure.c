#include "eggrt_internal.h"
#include "opt/serial/serial.h"

/* --help
 */
  char *video_driver;
  int fullscreen;
  char *video_device;
  char *audio_driver;
  int audio_rate;
  int audio_chanc;
  int audio_buffer;
  char *audio_device;
  char *input_driver;
 
void eggrt_print_help(const char *topic,int topicc) {
  fprintf(stderr,"\nUsage: %s [OPTIONS]\n",eggrt.exename);
  fprintf(stderr,
    "\n"
    "OPTIONS:\n"
    "  --help                   Print this message and exit.\n"
    "  --video=DRIVER           Select driver manually (see below).\n"
    "  --fullscreen             Start in fullscreen mode.\n"
    "  --video-device=NAME      Depends on driver.\n"
    "  --audio=DRIVER           Select driver manually (see below).\n"
    "  --audio-rate=HZ          Suggest audio output rate.\n"
    "  --audio-chanc=1|2        Suggest audio channel count.\n"
    "  --stereo                 --audio-chanc=2\n"
    "  --mono                   --audio-chanc=1\n"
    "  --audio-buffer=FRAMES    Suggest audio buffer size in frames.\n"
    "  --audio-device=NAME      Depends on driver.\n"
    "  --input=DRIVER           Select driver manually (see below).\n"
    "\n"
  );
  int i;
  fprintf(stderr,"Video drivers:\n");
  for (i=0;;i++) {
    const struct hostio_video_type *type=hostio_video_type_by_index(i);
    if (!type) break;
    fprintf(stderr,"  %12s: %s\n",type->name,type->desc);
  }
  fprintf(stderr,"\n");
  fprintf(stderr,"Audio drivers:\n");
  for (i=0;;i++) {
    const struct hostio_audio_type *type=hostio_audio_type_by_index(i);
    if (!type) break;
    fprintf(stderr,"  %12s: %s\n",type->name,type->desc);
  }
  fprintf(stderr,"\n");
  fprintf(stderr,"Input drivers:\n");
  for (i=0;;i++) {
    const struct hostio_input_type *type=hostio_input_type_by_index(i);
    if (!type) break;
    fprintf(stderr,"  %12s: %s\n",type->name,type->desc);
  }
  fprintf(stderr,"\n");
}

/* Key=value arguments.
 */
 
static int eggrt_arg_kv(const char *k,int kc,const char *v,int vc) {
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
    eggrt_print_help(v,vc);
    eggrt.terminate=1;
    return 0;
  }
  
  //TODO Options.
  
  fprintf(stderr,"%s: Unexpected option '%.*s' = '%.*s'\n",eggrt.exename,kc,k,vc,v);
  return -2;
}

/* Positional arguments.
 */
 
static int eggrt_arg_positional(const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  //TODO ROM, if we ever do a Wasm runtime.
  
  fprintf(stderr,"%s: Unexpected argument '%.*s'\n",eggrt.exename,srcc,src);
  return -2;
}

/* Configure, main entry point.
 */
 
int eggrt_configure(int argc,char **argv) {

  eggrt.exename="egg";
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) eggrt.exename=argv[0];
  
  int argi=1,err;
  while (argi<argc) {
    const char *arg=argv[argi++];
    
    // Null or empty, skip it.
    if (!arg||!arg[0]) continue;
    
    // No dash, positional.
    if (arg[0]!='-') {
      if ((err=eggrt_arg_positional(arg,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Error applying argument '%s'.\n",eggrt.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Single dash alone, reserved.
    if (!arg[1]) goto _unexpected_;
    
    // Single dash: "-kvv" or "-k vv".
    if (arg[1]!='-') {
      char k=arg[1];
      const char *v=0;
      if (arg[2]) v=arg+2;
      else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
      if ((err=eggrt_arg_kv(&k,1,v,-1))<0) {
        if (err!=-2) fprintf(stderr,"%s: Error applying argument '%s'.\n",eggrt.exename,arg);
        return -2;
      }
      continue;
    }
    
    // Double dash alone or more than 2 dashes, reserved.
    if (!arg[2]) goto _unexpected_;
    if (arg[2]=='-') goto _unexpected_;
    
    // Double dash: "--kk=vv" or "--kk vv".
    const char *k=arg+2;
    int kc=0;
    while (k[kc]&&(k[kc]!='=')) kc++;
    const char *v=0;
    if (k[kc]=='=') v=k+kc+1;
    else if ((argi<argc)&&argv[argi]&&argv[argi][0]&&(argv[argi][0]!='-')) v=argv[argi++];
    if ((err=eggrt_arg_kv(k,kc,v,-1))<0) {
      if (err!=-2) fprintf(stderr,"%s: Error applying argument '%s'.\n",eggrt.exename,arg);
      return -2;
    }
    continue;
    
   _unexpected_:;
    fprintf(stderr,"%s: Unexpected argument '%s'\n",eggrt.exename,arg);
    return -2;
  }
  
  return 0;
}
