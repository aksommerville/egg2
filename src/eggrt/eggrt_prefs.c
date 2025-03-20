#include "eggrt_internal.h"

/* Evaluate the first two characters of (src).
 * If not already present in (dst) and there's room, append it.
 * Returns the new (dstc).
 */
 
static int eggrt_lang_append(int *dst,int dsta,int dstc,const char *src) {
  if (dstc>=dsta) return dstc;
  if ((src[0]<'a')||(src[0]>'z')||(src[1]<'a')||(src[1]>'z')) return dstc;
  int lang=((src[0]-'a'+1)<<5)|(src[1]-'a'+1);
  int i=dstc; while (i-->0) {
    if (dst[i]==lang) return dstc;
  }
  dst[dstc++]=lang;
  return dstc;
}

/* List the system languages in preference order.
 * Never returns more than the size of the output vector.
 */
 
static int eggrt_get_sys_langs(int *dst,int dsta) {
  if (!dst||(dsta<1)) return 0;
  int dstc=0;
  
  /* If LANG starts with a valid code, that's the preferred language.
   */
  const char *LANG=getenv("LANG");
  if (LANG) dstc=eggrt_lang_append(dst,dsta,dstc,LANG);
  
  /* LANGUAGE may be a colon-delimited list of codes, in order.
   */
  const char *LANGUAGE=getenv("LANGUAGE");
  if (LANGUAGE) {
    int p=0;
    while (LANGUAGE[p]) {
      if (LANGUAGE[p]==':') { p++; continue; }
      const char *token=LANGUAGE+p;
      int tokenc=0;
      while (LANGUAGE[p]&&(LANGUAGE[p++]!=':')) tokenc++;
      if (tokenc>=2) dstc=eggrt_lang_append(dst,dsta,dstc,token);
    }
  }
  
  // TODO Alternate API for MacOS?
  // TODO Alternate API for Windows?
  
  return dstc;
}

/* Decode the metadata "lang" field.
 * Never returns more than (dsta).
 */
 
static int eggrt_eval_metadata_lang(int *dst,int dsta,const char *src,int srcc) {
  if (dsta<1) return 0;
  int srcp=0,dstc=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]==',') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp]!=',')&&((unsigned char)src[srcp]>0x20)) { srcp++; tokenc++; }
    if ((tokenc==2)&&(token[0]>='a')&&(token[0]<='z')&&(token[1]>='a')&&(token[1]<='z')) {
      dst[dstc++]=((token[0]-'a'+1)<<5)|(token[1]-'a'+1);
      if (dstc>=dsta) return dstc;
    }
  }
  return dstc;
}

/* List the ROM's languages in preference order.
 * This is the content of metadata:1 "lang".
 * Never returns more than (dsta).
 */
 
static int eggrt_get_rom_langs(int *dst,int dsta) {
  if (!dst||(dsta<1)) return 0;
  int dstc=0;
  
  // If "lang" exists in metadata:1, that's the whole answer.
  int resp=eggrt_rom_search(EGG_TID_metadata,1);
  if (resp>=0) {
    struct metadata_reader reader;
    if (metadata_reader_init(&reader,eggrt.resv[resp].v,eggrt.resv[resp].c)>=0) {
      struct metadata_entry entry;
      while (metadata_reader_next(&entry,&reader)>0) {
        if (entry.kc!=4) continue;
        if (memcmp(entry.k,"lang",4)) continue;
        if ((dstc=eggrt_eval_metadata_lang(dst,dsta,entry.v,entry.vc))>0) return dstc;
        dstc=0;
        break;
      }
    }
  }
  
  // ROM didn't declare any languages. Infer the list from its strings resources.
  // Note that these will be in alphabetical "preference" order, that's the best we can do.
  resp=eggrt_rom_search(EGG_TID_strings,1);
  if (resp<0) resp=0;
  int pvlang=0;
  while ((resp<eggrt.resc)&&(eggrt.resv[resp].tid==EGG_TID_strings)) {
    int lang=eggrt.resv[resp].rid>>6;
    if (lang==pvlang) continue;
    pvlang=lang;
    char s[2]={0};
    EGG_STRING_FROM_LANG(s,lang);
    if ((s[0]<'a')||(s[0]>'z')||(s[1]<'a')||(s[1]>'z')) continue;
    if (dstc>=dsta) break;
    dst[dstc++]=lang;
  }
  return dstc;
}

/* Choose initial language.
 */
 
static int eggrt_prefs_choose_language() {

  // Get prioritized lists for the system (ie user) and the rom.
  int sysv[32],romv[32];
  int sysc=eggrt_get_sys_langs(sysv,sizeof(sysv)/sizeof(sysv[0]));
  int romc=eggrt_get_rom_langs(romv,sizeof(romv)/sizeof(romv[0]));
  
  /* Check the user's preferences in order.
   * First one supported at all by the rom wins.
   */
  int i=0;
  for (;i<sysc;i++) {
    int ri=romc,match=0;
    while (ri-->0) if (romv[ri]==sysv[i]) {
      match=1;
      break;
    }
    if (match) return sysv[i];
  }
  
  /* I'd like to fuzz it out from here.
   * eg if rom=[en,es] and sys=[pt], we should start in Spanish rather than English, since Portugese wasn't an option.
   * But I'm not competent to classify languages in that kind of detail.
   * Is there some standard way to go about it?
   */
   
  /* Since we can't give the user a language she speaks, use the rom's most preferred language.
   */
  if (romc>0) return romv[0];
  
  /* Rom didn't declare any languages.
   * It probably doesn't matter what we choose now.
   * So take the user's first preference, or if that's unset, English.
   */
  if (sysc>0) return sysv[0];
  return EGG_LANG_FROM_STRING("en");
}

/* Initialize audio preferences.
 * Disabled if we can detect dummy output, otherwise enabled.
 * Could persist these somewhere if we want to.
 */
 
static void eggrt_prefs_init_audio() {
  if (!eggrt.hostio->audio||(eggrt.hostio->audio->type==&hostio_audio_type_dummy)) {
    fprintf(stderr,"%s: Starting sound preferences disabled due to dummy driver.\n",eggrt.exename);
    eggrt.music_enable=0;
    eggrt.sound_enable=0;
  } else {
    eggrt.music_enable=1;
    eggrt.sound_enable=1;
  }
}

/* Initialize preferences.
 */

int eggrt_prefs_init() {
  eggrt.lang=eggrt_prefs_choose_language();
  eggrt_prefs_init_audio();
  eggrt_language_changed();
  return 0;
}
