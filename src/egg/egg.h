/* egg.h
 * Egg Platform API.
 */
 
#ifndef EGG_H
#define EGG_H

//TODO

#define EGG_TID_metadata 1
#define EGG_TID_code 2
#define EGG_TID_strings 3
#define EGG_TID_image 4
#define EGG_TID_song 5
#define EGG_TID_sound 6
#define EGG_TID_tilesheet 7
#define EGG_TID_decalsheet 8
#define EGG_TID_map 9
#define EGG_TID_sprite 10
#define EGG_TID_FOR_EACH \
  _(metadata) \
  _(code) \
  _(strings) \
  _(image) \
  _(song) \
  _(sound) \
  _(tilesheet) \
  _(decalsheet) \
  _(map) \
  _(sprite)
  
#endif
