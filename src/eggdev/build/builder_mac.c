#include "eggdev/eggdev_internal.h"
#include "builder.h"
#include "opt/image/image.h"
#include <time.h>

/* Replace a single token or emit it verbatim.
 * Replaceable symbols are all uppercase C identifiers:
 *  - GAMETITLE
 *  - LANGS
 *  - GAMEEXENAME
 *  - REVDNS_IDENTIFIER
 *  - PUBYEAR
 *  - AUTHOR
 * We're not doing any XML escaping, though in all cases we are begin output to XML.
 * I hope that won't come up, keep your strings neat.
 */

static int builder_mac_replace_token(struct sr_encoder *dst,struct builder *builder,const char *src,int srcc) {

  if ((srcc==9)&&!memcmp(src,"GAMETITLE",9)) {
    const char *title=0;
    int titlec=builder_get_metadata(&title,builder,"title",5);
    if (titlec>0) {
      return sr_encode_raw(dst,title,titlec);
    }
    return sr_encode_raw(dst,builder->projname,builder->projnamec); // Better than nothing I guess?
  }

  if ((srcc==5)&&!memcmp(src,"LANGS",5)) {
    const char *lang=0;
    int langc=builder_get_metadata(&lang,builder,"lang",4);
    int langp=0;
    while (langp<langc) {
      if (((unsigned char)lang[langp]<=0x20)||(lang[langp]==',')) {
        langp++;
        continue;
      }
      const char *token=lang+langp;
      int tokenc=0;
      while ((langp<langc)&&(lang[langp++]!=',')) tokenc++;
      while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
      if (!tokenc) continue;
      if (sr_encode_fmt(dst,"<string>%.*s</string>",tokenc,token)<0) return -1;
    }
    return 0;
  }

  if ((srcc==11)&&!memcmp(src,"GAMEEXENAME",11)) {
    return sr_encode_raw(dst,builder->projname,builder->projnamec);
  }

  if ((srcc==17)&&!memcmp(src,"REVDNS_IDENTIFIER",17)) {
    const char *revdns=0;
    int revdnsc=builder_get_metadata(&revdns,builder,"revdns",6);
    if (revdnsc>0) {
      return sr_encode_raw(dst,revdns,revdnsc);
    }
    // This default behavior is called out in etc/doc/metadata-format.md:
    return sr_encode_fmt(dst,"com.aksommerville.unspec.%.*s",builder->projnamec,builder->projname);
  }

  if ((srcc==7)&&!memcmp(src,"PUBYEAR",7)) {
    const char *v=0;
    int vc=builder_get_metadata(&v,builder,"time",4);
    if ((vc>=4)&&(v[0]>='0')&&(v[0]<='9')&&(v[1]>='0')&&(v[1]<='9')&&(v[2]>='0')&&(v[2]<='9')&&(v[3]>='0')&&(v[3]<='9')) {
      return sr_encode_raw(dst,v,4);
    }
    time_t now=time(0);
    struct tm *tm=localtime(&now);
    if (tm) {
      int year=1900+tm->tm_year;
      return sr_encode_fmt(dst,"%d",year);
    }
    return sr_encode_raw(dst,"????",4);
  }

  if ((srcc==6)&&!memcmp(src,"AUTHOR",6)) {
    const char *v=0;
    int vc=builder_get_metadata(&v,builder,"author",6);
    if (vc>0) return sr_encode_raw(dst,v,vc);
    return sr_encode_raw(dst,"Anonymous",9);
  }

  return sr_encode_raw(dst,src,srcc);
}

/* Replace text from Xib or Plist.
 */

static int builder_mac_isident(char ch) {
  if ((ch>='A')&&(ch<='Z')) return 1;
  if ((ch>='a')&&(ch<='z')) return 1;
  if ((ch>='0')&&(ch<='9')) return 1;
  if (ch=='_') return 1;
  return 0;
}

static int builder_mac_replace_text(void *dstpp,struct builder *builder,const char *src,int srcc,const char *path) {
  struct sr_encoder dst={0};
  int srcp=0;
  while (srcp<srcc) {
    const char *token=src+srcp++;
    int tokenc=1;
    if (builder_mac_isident(token[0])) {
      while ((srcp<srcc)&&builder_mac_isident(src[srcp])) { srcp++; tokenc++; }
      int err=builder_mac_replace_token(&dst,builder,token,tokenc);
      if (err<0) {
        sr_encoder_cleanup(&dst);
        return err;
      }
    } else {
      while ((srcp<srcc)&&!builder_mac_isident(src[srcp])) { srcp++; tokenc++; }
      if (sr_encode_raw(&dst,token,tokenc)<0) {
        sr_encoder_cleanup(&dst);
        return -1;
      }
    }
  }
  *(void**)dstpp=dst.v; // HANDOFF
  return dst.c;
}

/* Read some file from EGG_SDK/src/opt/macos/, replacing symbols.
 */

int builder_mac_read_with_replacements(void *dstpp,struct builder *builder,const char *basename) {
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%s/src/opt/macos/%s",g.sdkpath,basename);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) return builder_error(builder,"%s: Failed to read file.\n",path);
  int err=builder_mac_replace_text(dstpp,builder,src,srcc,path);
  free(src);
  return err;
}

