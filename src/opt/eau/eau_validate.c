#include "eau.h"
#include <string.h>
#include <stdint.h>

/* Validate post.
 */
 
static const char *eau_validate_post(const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    uint8_t type=src[srcp++];
    if (srcp>=srcc) return "Unexpected EOF in Channel Headers post block.";
    uint8_t len=src[srcp++];
    if (srcp>srcc-len) return "Unexpected EOF in Channel Headers post block.";
    // TODO Could validate individual post stages here. Probably not worth the trouble.
    srcp+=len;
  }
  return 0;
}

/* Validate Channel Headers.
 */
 
static const char *eau_validate_chdr(const uint8_t *src,int srcc) {
  uint8_t usage[32]={0};
  const char *msg;
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return "Unexpected EOF in Channel Headers.";
    uint8_t chid=src[srcp++];
    int bytep=chid>>3;
    uint8_t mask=1<<(chid&7);
    if (usage[bytep]&mask) return "Duplicate Channel Header.";
    usage[bytep]|=mask;
    srcp+=2; // trim, pan, anything goes.
    uint8_t mode=src[srcp++];
    switch (mode) {
      case 0: case 1: case 2: case 3: case 4: break;
      default: return "Unknown mode in Channel Headers.";
    }
    int modecfglen=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-modecfglen) return "Unexpected EOF in Channel Headers.";
    // TODO Could validate modecfg here, but I'm not sure it's worth the trouble.
    srcp+=modecfglen;
    if (srcp>srcc-2) return "Unexpected EOF in Channel Headers.";
    int postlen=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-postlen) return "Unexpected EOF in Channel Headers.";
    if (msg=eau_validate_post(src+srcp,postlen)) return msg;
    srcp+=postlen;
  }
  return 0;
}

/* Validate Events.
 */
 
static const char *eau_validate_evts(const uint8_t *src,int srcc) {
  //TODO Might be nice of us to confirm that channels were initialized?
  int srcp=0;
  while (srcp<srcc) {
    uint8_t lead=src[srcp++];
    if (!(lead&0x80)) {
      // Delay. Anything goes, and no need to track.
      continue;
    }
    switch (lead&0xf0) {
      case 0x80: case 0x90: case 0xe0: { // 2-byte MIDI events.
          if (srcp>srcc-2) return "Unexpected EOF in Events.";
          if ((src[srcp]&0x80)||(src[srcp+1]&0x80)) return "MIDI event payload has a high bit set.";
          srcp+=2;
        } break;
      case 0xa0: { // Note Once. Every payload is valid, just check length.
          if (srcp>srcc-3) return "Unexpected EOF in Events.";
          srcp+=3;
        } break;
      case 0xf0: break; // Marker. Single byte and everything is valid.
      default: return "Unknown opcode in Events.";
    }
  }
  return 0;
}

/* Validate Text.
 */
 
static const char *eau_validate_text(const uint8_t *src,int srcc) {
  //TODO I guess we ought to check for duplicates. That's a little complicated.
  int srcp=0;
  while (srcp<srcc) {
    uint8_t chid=src[srcp++];
    if (srcp>=srcc) return "Unexpected EOF in Text chunk.";
    uint8_t noteid=src[srcp++];
    if ((noteid>=0x80)&&(noteid<0xff)) return "Invalid noteid in Text chunk."; // 0xff is legal, meaning the channel itself, but otherwise must be a MIDI note.
    if (srcp>=srcc) return "Unexpected EOF in Text chunk.";
    int len=src[srcp++];
    if (srcp>srcc-len) return "Unexpected EOF in Text chunk.";
    for (;len-->0;srcp++) {
      if ((src[srcp]<0x20)||(src[srcp]>0x7e)) return "Text chunk contains a byte outside ASCII G0.";
    }
  }
  return 0;
}

/* Validate EAU, main entry point.
 */
 
const char *eau_validate(const void *src,int srcc) {
  if (!src) return "Null input.";
  if (srcc<1) return "Empty input.";
  const uint8_t *SRC=src;
  int srcp=0;
  const char *msg;
  
  if ((srcp>srcc-4)||memcmp(SRC+srcp,"\0EAU",4)) return "EAU signature mismatch.";
  srcp+=4;
  
  if (srcp>srcc-2) return "Missing tempo.";
  int tempo=(SRC[srcp]<<8)|SRC[srcp+1];
  if ((tempo<1)||(tempo>0x7fff)) return "Tempo not in 1..32767.";
  srcp+=2;
  
  if (srcp>=srcc) return 0; // ok to stop before any of the length-prefixed chunks.
  if (srcp>srcc-4) return "Unexpected EOF at Channel Headers length.";
  int chunkc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((chunkc<0)||(srcp>srcc-chunkc)) return "Invalid Channel Headers length.";
  if (msg=eau_validate_chdr(SRC+srcp,chunkc)) return msg;
  srcp+=chunkc;
  
  if (srcp>=srcc) return 0; // ok to stop before any of the length-prefixed chunks.
  if (srcp>srcc-4) return "Unexpected EOF at Events length.";
  chunkc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((chunkc<0)||(srcp>srcc-chunkc)) return "Invalid Events length.";
  if (msg=eau_validate_evts(SRC+srcp,chunkc)) return msg;
  srcp+=chunkc;
  
  if (srcp>=srcc) return 0; // ok to stop before any of the length-prefixed chunks.
  if (srcp>srcc-4) return "Unexpected EOF at Text length.";
  chunkc=(SRC[srcp]<<24)|(SRC[srcp+1]<<16)|(SRC[srcp+2]<<8)|SRC[srcp+3];
  srcp+=4;
  if ((chunkc<0)||(srcp>srcc-chunkc)) return "Invalid Text length.";
  if (msg=eau_validate_text(SRC+srcp,chunkc)) return msg;
  srcp+=chunkc;

  return 0;
}
