#include "eau.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <stdio.h>

/* Guess format.
 */
 
int eau_guess_format(const void *src,int srcc,const char *path) {

  // If (path) has a known suffix, case-insensitive, we trust it (even if we might know better).
  if (path) {
    const char *presfx=0;
    int presfxc=0,pathp=0;
    for (;path[pathp];pathp++) {
      if (path[pathp]=='/') {
        presfx=0;
        presfxc=0;
      } else if (path[pathp]=='.') {
        presfx=path+pathp+1;
        presfxc=0;
      } else if (presfx) {
        presfxc++;
      }
    }
    char sfx[16];
    if ((presfxc>0)&&(presfxc<=sizeof(sfx))) {
      int i=presfxc;
      while (i-->0) {
        if ((presfx[i]>='A')&&(presfx[i]<='Z')) sfx[i]=presfx[i]+0x20;
        else sfx[i]=presfx[i];
      }
      switch (presfxc) {
        case 3: {
            if (!memcmp(sfx,"eau",3)) return EAU_FORMAT_EAU;
            if (!memcmp(sfx,"mid",3)) return EAU_FORMAT_MIDI;
          } break;
        case 4: {
            if (!memcmp(sfx,"eaut",4)) return EAU_FORMAT_EAUT;
            if (!memcmp(sfx,"midi",4)) return EAU_FORMAT_MIDI; // I've never seen them like this, but would feel like a fool if we didn't acknowledge it.
          } break;
      }
    }
  }
  
  // EAU and MIDI have unambiguous signatures.
  if (src) {
    if ((srcc>=4)&&!memcmp(src,"\0EAU",4)) return EAU_FORMAT_EAU;
    if ((srcc>=4)&&!memcmp(src,"MThd",4)) return EAU_FORMAT_MIDI;
  }
  
  // EAUT or UNKNOWN. If the first kilobyte is G0 and LF, call it EAUT.
  if (src) {
    const uint8_t *SRC=src;
    int i=1024;
    if (i>srcc) i=srcc;
    for (;i-->0;SRC++) {
      if (*SRC==0x0a) continue;
      if ((*SRC>=0x20)&&(*SRC<=0x7e)) continue;
      return EAU_FORMAT_UNKNOWN;
    }
    return EAU_FORMAT_EAUT;
  }
  
  return EAU_FORMAT_UNKNOWN;
}

/* Convert whatever to whatever.
 */

int eau_convert(
  struct sr_encoder *dst,int dstfmt,
  const void *src,int srcc,int srcfmt,
  const char *path,eau_get_chdr_fn get_chdr
) {

  if (srcfmt==EAU_FORMAT_UNKNOWN) {
    srcfmt=eau_guess_format(src,srcc,path);
    if (srcfmt==EAU_FORMAT_UNKNOWN) {
      if (!path) return -1;
      fprintf(stderr,"%s: Unable to determine song format. Expected EAU, EAU-Text, or MIDI.\n",path);
      return -2;
    }
  }
  
  if (dstfmt==EAU_FORMAT_UNKNOWN) {
    if (srcfmt==EAU_FORMAT_EAU) dstfmt=EAU_FORMAT_MIDI;
    else dstfmt=EAU_FORMAT_EAU;
  }

  if (srcfmt==dstfmt) return sr_encode_raw(dst,src,srcc);
  
  switch (dstfmt) {
    case EAU_FORMAT_EAU: switch (srcfmt) {
        case EAU_FORMAT_EAUT: return eau_cvt_eau_eaut(dst,src,srcc,path,get_chdr);
        case EAU_FORMAT_MIDI: return eau_cvt_eau_midi(dst,src,srcc,path,get_chdr);
      } break;
    case EAU_FORMAT_EAUT: switch (srcfmt) {
        case EAU_FORMAT_EAU: return eau_cvt_eaut_eau(dst,src,srcc,path,get_chdr);
        case EAU_FORMAT_MIDI: {
            struct sr_encoder eau={0};
            int err=eau_cvt_eau_midi(&eau,src,srcc,path,get_chdr);
            if (err<0) {
              sr_encoder_cleanup(&eau);
              return err;
            }
            err=eau_cvt_eaut_eau(dst,eau.v,eau.c,path,get_chdr);
            sr_encoder_cleanup(&eau);
            return err;
          }
      } break;
    case EAU_FORMAT_MIDI: switch (srcfmt) {
        case EAU_FORMAT_EAU: return eau_cvt_midi_eau(dst,src,srcc,path,get_chdr);
        case EAU_FORMAT_EAUT: {
            struct sr_encoder eau={0};
            int err=eau_cvt_eau_eaut(&eau,src,srcc,path,get_chdr);
            if (err<0) {
              sr_encoder_cleanup(&eau);
              return err;
            }
            err=eau_cvt_midi_eau(dst,eau.v,eau.c,path,get_chdr);
            sr_encoder_cleanup(&eau);
            return err;
          }
      } break;
  }
  if (!path) return -1;
  fprintf(stderr,"%s: Conversion %d => %d undefined [%s:%d]\n",path,srcfmt,dstfmt,__FILE__,__LINE__);
  return -2;
}
