/* eggdev_convert_res.c
 * Resource types specific to Egg that have text and binary forms.
 * "sprite" is one of these, but it's implemented instead in eggdev_convert_trivial.c.
 * These formats are all defined under EGG_SDK/etc/doc/.
 */

#include "eggdev/eggdev_internal.h"

/* Metadata validation.
 */
 
static int eggdev_metadata_validate_value(const char *src,int srcc) {
  if ((srcc<0)||(srcc>0xff)) return -1;
  if (!srcc) return 0;
  if ((unsigned char)src[0]<=0x20) return -1;
  if ((unsigned char)src[srcc-1]<=0x20) return -1;
  for (;srcc-->0;src++) {
    if ((*src<0x20)||(*src>0x7e)) return -1;
  }
  return 0;
}

/* Metadata bin from text.
 */

int eggdev_metadata_from_metatxt(struct eggdev_convert_context *ctx) {
  if (sr_encode_raw(ctx->dst,"\0EMD",4)<0) return -1;
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    const char *k=line;
    int kc=0,linep=0;
    while ((linep<linec)&&(line[linep++]!='=')) kc++;
    while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *v=line+linep;
    int vc=linec-linep;
    if (!kc||(eggdev_metadata_validate_value(k,kc)<0)) return eggdev_convert_error_at(ctx,lineno,"Invalid key. Must be 1..255 of G0.");
    if (eggdev_metadata_validate_value(v,vc)<0) return eggdev_convert_error_at(ctx,lineno,"Invalid value. Must be 0..255 of G0.");
    if (sr_encode_u8(ctx->dst,kc)<0) return -1;
    if (sr_encode_u8(ctx->dst,vc)<0) return -1;
    if (sr_encode_raw(ctx->dst,k,kc)<0) return -1;
    if (sr_encode_raw(ctx->dst,v,vc)<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,0)<0) return -1; // Terminator required.
  return 0;
}

/* Metadata text from bin.
 */
 
int eggdev_metatxt_from_metadata(struct eggdev_convert_context *ctx) {
  if ((ctx->srcc<4)||memcmp(ctx->src,"\0EMD",4)) return -1;
  const uint8_t *SRC=ctx->src;
  int srcp=4;
  for (;;) {
    if (srcp>=ctx->srcc) return eggdev_convert_error(ctx,"Metadata unterminated.");
    uint8_t kc=SRC[srcp++];
    if (!kc) return 0; // not kc, terminator.
    if (srcp>=ctx->srcc) return eggdev_convert_error(ctx,"Metadata overrun.");
    uint8_t vc=SRC[srcp++];
    if (srcp>ctx->srcc-vc-kc) return eggdev_convert_error(ctx,"Metadata overrun.");
    const char *k=(char*)(SRC+srcp); srcp+=kc;
    const char *v=(char*)(SRC+srcp); srcp+=vc;
    if (sr_encode_fmt(ctx->dst,"%.*s=%.*s\n",kc,k,vc,v)<0) return -1;
  }
  return 0;
}

/* Strings validation.
 */
 
static int eggdev_strings_requires_quote(const char *src,int srcc) {
  if (srcc<1) return 0;
  if (src[0]=='"') return 1;
  if ((unsigned char)src[0]<=0x20) return 1;
  if ((unsigned char)src[srcc-1]<=0x20) return 1;
  for (;srcc-->0;src++) {
    if (*src<0x20) return 1;
    if (*src>0x7e) return 1;
  }
  return 0;
}

/* Strings bin from text.
 */
 
