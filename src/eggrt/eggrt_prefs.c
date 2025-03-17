#include "eggrt_internal.h"

int eggrt_prefs_init() {

  //TODO Acquire language per system and rom. Rom is loaded by this point.
  eggrt.lang=EGG_LANG_FROM_STRING("en");
  
  //TODO Disable music and sound by default if there's a dummy audio driver.
  eggrt.music_enable=1;
  eggrt.sound_enable=1;
  
  return 0;
}
