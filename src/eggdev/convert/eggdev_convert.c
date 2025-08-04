#include "eggdev/eggdev_internal.h"
#include "eggdev_convert.h"
#include <stdarg.h>

/* Represent format name.
 */
 
int eggdev_fmt_repr(char *dst,int dsta,int fmt) {
  if (!dst||(dsta<0)) dsta=0;
  const char *src;
  int srcc=0;
  switch (fmt) {
    #define _(tag) case EGGDEV_FMT_##tag: src=#tag; srcc=sizeof(#tag)-1; break;
    EGGDEV_FMT_FOR_EACH
    #undef _
  }
  if (srcc>0) {
    if (srcc<=dsta) {
      memcpy(dst,src,srcc);
      if (srcc<dsta) dst[srcc]=0;
    }
    return srcc;
  }
  return sr_decsint_repr(dst,dsta,fmt);
}

/* Evaluate format name.
 */
 
int eggdev_fmt_eval(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  // Force to lowercase and check symbols.
  char norm[16];
  if (srcc<=sizeof(norm)) {
    int i=srcc; while (i-->0) {
      if ((src[i]>='A')&&(src[i]<='Z')) norm[i]=src[i]+0x20;
      else norm[i]=src[i];
    }
    #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(norm,#tag,srcc)) return EGGDEV_FMT_##tag;
    EGGDEV_FMT_FOR_EACH
    #undef _
    // Check a few aliases.
    switch (srcc) {
      case 3: {
          if (!memcmp(norm,"jar",3)) return EGGDEV_FMT_zip;
          if (!memcmp(norm,"htm",3)) return EGGDEV_FMT_html;
          if (!memcmp(norm,"jpg",3)) return EGGDEV_FMT_jpeg;
        } break;
      case 4: {
          if (!memcmp(norm,"midi",4)) return EGGDEV_FMT_mid;
        } break;
    }
  }
  int v;
  if ((sr_int_eval(&v,src,srcc)>=2)&&(v>0)) return v;
  return 0;
}

/* Format from path.
 */
 
int eggdev_fmt_by_path(const char *path,int pathc) {
  if (!path) return 0;
  if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
  // Usually we want to determine from the suffix.
  const char *sfx=path+pathc;
  int sfxc=0;
  while (sfxc<pathc) {
    if (sfx[-1]=='.') {
      int fmt=eggdev_fmt_eval(sfx,sfxc);
      if (fmt>0) return fmt;
      break;
    }
    if (sfx[-1]=='/') {
      // No suffix. We do recognize a special file "metadata".
      if ((sfxc==8)&&!memcmp(sfx,"metadata",8)) return EGGDEV_FMT_metatxt;
      break;
    }
    sfx--;
    sfxc++;
  }
  // If there's a directory called "data", the directory below it is the resource type.
  int pathp=0,underdata=0;
  while (pathp<pathc) {
    if (path[pathp]=='/') { pathp++; continue; }
    const char *dir=path+pathp;
    int dirc=0;
    while ((pathp<pathc)&&(path[pathp++]!='/')) dirc++;
    if (underdata) {
      int tid=eggdev_tid_eval(dir,dirc);
      if (tid==EGG_TID_sound) break; // Sounds are often empty, and that's perfectly legal.
      if (tid>0) {
        int fmt=eggdev_fmt_by_tid(tid);
        if (fmt>0) return eggdev_fmt_portable(fmt);
      }
      break;
    } else if ((dirc==4)&&!memcmp(dir,"data",4)) {
      underdata=1;
    }
  }
  // Welp I got nothing.
  return 0;
}

/* Preferred format in ROM.
 */
 
int eggdev_fmt_by_tid(int tid) {
  switch (tid) {
    case EGG_TID_metadata: return EGGDEV_FMT_metadata;
    case EGG_TID_code: return EGGDEV_FMT_wasm;
    case EGG_TID_strings: return EGGDEV_FMT_strings;
    case EGG_TID_image: return EGGDEV_FMT_png;
    case EGG_TID_song: return EGGDEV_FMT_eau;
    case EGG_TID_sound: return EGGDEV_FMT_eau;
    case EGG_TID_tilesheet: return EGGDEV_FMT_tilesheet;
    case EGG_TID_decalsheet: return EGGDEV_FMT_decalsheet;
    case EGG_TID_map: return EGGDEV_FMT_map;
    case EGG_TID_sprite: return EGGDEV_FMT_sprite;
  }
  return 0;
}

