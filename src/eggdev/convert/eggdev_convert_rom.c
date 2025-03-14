/* eggdev_convert_rom.c
 * Converters that reformat a ROM or executable, or something in that neighborhood.
 */

#include "eggdev/eggdev_internal.h"
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

/* ROM from Standalone HTML.
 */
 
int eggdev_egg_from_html(struct eggdev_convert_context *ctx) {
  /* No need to overengineer this with an HTML parser or anything.
   * Just find <egg-rom> and </egg-rom>, and the text between them is the base64-encoded ROM.
   */
  const char *src=ctx->src;
  int srcc=ctx->srcc;
  int stopp=srcc-10;
  int beginp=-1,endp=-1;
  int srcp=0; for (;srcp<=stopp;srcp++) {
    if (beginp<0) {
      if (!memcmp(src+srcp,"<egg-rom>",9)) beginp=srcp+9;
    } else {
      if (!memcmp(src+srcp,"</egg-rom>",10)) endp=srcp;
    }
  }
  if (beginp<0) return eggdev_convert_error(ctx,"<egg-rom> tag not found.");
  if (endp<beginp) return eggdev_convert_error(ctx,"Unclosed <egg-rom> tag.");
  // Our base64 decoder does tolerate whitespace anywhere, which will probably happen here.
  for (;;) {
    int err=sr_base64_decode((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,src+beginp,endp-beginp);
    if (err<0) return eggdev_convert_error(ctx,"Malformed base64 in <egg-rom> tag.");
    if (ctx->dst->c<=ctx->dst->a-err) {
      ctx->dst->c+=err;
      break;
    }
    if (sr_encoder_require(ctx->dst,err)<0) return -1;
  }
  return 0;
}

/* Populate Separate HTML template.
 */
 
static int eggdev_populate_separate_html(struct sr_encoder *dst,const char *src,int srcc,const void *rom,int romc) {
  return sr_encode_raw(dst,src,srcc);//TODO title, canvas size, icon, etc...
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

/* Standalone HTML from ROM.
 */
 
int eggdev_html_from_egg(struct eggdev_convert_context *ctx) {
  //TODO Other insertions like title, canvas size...
  const char *tm=0;
  int tmc=eggdev_get_standalone_html_template(&tm);
  if (tmc<0) return eggdev_convert_error(ctx,"Failed to acquire Standalone HTML template.");
  const char *marker="<egg-rom></egg-rom>";
  const int markerc=19;
  int stopp=tmc-markerc;
  int tmp=0; for (;tmp<=stopp;tmp++) {
    if (!memcmp(tm+tmp,marker,markerc)) {
      if (sr_encode_raw(ctx->dst,tm,tmp)<0) return -1;
      if (sr_encode_raw(ctx->dst,"<egg-rom>\n",-1)<0) return -1;
      int rowlen=90; // Binary bytes. 90 => 120 bytes text.
      const uint8_t *src=ctx->src;
      int srcp=0;
      for (;srcp<ctx->srcc;srcp+=rowlen) {
        int rdc=ctx->srcc-srcp;
        if (rdc>rowlen) rdc=rowlen;
        if (sr_encode_base64(ctx->dst,src+srcp,rdc)<0) return -1;
        if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
      }
      if (sr_encode_raw(ctx->dst,"</egg-rom>",-1)<0) return -1;
      if (sr_encode_raw(ctx->dst,tm+tmp+markerc,tmc-markerc-tmp)<0) return -1;
      return 0;
    }
  }
  return eggdev_convert_error(ctx,"Standalone HTML template does not contain a ROM insertion point (%s)",marker);
}
