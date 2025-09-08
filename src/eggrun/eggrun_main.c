/* eggrun_main.c
 * Owner of main() for the generic runtime.
 * We do the same stuff as eggrt_main.c, but also load the ROM file and provide glue for the client entry points.
 */

#include "eggrun_internal.h"
#include <sys/signal.h>

#if USE_macos
  #include "opt/macos/macos.h"
#endif

// Dummy embedded ROM so eggrt_rom knows to use the dynamic one.
const uint8_t _egg_embedded_rom[1]={0};
const int _egg_embedded_rom_size=0;

// This is the real thing; we'll populate it before initializing eggrt.
uint8_t *_egg_dynamic_rom=0;
int _egg_dynamic_rom_size=0;

/* Signal handler.
 */
 
static volatile int eggrt_sigc=0;
 
static void eggrt_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++eggrt_sigc>=3) {
        fprintf(stderr,"%s: Too many unprocessed signals.\n",eggrt.exename);
        exit(1);
      } break;
  }
}

/* Lifecycle hooks for MacOS only.
 */
#if USE_macos

static void eggrt_cb_mac_quit(void *userdata) {
  eggrt_quit(0);
}

static int eggrt_cb_mac_init(void *userdata) {
  return eggrt_init();
}

static void eggrt_cb_mac_update(void *userdata) {
  if (eggrt.terminate||eggrt_sigc) {
    macioc_terminate(0);
    return;
  }
  int err=eggrt_update();
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error updating runtime.\n",eggrt.exename);
    macioc_terminate(1);
  }
}

#endif

/* Main.
 */

int main(int argc,char **argv) {
  eggrt.exename=((argc>=1)&&argv&&argv[0]&&argv[0][0])?argv[0]:"eggrun";

  #if USE_macos
    argc=macos_prerun_argv(argc,argv);
  #endif
  
  /* Before eggrt_configure, locate the ROM path and load it.
   */
  if ((_egg_dynamic_rom_size=eggrun_load_file(&_egg_dynamic_rom,argc,argv))<0) {
    if (_egg_dynamic_rom_size!=-2) fprintf(stderr,"%s: Unspecified error loading ROM file.\n",eggrt.exename);
    return 1;
  }
  int err=eggrun_boot(_egg_dynamic_rom,_egg_dynamic_rom_size,eggrun_rom_path);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error instantiating Wasm runtime.\n",eggrt.exename);
    return 1;
  }

  if ((err=eggrt_configure(argc,argv))<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error reading configuration.\n",eggrt.exename);
    return 1;
  }
  if (eggrt.terminate) return 0;
  
  signal(SIGINT,eggrt_signal);

  /* MacOS has its own IoC concept.
   */
  #if USE_macos
    return macos_main(argc,argv,eggrt_cb_mac_quit,eggrt_cb_mac_init,eggrt_cb_mac_update,0);
  #else
  
    if ((err=eggrt_init())<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error during initialization.\n",eggrt.exename);
      eggrt_quit(1);
      return 1;
    }
  
    while (!eggrt.terminate&&!eggrt_sigc) {
      if ((err=eggrt_update())<0) {
        if (err!=-2) fprintf(stderr,"%s: Unspecified error updating runtime.\n",eggrt.exename);
        eggrt_quit(1);
        return 1;
      }
    }
  
    eggrt_quit(0);
    return 0;
  #endif
}