/* Preferred format for extraction.
 */
 
int eggdev_fmt_portable(int fmt) {
  switch (fmt) {
    case EGGDEV_FMT_eau: return EGGDEV_FMT_mid;
    case EGGDEV_FMT_metadata: return EGGDEV_FMT_metatxt;
    case EGGDEV_FMT_strings: return EGGDEV_FMT_strtxt;
    case EGGDEV_FMT_tilesheet: return EGGDEV_FMT_tstxt;
    case EGGDEV_FMT_decalsheet: return EGGDEV_FMT_dstxt;
    case EGGDEV_FMT_map: return EGGDEV_FMT_maptxt;
    case EGGDEV_FMT_sprite: return EGGDEV_FMT_sprtxt;
    case EGGDEV_FMT_cmdlist: return EGGDEV_FMT_cmdltxt;
  }
  return fmt; // Preserving the original format is always a sensible option.
}

/* Resource type by path if concrete, or data format if necessary.
 */
 
int eggdev_tid_by_path_or_fmt(const char *path,int pathc,int fmt) {
  if (path) {
    if (pathc<0) { pathc=0; while (path[pathc]) pathc++; }
    if ((pathc>=8)&&!memcmp(path+pathc-8,"metadata",8)) return EGG_TID_metadata;
    if ((pathc>=9)&&!memcmp(path+pathc-9,"code.wasm",9)) return EGG_TID_code;
    int pathp=0,underdata=0;
    while (pathp<pathc) {
      if (path[pathp]=='/') { pathp++; continue; }
      const char *dir=path+pathp;
      int dirc=0;
      while ((pathp<pathc)&&(path[pathp++]!='/')) dirc++;
      if (underdata) {
        int tid=eggdev_tid_eval(dir,dirc);
        if (tid>0) return tid;
        break;
      } else if ((dirc==4)&&!memcmp(dir,"data",4)) {
        underdata=1;
      }
    }
  }
  switch (fmt) {
    case EGGDEV_FMT_png:
    case EGGDEV_FMT_gif:
    case EGGDEV_FMT_jpeg:
      return EGG_TID_image;
    case EGGDEV_FMT_wav:
    case EGGDEV_FMT_mid:
    case EGGDEV_FMT_eau:
    case EGGDEV_FMT_eaut:
      return EGG_TID_sound;
    case EGGDEV_FMT_wasm:
      return EGG_TID_code;
    case EGGDEV_FMT_metadata:
    case EGGDEV_FMT_metatxt:
      return EGG_TID_metadata;
    case EGGDEV_FMT_strings:
    case EGGDEV_FMT_strtxt:
      return EGG_TID_strings;
    case EGGDEV_FMT_tilesheet:
    case EGGDEV_FMT_tstxt:
      return EGG_TID_tilesheet;
    case EGGDEV_FMT_decalsheet:
    case EGGDEV_FMT_dstxt:
      return EGG_TID_decalsheet;
    case EGGDEV_FMT_map:
    case EGGDEV_FMT_maptxt:
      return EGG_TID_map;
    case EGGDEV_FMT_sprite:
    case EGGDEV_FMT_sprtxt:
      return EGG_TID_sprite;
  }
  return 0;
}

/* Format by signature.
 */
 
