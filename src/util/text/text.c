#include "text.h"
#include "util/res/res.h"
#include "egg/egg.h"
#include <limits.h>

/* Globals.
 * An argument could be made for indexing all the strings.
 * Then lookups could be much faster, especially in ROMs with a huge count of strings.
 * But then we would need stdlib, would consume much more memory, and the logic would be much more complicated.
 * I think our linear-search-on-demand strategy is prudent.
 */
 
static struct {
  struct rom_reader rom; // Empty, or will "next" to the first strings resource.
} gtext={0};

/* Set ROM.
 */
 
void text_set_rom(const void *src,int srcc) {
  __builtin_memset(&gtext.rom,0,sizeof(struct rom_reader));
  struct rom_reader reader;
  if (rom_reader_init(&reader,src,srcc)<0) return;
  // Keep the previous reader state in (g.text.rom), then stop reading when we find a strings.
  gtext.rom=reader;
  struct rom_entry res;
  while (rom_reader_next(&res,&reader)>0) {
    if (res.tid>=EGG_TID_strings) return; // sic ">=": Stop anywhere if there aren't any strings.
    gtext.rom=reader;
  }
}

/* Get strings resource from our partial ROM.
 */
 
static int text_get_res(void *dstpp,int rid) {
  struct rom_reader reader=gtext.rom;
  struct rom_entry res;
  while (rom_reader_next(&res,&reader)>0) {
    if (res.tid>EGG_TID_strings) return 0;
    if (res.rid==rid) {
      *(const void**)dstpp=res.v;
      return res.c;
    }
  }
  return 0;
}

/* Get a string.
 */

int text_get_string(void *dstpp,int rid,int strix) {
  if (strix<1) return 0;
  if (rid<1) return 0;
  if (rid<0x40) rid|=egg_prefs_get(EGG_PREF_LANG)<<6;
  const void *src=0;
  int srcc=text_get_res(&src,rid);
  struct strings_reader reader;
  if (strings_reader_init(&reader,src,srcc)<0) return 0;
  struct strings_entry entry;
  while (strings_reader_next(&entry,&reader)>0) {
    if (entry.index>strix) return 0;
    if (entry.index==strix) {
      *(const void**)dstpp=entry.v;
      return entry.c;
    }
  }
  return 0;
}

/* Format string.
 */
 