int eggdev_strings_from_strtxt(struct eggdev_convert_context *ctx) {
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1,pvindex=0;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    
    int linep=0;
    const char *token=line;
    int tokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    int index;
    if (sr_int_eval(&index,token,tokenc)<2) return eggdev_convert_error_at(ctx,lineno,"Expected string index, found '%.*s'.",tokenc,token);
    if (index<=pvindex) return eggdev_convert_error_at(ctx,lineno,"Invalid string index %d, must be at least %d",index,pvindex);
    if (index>1024) return eggdev_convert_error_at(ctx,lineno,"Limit 1024.");
    pvindex++;
    int zeroc=(index-pvindex)*2;
    if (zeroc&&(sr_encode_zero(ctx->dst,zeroc)<0)) return -1;
    pvindex=index;
    
    token=line+linep;
    tokenc=linec-linep;
    if (tokenc&&(token[0]=='"')) { // json string
      int lenp=ctx->dst->c;
      if (sr_encode_zero(ctx->dst,2)<0) return -1;
      for (;;) {
        int err=sr_string_eval((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,token,tokenc);
        if (err<0) return eggdev_convert_error_at(ctx,lineno,"Malformed JSON string token.");
        if (ctx->dst->c<=ctx->dst->a-err) {
          ctx->dst->c+=err;
          break;
        }
        if (sr_encoder_require(ctx->dst,err)<0) return -1;
      }
      int len=ctx->dst->c-lenp-2;
      if ((len<0)||(len>0xffff)) return eggdev_convert_error_at(ctx,lineno,"String too long. %d, limit 65535.",len);
      ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
      ((uint8_t*)ctx->dst->v)[lenp+1]=len;
    } else { // loose text
      if (tokenc>0xffff) return eggdev_convert_error_at(ctx,lineno,"String too long. %d, limit 65535.",tokenc);
      if (sr_encode_intbe(ctx->dst,tokenc,2)<0) return -1;
      if (sr_encode_raw(ctx->dst,token,tokenc)<0) return -1;
    }
  }
  return 0;
}

/* Strings text from bin.
 */
 
int eggdev_strtxt_from_strings(struct eggdev_convert_context *ctx) {
  if (!ctx->src||(ctx->srcc<4)||memcmp(ctx->src,"\0EST",4)) return -1;
  const uint8_t *SRC=ctx->src;
  int srcp=4,index=1;
  for (;;) {
    if (srcp>=ctx->srcc) break;
    if (srcp>ctx->srcc-2) return eggdev_convert_error(ctx,"Strings overrun.");
    int len=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (srcp<ctx->srcc-len) return eggdev_convert_error(ctx,"Strings overrun.");
    if (len) {
      const char *string=(char*)(SRC+srcp);
      srcp+=len;
      if (sr_encode_fmt(ctx->dst,"%d ",index)<0) return -1;
      if (eggdev_strings_requires_quote(string,len)) {
        for (;;) {
          int err=sr_string_repr((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,string,len);
          if (err<0) return -1;
          if (ctx->dst->c<=ctx->dst->a-err) {
            ctx->dst->c+=err;
            break;
          }
          if (sr_encoder_require(ctx->dst,err)<0) return -1;
        }
      } else {
        if (sr_encode_raw(ctx->dst,string,len)<0) return -1;
      }
      if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
    }
    index++;
  }
  return 0;
}

/* Tilesheet bin from text.
 */
 
int eggdev_tilesheet_from_tstxt(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Tilesheet text from bin.
 */
 
int eggdev_tstxt_from_tilesheet(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Decalsheet bin from text.
 */
 
int eggdev_decalsheet_from_dstxt(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Decalsheet text from bin.
 */
 
int eggdev_dstxt_from_decalsheet(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Map bin from text.
 */
 
int eggdev_map_from_maptxt(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Map text from bin.
 */
 
int eggdev_maptxt_from_map(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Command list bin from text.
 */
 
int eggdev_cmdlist_from_cmdltxt(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}

/* Command list text from bin.
 */
 
int eggdev_cmdltxt_from_cmdlist(struct eggdev_convert_context *ctx) {
  fprintf(stderr,"TODO %s %s:%d\n",__func__,__FILE__,__LINE__);//TODO
  return -2;
}