int eggdev_fmt_by_signature(const void *src,int srcc) {
  if (!src) return 0;
  if (srcc<1) return 0;
  
  /* Start with unambiguous binary signatures.
   * Egg's own formats always have these, and well-behaved portable formats too.
   */
  if (srcc>=4) {
    if (!memcmp(src,"\0ERM",4)) return EGGDEV_FMT_egg;
    if (!memcmp(src,"\0EAU",4)) return EGGDEV_FMT_eau;
    if (!memcmp(src,"\0asm",4)) return EGGDEV_FMT_wasm;
    if (!memcmp(src,"\0EMD",4)) return EGGDEV_FMT_metadata;
    if (!memcmp(src,"\0EST",4)) return EGGDEV_FMT_strings;
    if (!memcmp(src,"\0ETS",4)) return EGGDEV_FMT_tilesheet;
    if (!memcmp(src,"\0EDS",4)) return EGGDEV_FMT_decalsheet;
    if (!memcmp(src,"\0EMP",4)) return EGGDEV_FMT_map;
    if (!memcmp(src,"\0ESP",4)) return EGGDEV_FMT_sprite;
    if (!memcmp(src,"\x7f""ELF",4)) return EGGDEV_FMT_exe;
    // Do other executable formats have unambiguous signatures? I'd expect so. Find them. They can all be called "exe".
  }
  if (srcc>=8) {
    if (!memcmp(src,"\x89PNG\r\n\x1a\n",8)) return EGGDEV_FMT_png;
    if (!memcmp(src,"MThd\0\0\0\6",8)) return EGGDEV_FMT_mid;
  }
  
  /* WAV, GIF, and ZIP are binary formats with text signatures.
   * What a stupid thing to do.
   * Nevertheless, we'll trust the signature. And if you have a text file that begins "PK", well, bad luck.
   */
  if ((srcc>=12)&&!memcmp(src,"RIFF",4)&&!memcmp((char*)src+8,"WAVE",4)) return EGGDEV_FMT_wav;
  if ((srcc>=6)&&(!memcmp(src,"GIF87a",6)||!memcmp(src,"GIF89a",6))) return EGGDEV_FMT_gif;
  if ((srcc>=2)&&!memcmp(src,"PK",2)) return EGGDEV_FMT_zip;
  
  /* HTML files, ones I write at least, will always begin with the HTML 5 DOCTYPE.
   */
  if ((srcc>=14)&&!memcmp(src,"<!DOCTYPE html",14)) return EGGDEV_FMT_html;
  
  /* And anything else is ambiguous.
   */
  return 0;
}

/* MIME type from egg format.
 * Egg-specific formats are "text/x-egg-*" for text or "application/x-egg-*" for binary.
 */

const char *eggdev_mime_type_by_fmt(int fmt) {
  switch (fmt) {
    case EGGDEV_FMT_egg: return "application/x-egg-rom";
    case EGGDEV_FMT_exe: return "application/x-executable";
    case EGGDEV_FMT_zip: return "application/zip";
    case EGGDEV_FMT_html: return "text/html";
    case EGGDEV_FMT_css: return "text/css";
    case EGGDEV_FMT_js: return "application/javascript";
    case EGGDEV_FMT_png: return "image/png";
    case EGGDEV_FMT_gif: return "image/gif";
    case EGGDEV_FMT_jpeg: return "image/jpeg";
    case EGGDEV_FMT_wav: return "audio/wav"; // not standard
    case EGGDEV_FMT_mid: return "audio/midi";
    case EGGDEV_FMT_eau: return "application/x-egg-eau";
    case EGGDEV_FMT_eaut: return "text/x-egg-eau";
    case EGGDEV_FMT_wasm: return "application/wasm";
    case EGGDEV_FMT_metadata: return "application/x-egg-metadata";
    case EGGDEV_FMT_metatxt: return "text/x-egg-metadata";
    case EGGDEV_FMT_strings: return "application/x-egg-strings";
    case EGGDEV_FMT_strtxt: return "text/x-egg-strings";
    case EGGDEV_FMT_tilesheet: return "application/x-egg-tilesheet";
    case EGGDEV_FMT_tstxt: return "text/x-egg-tilesheet";
    case EGGDEV_FMT_decalsheet: return "application/x-egg-decalsheet";
    case EGGDEV_FMT_dstxt: return "text/x-egg-decalsheet";
    case EGGDEV_FMT_map: return "application/x-egg-map";
    case EGGDEV_FMT_maptxt: return "text/x-egg-map";
    case EGGDEV_FMT_sprite: return "application/x-egg-sprite";
    case EGGDEV_FMT_sprtxt: return "text/x-egg-sprite";
    case EGGDEV_FMT_cmdlist: return "application/x-egg-cmdlist";
    case EGGDEV_FMT_cmdltxt: return "text/x-egg-cmdlist";
    case EGGDEV_FMT_ico: return "image/vnd.microsoft.icon";
  }
  return "application/octet-stream";
}