int text_format(
  char *dst,int dsta,
  const char *fmt,int fmtc,
  const struct text_insertion *insv,int insc
) {
  if (!dst||(dsta<0)) return 0;
  if (!fmt) fmtc=0; else if (fmtc<0) { fmtc=0; while (fmt[fmtc]) fmtc++; }
  int dstc=0,fmtp=0;
  for (;;) {
    const char *raw=fmt+fmtp;
    int rawc=0;
    while ((fmtp<fmtc)&&(fmt[fmtp]!='%')) { fmtp++; rawc++; }
    if (rawc) {
      if (dstc>dsta-rawc) rawc=dsta-dstc;
      __builtin_memcpy(dst+dstc,raw,rawc);
      dstc+=rawc;
      if (dstc>=dsta) return dstc;
    }
    if (fmtp>=fmtc) break;
    fmtp++; // Skip introducer.
    if (fmtp>=fmtc) { // Trailing '%'. Emit it verbatim.
      if (dstc<dsta) dst[dstc++]='%';
      break;
    }
    char marker=fmt[fmtp++];
    if (marker=='%') { // Escaped introducer.
      if (dstc<dsta) dst[dstc++]='%';
    } else if ((marker>='0')&&(marker<='9')) { // Insertion.
      int insp=marker-'0';
      if (insp<insc) { // Invalid insertion reference is fine, emit nothing.
        const struct text_insertion *ins=insv+insp;
        int addc=0;
        switch (ins->mode) {
          case 'i': addc=text_decsint_repr(dst+dstc,dsta-dstc,ins->i); break;
          case 't': addc=text_time_repr(dst+dstc,dsta-dstc,ins->t.s,ins->t.wholec,ins->t.fractc); break;
          case 's': {
              const char *src=ins->s.v;
              addc=ins->s.c;
              if (!src) addc=0; else if (addc<0) { addc=0; while (src[addc]) addc++; }
              if (dstc<=dsta-addc) __builtin_memcpy(dst+dstc,src,addc);
            } break;
          case 'r': {
              const char *src=0;
              addc=text_get_string(&src,ins->r.rid,ins->r.strix);
              if (dstc<=dsta-addc) __builtin_memcpy(dst+dstc,src,addc);
            } break;
          // Unknown mode, same as no-such-insertion: emit nothing.
        }
        if (dstc>dsta-addc) return dstc;
        dstc+=addc;
      }
    } else { // Unknown. Emit entire sequence.
      if (dstc<=dsta-2) {
        dst[dstc++]='%';
        dst[dstc++]=marker;
      }
    }
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Format string with resource, convenience.
 */
 
int text_format_res(
  char *dst,int dsta,
  int rid,int strix,
  const struct text_insertion *insv,int insc
) {
  const char *fmt=0;
  int fmtc=text_get_string(&fmt,rid,strix);
  return text_format(dst,dsta,fmt,fmtc,insv,insc);
}

/* Represent signed decimal integer.
 */

int text_decsint_repr(char *dst,int dsta,int src) {
  if (!dst||(dsta<0)) dsta=0;
  int dstc;
  if (src<0) {
    dstc=2;
    int limit=-10;
    while (limit>=src) { dstc++; if (limit<INT_MIN/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc;
    for (;i-->1;src/=10) dst[i]='0'-src%10;
    dst[0]='-';
  } else {
    dstc=1;
    int limit=10;
    while (limit<=src) { dstc++; if (limit>INT_MAX/10) break; limit*=10; }
    if (dstc>dsta) return dstc;
    int i=dstc;
    for (;i-->0;src/=10) dst[i]='0'+src%10;
  }
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

/* Represent time from floating-point seconds.
 */
 
int text_time_repr(char *dst,int dsta,double s,int wholec,int fractc) {
  if (!dst||(dsta<0)) dsta=0;
  if (fractc<0) fractc=0; else if (fractc>3) fractc=3;
  int ms=(int)(s*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  int min=sec/60; sec%=60;
  int hour=min/60; min%=60;
  
  // If (wholec) is negative, determine how many whole digits we actually need.
  if (wholec<0) {
    int limit=-wholec;
    if (limit<1) limit=1; else if (limit>7) limit=7;
         if (hour>=100) wholec=7;
    else if (hour>= 10) wholec=6;
    else if (hour>=  1) wholec=5;
    else if (min >= 10) wholec=4;
    else if (min >=  1) wholec=3;
    else if (sec >= 10) wholec=2;
    else                wholec=1;
    if (wholec>limit) wholec=limit;
  }
  
  // If we're over the limit, force everything to 9.
  if (wholec<1) wholec=1; else if (wholec>7) wholec=7;
  switch (wholec) {
    case 7: if (hour>999) { hour=ms=999; min=sec=99; } break;
    case 6: if (hour> 99) { hour=ms=999; min=sec=99; } break;
    case 5: if (hour>  9) { hour=ms=999; min=sec=99; } break;
    case 4: if (min > 99) { hour=ms=999; min=sec=99; } break;
    case 3: if (min >  9) { hour=ms=999; min=sec=99; } break;
    case 2: if (sec > 99) { hour=ms=999; min=sec=99; } break;
    case 1: if (sec >  9) { hour=ms=999; min=sec=99; } break;
  }
  
  // Determine output length.
  int dstc=wholec+fractc;
  if (fractc) dstc++; // dot
  if (wholec>=3) dstc++; // MS separator
  if (wholec>=5) dstc++; // HM separator
  if (dstc>dsta) return dstc;
  
  // Produce text.
  int dstp=0;
  if (wholec>=7) dst[dstp++]='0'+hour/100;
  if (wholec>=6) dst[dstp++]='0'+(hour/10)%10;
  if (wholec>=5) dst[dstp++]='0'+hour%10;
  if (wholec>=5) dst[dstp++]=':';
  if (wholec>=4) dst[dstp++]='0'+min/10;
  if (wholec>=3) dst[dstp++]='0'+min%10;
  if (wholec>=3) dst[dstp++]=':';
  if (wholec>=2) dst[dstp++]='0'+sec/10;
  dst[dstp++]='0'+sec%10;
  if (fractc) {
    dst[dstp++]='.';
    dst[dstp++]='0'+ms/100;
    if (fractc>=2) dst[dstp++]='0'+(ms/10)%10;
    if (fractc>=3) dst[dstp++]='0'+ms%10;
  }
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}
