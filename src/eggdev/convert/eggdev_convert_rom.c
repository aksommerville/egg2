/* eggdev_convert_rom.c
 * Converters that reformat a ROM or executable, or something in that neighborhood.
 */

#include "eggdev/eggdev_internal.h"
#include "eggdev/convert/eggdev_rom.h"
#include "opt/zip/zip.h"

/* Validate and measure ROM.
 * We don't validate that tid and rid stay in range, only what's necessary to measure it.
 */
 
static int eggdev_measure_rom(const uint8_t *src,int srcc) {
  if (!src||(srcc<4)||memcmp(src,"\0ERM",4)) return -1;
  int srcp=4;
  for (;;) {
    if (srcp>=srcc) return -1; // Terminator required.
    uint8_t lead=src[srcp++];
    if (!lead) return srcp; // Terminator.
    switch (lead&0xc0) {
      case 0x00: break; // TID, single byte.
      case 0x40: srcp++; break; // RID
      case 0x80: { // RES
          if (srcp>srcc-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len++;
          if (srcp>srcc-len) return -1;
          srcp+=len;
        } break;
      default: return -1; // RESERVED
    }
  }
}

/* ROM from executable, ie search and extract.
 */
 
int eggdev_egg_from_exe(struct eggdev_convert_context *ctx) {
  const uint8_t *src=ctx->src;
  int srcc=ctx->srcc;
  int srcp=0,stopp=srcc-4,gotempty=0;
  while (srcp<=stopp) {
    if (memcmp(src+srcp,"\0ERM",4)) { srcp++; continue; }
    int c=eggdev_measure_rom(src+srcp,srcc-srcp);
    if (c>5) {
      return sr_encode_raw(ctx->dst,src+srcp,c);
    } else if (c==5) {
      // A 5-byte ROM, just the signature and terminator, is technically legal, but also likely to happen by accident.
      // Keep looking.
      gotempty=1;
      srcp++;
    } else {
      // If measurement failed, skip it and keep looking.
      // It's not suprising for the string "\0ERM" to appear in an executable in some context other than the embedded ROM.
      srcp++;
    }
  }
  if (gotempty) return sr_encode_raw(ctx->dst,"\0ERM\0",5);
  return eggdev_convert_error(ctx,"Egg ROM not found.");
}

/* ROM from Zip. Must be "/game.bin".
 */

int eggdev_egg_from_zip(struct eggdev_convert_context *ctx) {
  struct zip_reader reader={0};
  if (zip_reader_init(&reader,ctx->src,ctx->srcc)<0) {
    zip_reader_cleanup(&reader);
    return eggdev_convert_error(ctx,"Failed to initialize ZIP reader.");
  }
  for (;;) {
    struct zip_file file={0};
    int err=zip_reader_next(&file,&reader);
    if (err<0) {
      zip_reader_cleanup(&reader);
      return eggdev_convert_error(ctx,"Error reading ZIP file.");
    }
    if (!err) {
      zip_reader_cleanup(&reader);
      return eggdev_convert_error(ctx,"'game.bin' not found in ZIP file.");
    }
    if ((file.namec==8)&&!memcmp(file.name,"game.bin",8)) {
      err=sr_encode_raw(ctx->dst,file.udata,file.usize);
      zip_reader_cleanup(&reader);
      return err;
    }
  }
}

/* Gather the things we need from a ROM file, for generating either of the HTMLs.
 */
 
struct eggdev_html_rom_bits {
  const char *title;
  int titlec;
  const void *icon;
  int iconc;
  int iconid;
};

static void eggdev_html_rom_bits_from_metadata(struct eggdev_html_rom_bits *bits,const uint8_t *src,int srcc) {
  if ((srcc<4)||memcmp(src,"\0EMD",4)) return;
  int srcp=4;
  while (srcp<srcc) {
    uint8_t kc=src[srcp++];
    if (!kc||(srcp>=srcc)) return;
    uint8_t vc=src[srcp++];
    if (srcp>srcc-vc-kc) return;
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    
    if ((kc==5)&&!memcmp(k,"title",5)) {
      bits->title=v;
      bits->titlec=vc;
      
    } else if ((kc==9)&&!memcmp(k,"iconImage",9)) {
      bits->iconid=0;
      int i=0; for (;i<vc;i++) {
        int digit=v[i]-'0';
        if ((digit<0)||(digit>9)) {
          bits->iconid=0;
          break;
        }
        bits->iconid*=10;
        bits->iconid+=digit;
      }
    }
  }
}

static void eggdev_html_rom_bits_extract(struct eggdev_html_rom_bits *bits,const void *rom,int romc) {
  struct eggdev_rom_reader reader;
  if (eggdev_rom_reader_init(&reader,rom,romc)<0) return;
  struct eggdev_res res;
  while (eggdev_rom_reader_next(&res,&reader)>0) {
    
    if ((res.tid==EGG_TID_metadata)&&(res.rid==1)) {
      eggdev_html_rom_bits_from_metadata(bits,res.v,res.c);
      
    } else if ((res.tid==EGG_TID_image)&&(res.rid==bits->iconid)) {
      // Don't worry; (iconid) will always be populated before we see any images (or not at all).
      bits->icon=res.v;
      bits->iconc=res.c;
      return; // No further resources needed after the icon.
    }
  }
}

/* Populate Separate HTML template.
 * We do not embed (rom); we just use it for metadata extraction.
 */
 
static int eggdev_populate_separate_html(struct sr_encoder *dst,const char *src,int srcc,const void *rom,int romc) {
  struct eggdev_html_rom_bits bits={0};
  eggdev_html_rom_bits_extract(&bits,rom,romc);
  
  /* Locate "<title>...</title>" and replace with both icon and title, whatever we have in (bits).
   * If (bits.title) is empty, remove the <title> tag altogether.
   */
  int srcp=0;
  for (;;) {
    int cpc=0;
    while (srcp+cpc<srcc) {
      if ((srcp+cpc<=srcc-7)&&!memcmp(src+srcp+cpc,"<title>",7)) break;
      cpc++;
    }
    if (sr_encode_raw(dst,src+srcp,cpc)<0) return -1;
    if ((srcp+=cpc)>=srcc) break;
    srcp+=7;
    while ((srcp<=srcc-8)&&memcmp(src+srcp,"</title>",8)) srcp++;
    if (srcp<srcc) srcp+=8;
    
    if (bits.iconc) {
      if (sr_encode_raw(dst,"<link rel=\"icon\" type=\"image/png\" href=\"data:;base64,",-1)<0) return -1;
      if (sr_encode_base64(dst,bits.icon,bits.iconc)<0) return -1;
      if (sr_encode_raw(dst,"\" />\n",-1)<0) return -1;
    }
    if (bits.titlec) {
      if (sr_encode_fmt(dst,"<title>%.*s</title>\n",bits.titlec,bits.title)<0) return -1;
    }
  }
  return 0;
}

/* Zip from ROM.
 */
 
int eggdev_zip_from_egg(struct eggdev_convert_context *ctx) {
  struct zip_writer writer={0};
  struct zip_file file={
    .zip_version=20|(3<<8),
    .flags=0,
    .udata=ctx->src,
    .usize=ctx->srcc,
    .name="game.bin",
    .namec=8,
    .extra="\x75\x78\x0b\x00\x01\x04\xe8\x03\x00\x00\x04\xe8\x03\x00\x00",
    .extrac=15,
  };
  if (zip_writer_add(&writer,&file)<0) {
    zip_writer_cleanup(&writer);
    return -1;
  }
  const char *tm=0;
  int tmc=eggdev_get_separate_html_template(&tm);
  if (tmc<0) {
    zip_writer_cleanup(&writer);
    return eggdev_convert_error(ctx,"Failed to acquire Separate HTML template.");
  }
  struct sr_encoder html={0};
  if (eggdev_populate_separate_html(&html,tm,tmc,ctx->src,ctx->srcc)<0) {
    sr_encoder_cleanup(&html);
    zip_writer_cleanup(&writer);
    return eggdev_convert_error(ctx,"Failed to generate HTML bootstrap.");
  }
  file.udata=html.v;
  file.usize=html.c;
  file.name="index.html";
  file.namec=10;
  if (zip_writer_add(&writer,&file)<0) {
    sr_encoder_cleanup(&html);
    zip_writer_cleanup(&writer);
    return -1;
  }
  sr_encoder_cleanup(&html);
  int err=zip_writer_finish(ctx->dst,&writer);
  zip_writer_cleanup(&writer);
  return err;
}

/* Fetch the HTML templates.
 */
 
int eggdev_get_separate_html_template(void *dstpp) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/out/separate.html",g.sdkpath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  return file_read(dstpp,path);
}