/* MIME type from content, path, or egg format.
 */
 
const char *eggdev_guess_mime_type(const void *src,int srcc,const char *path,int fmt) {
  if (fmt) return eggdev_mime_type_by_fmt(fmt);
  if (fmt=eggdev_fmt_by_path(path,-1)) return eggdev_mime_type_by_fmt(fmt);
  if (fmt=eggdev_fmt_by_signature(src,srcc)) return eggdev_mime_type_by_fmt(fmt);
  // Empty (or not provided), call it "application/octet-stream" because that's the most generic.
  if (srcc<1) return "application/octet-stream";
  // Finally, it's "text/plain" if UTF-8 and 'application/octet-stream" otherwise.
  const uint8_t *SRC=src;
  int srcp=0,codepoint,seqlen;
  while (srcp<srcc) {
    if (!SRC[srcp]) return "application/octet-stream"; // NUL is technically valid UTF-8 but very unlikely.
    if (!(SRC[srcp]&0x80)) { srcp++; continue; }
    if ((seqlen=sr_utf8_decode(&codepoint,SRC+srcp,srcc-srcp))<1) return "application/octet-stream";
    srcp+=seqlen;
    if (srcp>=1024) break; // Stop reading after a kilobyte (but do it here, not above, to ensure we don't break a UTF-8 sequence).
  }
  return "text/plain";
}

/* Get converter.
 */
 