/* Info.plist
 * Copy from EGG_SDK/src/opt/macos/Info.plist, filling in a few things.
 */
 
int build_mac_plist(struct builder *builder,struct builder_file *file) {
  char *src=0;
  int srcc=builder_mac_read_with_replacements(&src,builder,"Info.plist");
  if (srcc<0) return srcc;
  int err=file_write(file->path,src,srcc);
  free(src);
  if (err<0) return builder_error(builder,"%s: Failed to write file.\n",file->path);
  file->ready=1;
  return 0;
}

/* Main.nib
 * Replace a few things in EGG_SDK/src/opt/macos/Main.xib, then call: ibtool --compile $@ $<
 */
 
int builder_schedule_mac_nib(struct builder *builder,struct builder_step *step) {

  if (!step->file->target) return builder_error(builder,"%s: Target unset.\n",step->file->path);

  // Replace text and write to a temp file. ibtool apparently can't read from stdin.
  char *src=0;
  int srcc=builder_mac_read_with_replacements(&src,builder,"Main.xib");
  if (srcc<0) return srcc;
  char tmppath[1024];
  int tmppathc=snprintf(tmppath,sizeof(tmppath),"%.*s/mid/%.*s/Main.xib",builder->rootc,builder->root,step->file->target->namec,step->file->target->name);
  if ((tmppathc<1)||(tmppathc>=sizeof(tmppath))) {
    free(src);
    return -1;
  }
  if (file_write(tmppath,src,srcc)<0) {
    free(src);
    return builder_error(builder,"%s: Failed to write temporary xib.\n",tmppath);
  }
  free(src);

  // Schedule the ibtool call.
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"ibtool --compile %.*s %.*s",step->file->pathc,step->file->path,tmppathc,tmppath);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  return builder_begin_command(builder,step,cmd,cmdc,0,0);
}

/* appicon.icns
 * Locate the app's icon and feed to: iconutil -c icns -o $@ $(macos_ICONS_DIR)
 */

int builder_schedule_mac_icns(struct builder *builder,struct builder_step *step) {

  // Get "iconImage" and if unset, use the default icon, which is absolutely never what you want.
  const char *imageidstr=0;
  int imageidstrc=builder_get_metadata(&imageidstr,builder,"iconImage",9);
  if (imageidstrc<0) return imageidstrc;
  if (!imageidstrc) {
    char cmd[1024];
    int cmdc=snprintf(cmd,sizeof(cmd),"iconutil -c icns -o %.*s %s/src/opt/macos/appicon.iconset",step->file->pathc,step->file->path,g.sdkpath);
    if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
    return builder_begin_command(builder,step,cmd,cmdc,0,0);
  }

  /* Acquire the image.
   * Egg doesn't mandate that iconImage be PNG, but we do.
   */
  int imageid=0;
  if ((sr_int_eval(&imageid,imageidstr,imageidstrc)<2)||(imageid<1)||(imageid>0xffff)) {
    return builder_error(builder,"%s: Failed to generate icons due to invalid 'iconImage' in metadata. ('%.*s')\n",step->file->path,imageidstrc,imageidstr);
  }
  void *serial=0;
  int serialc=builder_get_resource(&serial,builder,"image",5,imageid);
  if (serialc<0) return builder_error(builder,"image:%d not found, referenced as iconImage in metadata.\n",imageid);
  if ((serialc<8)||memcmp(serial,"\x89PNG\r\n\x1a\n",8)) {
    free(serial);
    return builder_error(builder,"image:%d: Expected PNG.\n",imageid);
  }

  // Acquire dimensions. Should be 16x16 but we'll accept anything. iconutil probably won't be so tolerant.
  int w=0,h=0;
  if (image_measure(&w,&h,serial,serialc)<0) {
    free(serial);
    return builder_error(builder,"image:%d: Failed to decode image.\n",imageid);
  }

  /* Are all of the sizes mandatory? What happens if we supply only the one image?
   * Egg's general recommendation is for iconImage to be 16x16.
   */
  char ispath[1024];
  int ispathc=snprintf(ispath,sizeof(ispath),"%.*s/mid/%.*s/appicon.iconset",builder->rootc,builder->root,step->file->target->namec,step->file->target->name);
  if ((ispathc<1)||(ispathc>=sizeof(ispath))) {
    free(serial);
    return -1;
  }
  if (dir_mkdirp(ispath)<0) return builder_error(builder,"%s: mkdir failed\n",ispath);
  char tmppath[1024];
  int tmppathc=snprintf(tmppath,sizeof(tmppath),"%.*s/icon_16x16.png",ispathc,ispath);
  if ((tmppathc<1)||(tmppathc>=sizeof(tmppath))) {
    free(serial);
    return -1;
  }
  if (file_write(tmppath,serial,serialc)<0) {
    free(serial);
    return builder_error(builder,"%s: Failed to write temporary icon file, %d bytes.\n",tmppath,serialc);
  }
  free(serial);

  /* Schedule the iconutil call.
   */
  char cmd[1024];
  int cmdc=snprintf(cmd,sizeof(cmd),"iconutil -c icns -o %.*s %.*s",step->file->pathc,step->file->path,ispathc,ispath);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return -1;
  return builder_begin_command(builder,step,cmd,cmdc,0,0);
}