eggdev_convert_fn eggdev_get_converter(int dstfmt,int srcfmt) {

  /* Either format unspecified, or same to same, use the "noop" converter.
   * We fail only if two concrete formats are provided, and we can't do it.
   */
  if (!dstfmt||!srcfmt||(dstfmt==srcfmt)) return eggdev_convert_noop;
  
  switch (dstfmt) {
    case EGGDEV_FMT_egg: switch (srcfmt) {
        case EGGDEV_FMT_exe: return eggdev_egg_from_exe;
        case EGGDEV_FMT_zip: return eggdev_egg_from_zip;
        case EGGDEV_FMT_html: return eggdev_egg_from_html;
      } break;
    case EGGDEV_FMT_zip: switch (srcfmt) {
        case EGGDEV_FMT_egg: return eggdev_zip_from_egg;
        case EGGDEV_FMT_html: return eggdev_zip_from_html;
        case EGGDEV_FMT_exe: return eggdev_zip_from_exe;
      } break;
    case EGGDEV_FMT_html: switch (srcfmt) {
        case EGGDEV_FMT_egg: return eggdev_html_from_egg;
        case EGGDEV_FMT_zip: return eggdev_html_from_zip;
        case EGGDEV_FMT_exe: return eggdev_html_from_exe;
      } break;
    case EGGDEV_FMT_wav: switch (srcfmt) {
        case EGGDEV_FMT_eau: return eggdev_wav_from_eau;
        case EGGDEV_FMT_eaut: return eggdev_wav_from_eaut;
        case EGGDEV_FMT_mid: return eggdev_wav_from_mid;
      } break;
    case EGGDEV_FMT_mid: switch (srcfmt) {
        case EGGDEV_FMT_eau: return eggdev_mid_from_eau;
        case EGGDEV_FMT_eaut: return eggdev_mid_from_eaut;
      } break;
    case EGGDEV_FMT_eau: switch (srcfmt) {
        case EGGDEV_FMT_eaut: return eggdev_eau_from_eaut;
        case EGGDEV_FMT_mid: return eggdev_eau_from_mid;
      } break;
    case EGGDEV_FMT_eaut: switch (srcfmt) {
        case EGGDEV_FMT_eau: return eggdev_eaut_from_eau;
        case EGGDEV_FMT_mid: return eggdev_eaut_from_mid;
      } break;
    case EGGDEV_FMT_metadata: switch (srcfmt) {
        case EGGDEV_FMT_metatxt: return eggdev_metadata_from_metatxt;
      } break;
    case EGGDEV_FMT_metatxt: switch (srcfmt) {
        case EGGDEV_FMT_metadata: return eggdev_metatxt_from_metadata;
      } break;
    case EGGDEV_FMT_strings: switch (srcfmt) {
        case EGGDEV_FMT_strtxt: return eggdev_strings_from_strtxt;
      } break;
    case EGGDEV_FMT_strtxt: switch (srcfmt) {
        case EGGDEV_FMT_strings: return eggdev_strtxt_from_strings;
      } break;
    case EGGDEV_FMT_tilesheet: switch (srcfmt) {
        case EGGDEV_FMT_tstxt: return eggdev_tilesheet_from_tstxt;
      } break;
    case EGGDEV_FMT_tstxt: switch (srcfmt) {
        case EGGDEV_FMT_tilesheet: return eggdev_tstxt_from_tilesheet;
      } break;
    case EGGDEV_FMT_decalsheet: switch (srcfmt) {
        case EGGDEV_FMT_dstxt: return eggdev_decalsheet_from_dstxt;
      } break;
    case EGGDEV_FMT_dstxt: switch (srcfmt) {
        case EGGDEV_FMT_decalsheet: return eggdev_dstxt_from_decalsheet;
      } break;
    case EGGDEV_FMT_map: switch (srcfmt) {
        case EGGDEV_FMT_maptxt: return eggdev_map_from_maptxt;
      } break;
    case EGGDEV_FMT_maptxt: switch (srcfmt) {
        case EGGDEV_FMT_map: return eggdev_maptxt_from_map;
      } break;
    case EGGDEV_FMT_sprite: switch (srcfmt) {
        case EGGDEV_FMT_sprtxt: return eggdev_sprite_from_sprtxt;
      } break;
    case EGGDEV_FMT_sprtxt: switch (srcfmt) {
        case EGGDEV_FMT_sprite: return eggdev_sprtxt_from_sprite;
      } break;
    case EGGDEV_FMT_cmdlist: switch (srcfmt) {
        case EGGDEV_FMT_cmdltxt: return eggdev_cmdlist_from_cmdltxt;
      } break;
    case EGGDEV_FMT_cmdltxt: switch (srcfmt) {
        case EGGDEV_FMT_cmdlist: return eggdev_cmdltxt_from_cmdlist;
      } break;
  }
  return 0;
}

/* One-shot conversion for compiling ROM.
 */
 
int eggdev_convert_for_rom(struct sr_encoder *dst,const void *src,int srcc,int srcfmt,const char *path,struct sr_encoder *errmsg) {
  if (srcfmt<1) {
    srcfmt=eggdev_fmt_by_path(path,-1);
    if (srcfmt<1) {
      srcfmt=eggdev_fmt_by_signature(src,srcc);
    }
  }
  int tid=eggdev_tid_by_path_or_fmt(path,-1,srcfmt);
  int dstfmt=eggdev_fmt_by_tid(tid);
  eggdev_convert_fn cvt=eggdev_get_converter(dstfmt,srcfmt);
  if (!cvt) {
    if (!path) return -1;
    fprintf(stderr,"%s: Failed to select data conversion.\n",path);
    return -2;
  }
  char tname[32];
  int tnamec=eggdev_tid_repr(tname,sizeof(tname),tid);
  if ((tnamec<0)||(tnamec>sizeof(tname))) tnamec=0;
  struct eggdev_convert_context ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .ns=tname,
    .nsc=tnamec,
    .refname=path,
    .lineno0=0,
    .errmsg=errmsg,
  };
  return cvt(&ctx);
}

/* One-shot conversion for extracting from ROM.
 */
 
int eggdev_convert_for_extraction(struct sr_encoder *dst,const void *src,int srcc,int srcfmt,int tid,struct sr_encoder *errmsg) {
  if (srcfmt<1) {
    srcfmt=eggdev_fmt_by_signature(src,srcc);
    if (srcfmt<1) {
      srcfmt=eggdev_fmt_by_tid(tid);
    }
  }
  int dstfmt=eggdev_fmt_portable(srcfmt);
  eggdev_convert_fn cvt=eggdev_get_converter(dstfmt,srcfmt);
  if (!cvt) return -1;
  char tname[32];
  int tnamec=eggdev_tid_repr(tname,sizeof(tname),tid);
  if ((tnamec<0)||(tnamec>sizeof(tname))) tnamec=0;
  struct eggdev_convert_context ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .ns=tname,
    .nsc=tnamec,
    .refname=0,
    .lineno0=0,
    .errmsg=errmsg,
  };
  return cvt(&ctx);
}

/* One-shot anything-to-anything conversion.
 */
 
int eggdev_convert_auto(
  struct sr_encoder *dst, // Required.
  const void *src,int srcc, // Required.
  int dstfmt,int srcfmt, // Provide as much as you know...
  const char *dstpath,
  const char *srcpath,
  int tid
) {
  if (srcfmt<1) {
    srcfmt=eggdev_fmt_by_path(srcpath,-1);
    if (srcfmt<1) {
      srcfmt=eggdev_fmt_by_signature(src,srcc);
    }
  }
  if (dstfmt<1) {
    dstfmt=eggdev_fmt_by_path(dstpath,-1);
  }
  eggdev_convert_fn cvt=eggdev_get_converter(dstfmt,srcfmt);
  if (!cvt) {
    if (!srcpath) return -1;
    fprintf(stderr,"%s: Failed to determine data conversion.\n",srcpath);
    return -2;
  }
  if (tid<1) {
    tid=eggdev_tid_by_path_or_fmt(srcpath,-1,srcfmt);
    if (tid<1) {
      tid=eggdev_tid_by_path_or_fmt(dstpath,-1,dstfmt);
    }
  }
  char tname[32];
  int tnamec=eggdev_tid_repr(tname,sizeof(tname),tid);
  if ((tnamec<0)||(tnamec>sizeof(tname))) tnamec=0;
  struct eggdev_convert_context ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .ns=tname,
    .nsc=tnamec,
    .refname=srcpath,
    .lineno0=0,
  };
  return cvt(&ctx);
}

/* Log error in context.
 */
 
static int eggdev_convert_error_inner(struct eggdev_convert_context *ctx,int lineno,const char *fmt,va_list vargs) {
  if (!fmt) fmt="";
  char msg[256];
  int msgc=vsnprintf(msg,sizeof(msg),fmt,vargs);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  while (msgc&&((unsigned char)msg[msgc-1]<=0x20)) msgc--;
  const char *refname=ctx->refname;
  if (!refname) refname="<input>";
  else if ((refname[0]=='-')&&!refname[1]) refname="<stdin>";
  if (ctx->errmsg) {
    if (lineno) sr_encode_fmt(ctx->errmsg,"%s:%d: %.*s\n",refname,lineno,msgc,msg);
    else sr_encode_fmt(ctx->errmsg,"%s: %.*s\n",refname,msgc,msg);
  } else {
    if (lineno) fprintf(stderr,"%s:%d: %.*s\n",refname,lineno,msgc,msg);
    else fprintf(stderr,"%s: %.*s\n",refname,msgc,msg);
  }
  return -2;
}
 
int eggdev_convert_error(struct eggdev_convert_context *ctx,const char *fmt,...) {
  if (!ctx||(!ctx->errmsg&&!ctx->refname)) return -1;
  va_list vargs;
  va_start(vargs,fmt);
  return eggdev_convert_error_inner(ctx,0,fmt,vargs);
}

int eggdev_convert_error_at(struct eggdev_convert_context *ctx,int lineno,const char *fmt,...) {
  if (!ctx||(!ctx->errmsg&&!ctx->refname)) return -1;
  lineno+=ctx->lineno0;
  va_list vargs;
  va_start(vargs,fmt);
  return eggdev_convert_error_inner(ctx,lineno,fmt,vargs);
}
