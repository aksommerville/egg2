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

int eggdev_metadata_from_metatxt(struct sr_convert_context *ctx) {
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
    if (!kc||(eggdev_metadata_validate_value(k,kc)<0)) return sr_convert_error_at(ctx,k,"Invalid key. Must be 1..255 of G0.");
    if (eggdev_metadata_validate_value(v,vc)<0) return sr_convert_error_at(ctx,v,"Invalid value. Must be 0..255 of G0.");
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
 
int eggdev_metatxt_from_metadata(struct sr_convert_context *ctx) {
  if ((ctx->srcc<4)||memcmp(ctx->src,"\0EMD",4)) return -1;
  const uint8_t *SRC=ctx->src;
  int srcp=4;
  for (;;) {
    if (srcp>=ctx->srcc) return sr_convert_error(ctx,"Metadata unterminated.");
    uint8_t kc=SRC[srcp++];
    if (!kc) return 0; // not kc, terminator.
    if (srcp>=ctx->srcc) return sr_convert_error(ctx,"Metadata overrun.");
    uint8_t vc=SRC[srcp++];
    if (srcp>ctx->srcc-vc-kc) return sr_convert_error(ctx,"Metadata overrun.");
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
 
int eggdev_strings_from_strtxt(struct sr_convert_context *ctx) {
  if (sr_encode_raw(ctx->dst,"\0EST",4)<0) return -1;
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
    if (sr_int_eval(&index,token,tokenc)<2) return sr_convert_error_at(ctx,token,"Expected string index, found '%.*s'.",tokenc,token);
    if (index<=pvindex) return sr_convert_error_at(ctx,token,"Invalid string index %d, must be at least %d",index,pvindex+1);
    if (index>1024) return sr_convert_error_at(ctx,token,"Limit 1024.");
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
        if (err<0) return sr_convert_error_at(ctx,token,"Malformed JSON string token.");
        if (ctx->dst->c<=ctx->dst->a-err) {
          ctx->dst->c+=err;
          break;
        }
        if (sr_encoder_require(ctx->dst,err)<0) return -1;
      }
      int len=ctx->dst->c-lenp-2;
      if ((len<0)||(len>0xffff)) return sr_convert_error_at(ctx,token,"String too long. %d, limit 65535.",len);
      ((uint8_t*)ctx->dst->v)[lenp]=len>>8;
      ((uint8_t*)ctx->dst->v)[lenp+1]=len;
    } else { // loose text
      if (tokenc>0xffff) return sr_convert_error_at(ctx,token,"String too long. %d, limit 65535.",tokenc);
      if (sr_encode_intbe(ctx->dst,tokenc,2)<0) return -1;
      if (sr_encode_raw(ctx->dst,token,tokenc)<0) return -1;
    }
  }
  return 0;
}

/* Strings text from bin.
 */
 
int eggdev_strtxt_from_strings(struct sr_convert_context *ctx) {
  if (!ctx->src||(ctx->srcc<4)||memcmp(ctx->src,"\0EST",4)) return -1;
  const uint8_t *SRC=ctx->src;
  int srcp=4,index=1;
  for (;;) {
    if (srcp>=ctx->srcc) break;
    if (srcp>ctx->srcc-2) return sr_convert_error(ctx,"Strings overrun.");
    int len=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (srcp>ctx->srcc-len) return sr_convert_error(ctx,"Strings overrun.");
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

/* Tilesheet bin from text, single table.
 */
 
static int eggdev_tilesheet_encode_table(struct sr_encoder *dst,int tableid,const uint8_t *src/*256*/) {
  if (!tableid) return 0; // Zero tables are for the editor only; drop them.
  int srcc=256,srcp=0;
  while (srcc&&!src[srcc-1]) srcc--;
  while ((srcp<srcc)&&!src[srcp]) srcp++;
  while (srcp<srcc) {
    if (!src[srcp]) { srcp++; continue; }
    // Consume nonzero tiles, and also runs of 1..3 zeroes. It takes 3 bytes to introduce a run.
    int cpc=1;
    while (srcp+cpc<srcc) {
      if (src[srcp+cpc]) { cpc++; continue; }
      if ((srcp+cpc+3<=srcc)&&!memcmp(src+srcp+cpc,"\0\0\0",3)) { cpc+=3; continue; }
      break;
    }
    // The above formulation might have left us with trailing zeroes. Lop those off.
    while ((cpc>1)&&!src[srcp+cpc-1]) cpc--;
    // Emit a run.
    if (sr_encode_u8(dst,tableid)<0) return -1;
    if (sr_encode_u8(dst,srcp)<0) return -1;
    if (sr_encode_u8(dst,cpc-1)<0) return -1;
    if (sr_encode_raw(dst,src+srcp,cpc)<0) return -1;
    srcp+=cpc;
  }
  return 0;
}

/* Tilesheet bin from text.
 */
 
int eggdev_tilesheet_from_tstxt(struct sr_convert_context *ctx) {
  if (sr_encode_raw(ctx->dst,"\0ETS",4)<0) return -1;
  int tableid=-1; // >=0 if read in progress (and if zero, we're going to drop it)
  uint8_t tmp[256]; // Read each table into this buffer before encoding.
  int tmpp=0;
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    
    /* If a table is being read, comments and blanks are forbidden.
     */
    if (tableid>=0) {
      if (linec!=32) return sr_convert_error_at(ctx,line,"Expected 32 hex digits.");
      int linep=0; for (;linep<32;linep+=2,tmpp++) {
        int hi=sr_digit_eval(line[linep]);
        int lo=sr_digit_eval(line[linep+1]);
        if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return sr_convert_error_at(ctx,line+linep,"Invalid hex byte '%.2s'",line+linep);
        tmp[tmpp]=(hi<<4)|lo;
      }
      if (tmpp>=256) {
        int err=eggdev_tilesheet_encode_table(ctx->dst,tableid,tmp);
        if (err<0) return err;
        tableid=-1;
        tmpp=0;
      }
      
    /* Outer scope, skip comments and blanks, and anything else must be a table name.
     */
    } else {
      if (!linec||(line[0]=='#')) continue;
      int err=eggdev_symbol_eval(&tableid,line,linec,EGGDEV_NSTYPE_NS,"tilesheet",9);
      if ((err<0)||(tableid<0)||(tableid>0xff)) return sr_convert_error_at(ctx,line,"Expected 0..255 or a tilesheet name, found '%.*s'",linec,line);
      tmpp=0;
    }
  }
  if (tableid>=0) return sr_convert_error(ctx,"Incomplete table.");
  return 0;
}

/* Tilesheet model for bin=>text.
 */
 
struct eggdev_tilesheet {
  struct eggdev_tilesheet_table {
    int id;
    uint8_t v[256];
  } *tablev;
  int tablec,tablea;
};

static void eggdev_tilesheet_cleanup(struct eggdev_tilesheet *ts) {
  if (ts->tablev) free(ts->tablev);
}

// Get or create.
static struct eggdev_tilesheet_table *eggdev_tilesheet_get_table(struct eggdev_tilesheet *ts,int tableid) {
  struct eggdev_tilesheet_table *table=ts->tablev;
  int i=ts->tablec;
  for (;i-->0;table++) if (table->id==tableid) return table;
  if (ts->tablec>=ts->tablea) {
    int na=ts->tablea+8;
    if (na>INT_MAX/sizeof(struct eggdev_tilesheet_table)) return 0;
    void *nv=realloc(ts->tablev,sizeof(struct eggdev_tilesheet_table)*na);
    if (!nv) return 0;
    ts->tablev=nv;
    ts->tablea=na;
  }
  table=ts->tablev+ts->tablec++;
  memset(table,0,sizeof(struct eggdev_tilesheet_table));
  return table;
}

/* Tilesheet text from bin.
 */
 
static int eggdev_tstxt_from_tilesheet_inner(struct sr_encoder *dst,const uint8_t *src,int srcc,struct eggdev_tilesheet *ts,struct sr_convert_context *ctx) {
  if (!src||(srcc<4)||memcmp(src,"\0ETS",4)) return sr_convert_error(ctx,"Tilesheet signature mismatch.");
  int srcp=4;
  
  // Read all content into a temporary model containing every table.
  while (srcp<srcc) {
    if (srcp>srcc-3) return sr_convert_error(ctx,"Tilesheet overrun.");
    uint8_t tableid=src[srcp++];
    uint8_t tileid=src[srcp++];
    int tilec=src[srcp++]+1;
    if (tileid+tilec>256) return sr_convert_error(ctx,"Tilesheet addresses tiles above 0xff.");
    if (srcp>srcc-tilec) return sr_convert_error(ctx,"Tilesheet overrun.");
    struct eggdev_tilesheet_table *table=eggdev_tilesheet_get_table(ts,tableid);
    if (!table) return -1;
    memcpy(table->v+tileid,src+srcp,tilec);
    srcp+=tilec;
  }
  
  // Emit all tables.
  struct eggdev_tilesheet_table *table=ts->tablev;
  int i=ts->tablec;
  for (;i-->0;table++) {
    char name[32];
    int namec=eggdev_symbol_repr(name,sizeof(name),table->id,EGGDEV_NSTYPE_NS,"tilesheet",9);
    if ((namec<1)||(namec>sizeof(name))) return sr_convert_error(ctx,"Invalid name for tilesheet table %d",table->id);
    if (sr_encode_raw(dst,name,namec)<0) return -1;
    if (sr_encode_u8(dst,0x0a)<0) return -1;
    int rowp=0; for (;rowp<256;rowp+=16) {
      const uint8_t *v=table->v+rowp;
      int i=16;
      for (;i-->0;v++) {
        if (sr_encode_u8(dst,"0123456789abcdef"[(*v)>>4])<0) return -1;
        if (sr_encode_u8(dst,"0123456789abcdef"[(*v)&15])<0) return -1;
      }
      if (sr_encode_u8(dst,0x0a)<0) return -1;
    }
    if (sr_encode_u8(dst,0x0a)<0) return -1; // Extra not-entirely-necessary newline between tables.
  }

  return 0;
}
 
int eggdev_tstxt_from_tilesheet(struct sr_convert_context *ctx) {
  struct eggdev_tilesheet ts={0};
  int err=eggdev_tstxt_from_tilesheet_inner(ctx->dst,ctx->src,ctx->srcc,&ts,ctx);
  eggdev_tilesheet_cleanup(&ts);
  return err;
}

/* Decalsheet model for text=>bin.
 */
 
struct eggdev_decalsheet {
  struct eggdev_decal {
    int id,x,y,w,h;
    const char *cmt; // Raw text.
    int cmtc; // Length of (cmt) as text, with possible whitespace.
    int cmtlen; // Output length of comment.
    int lineno;
  } *decalv;
  int decalc,decala;
};

static void eggdev_decalsheet_cleanup(struct eggdev_decalsheet *ds) {
  if (ds->decalv) free(ds->decalv);
}

static int eggdev_decalsheet_search(const struct eggdev_decalsheet *ds,int id) {
  int lo=0,hi=ds->decalc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=ds->decalv[ck].id;
         if (id<q) hi=ck;
    else if (id>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct eggdev_decal *eggdev_decalsheet_insert(struct eggdev_decalsheet *ds,int p,int id) {
  if ((p<0)||(p>ds->decalc)) return 0;
  if (p&&(id<=ds->decalv[p-1].id)) return 0;
  if ((p<ds->decalc)&&(id>=ds->decalv[p].id)) return 0;
  if (ds->decalc>=ds->decala) {
    int na=ds->decala+16;
    if (na>INT_MAX/sizeof(struct eggdev_decal)) return 0;
    void *nv=realloc(ds->decalv,sizeof(struct eggdev_decal)*na);
    if (!nv) return 0;
    ds->decalv=nv;
    ds->decala=na;
  }
  struct eggdev_decal *decal=ds->decalv+p;
  memmove(decal+1,decal,sizeof(struct eggdev_decal)*(ds->decalc-p));
  ds->decalc++;
  memset(decal,0,sizeof(struct eggdev_decal));
  decal->id=id;
  return decal;
}

/* Decalsheet bin from text.
 */
 
static void eggdev_decalsheet_decode_comment(uint8_t *dst,int dstc,const char *src,int srcc) {
  if (!dstc) return;
  int srcp=0,dstp=0,hi=-1;
  for (;srcp<srcc;srcp++) {
    int digit=sr_digit_eval(src[srcp]);
    if ((digit<0)||(digit>15)) continue;
    if (hi<0) hi=digit;
    else {
      dst[dstp++]=(hi<<4)|digit;
      if (dstp>=dstc) return;
      hi=-1;
    }
  }
  memset(dst+dstp,0,dstc-dstp);
}
 
static int eggdev_decalsheet_from_dstxt_inner(struct sr_convert_context *ctx,struct eggdev_decalsheet *ds) {
  
  /* Start by reading the entire sheet into the temporary model.
   * This is absolutely necessary, since the output must be sorted but the input need not.
   * And also because we need to know the longest comment length before emitting any entry.
   */
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    int linep=0,tokenc,err;
    const char *token;
    int decalid,x,y,w,h;
    
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    token=line+linep;
    tokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    if ((eggdev_symbol_eval(&decalid,token,tokenc,EGGDEV_NSTYPE_NS,"decal",5)<0)||(decalid<1)||(decalid>0xff)) {
      return sr_convert_error_at(ctx,token,"Expected 1..255 or decal name, found '%.*s'",tokenc,token);
    }
    
    #define INTTOKEN(var) { \
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++; \
      token=line+linep; \
      tokenc=0; \
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++; \
      if ((sr_int_eval(&var,token,tokenc)<2)||(var<0)||(var>0xffff)) { \
        return sr_convert_error_at(ctx,token,"Expected 0..65535, found '%.*s'",tokenc,token); \
      } \
    }
    INTTOKEN(x)
    INTTOKEN(y)
    INTTOKEN(w)
    INTTOKEN(h)
    #undef INTTOKEN
    
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    const char *cmt=line+linep;
    int cmtc=linec-linep;
    int cmtp=0,digitc=0;
    for (;cmtp<cmtc;cmtp++) {
      if ((unsigned char)cmt[cmtp]<=0x20) continue;
      if (
        ((cmt[cmtp]>='0')&&(cmt[cmtp]<='9'))||
        ((cmt[cmtp]>='a')&&(cmt[cmtp]<='f'))||
        ((cmt[cmtp]>='A')&&(cmt[cmtp]<='F'))
      ) digitc++;
      else return sr_convert_error_at(ctx,cmt+cmtp,"Unexpected character '%c' in hex dump.",cmt[cmtp]);
    }
    if (digitc&1) return sr_convert_error_at(ctx,cmt,"Uneven hex dump length.");
    
    int p=eggdev_decalsheet_search(ds,decalid);
    if (p>=0) return sr_convert_error_at(ctx,line,"Duplicate decal ID %d (previous at line %d)",decalid,ds->decalv[p].lineno+ctx->lineno0);
    struct eggdev_decal *decal=eggdev_decalsheet_insert(ds,-p-1,decalid);
    if (!decal) return -1;
    decal->x=x;
    decal->y=y;
    decal->w=w;
    decal->h=h;
    decal->cmt=cmt;
    decal->cmtc=cmtc;
    decal->cmtlen=digitc>>1;
    if (decal->cmtlen>0xff) return sr_convert_error_at(ctx,line,"%d-byte comment exceeds 255-byte limit.",decal->cmtlen);
  }
  
  /* Determine comment length.
   */
  int cmtlen=0;
  struct eggdev_decal *decal=ds->decalv;
  int i=ds->decalc;
  for (;i-->0;decal++) {
    if (decal->cmtlen>cmtlen) cmtlen=decal->cmtlen;
  }
  
  /* Emit header and decals.
   */
  if (sr_encode_raw(ctx->dst,"\0EDS",4)<0) return -1;
  if (sr_encode_u8(ctx->dst,cmtlen)<0) return -1;
  for (decal=ds->decalv,i=ds->decalc;i-->0;decal++) {
    if (sr_encode_u8(ctx->dst,decal->id)<0) return -1;
    if (sr_encode_intbe(ctx->dst,decal->x,2)<0) return -1;
    if (sr_encode_intbe(ctx->dst,decal->y,2)<0) return -1;
    if (sr_encode_intbe(ctx->dst,decal->w,2)<0) return -1;
    if (sr_encode_intbe(ctx->dst,decal->h,2)<0) return -1;
    if (sr_encoder_require(ctx->dst,cmtlen)<0) return -1;
    eggdev_decalsheet_decode_comment((uint8_t*)ctx->dst->v+ctx->dst->c,cmtlen,decal->cmt,decal->cmtc);
    ctx->dst->c+=cmtlen;
  }
  
  return 0;
}
 
int eggdev_decalsheet_from_dstxt(struct sr_convert_context *ctx) {
  struct eggdev_decalsheet ds={0};
  int err=eggdev_decalsheet_from_dstxt_inner(ctx,&ds);
  eggdev_decalsheet_cleanup(&ds);
  return err;
}

/* Decalsheet text from bin.
 */
 
int eggdev_dstxt_from_decalsheet(struct sr_convert_context *ctx) {
  if (!ctx->src||(ctx->srcc<5)||memcmp(ctx->src,"\0EDS",4)) return sr_convert_error(ctx,"Decalsheet signature mismatch.");
  const uint8_t *src=ctx->src;
  int cmtlen=src[4];
  int reclen=9+cmtlen;
  int srcp=5;
  while (srcp<ctx->srcc) {
    if (srcp>ctx->srcc-reclen) return sr_convert_error(ctx,"Decalsheet overrun.");
    int id=src[srcp++];
    int x=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    int y=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    int w=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    int h=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    char name[32];
    int namec=eggdev_symbol_repr(name,sizeof(name),id,EGGDEV_NSTYPE_NS,"decal",5);
    if ((namec<1)||(namec>sizeof(name))) return -1;
    if (sr_encode_fmt(ctx->dst,"%.*s %d %d %d %d ",namec,name,x,y,w,h)<0) return -1;
    
    const uint8_t *cmt=src+srcp;
    int cmtc=cmtlen;
    srcp+=cmtlen;
    while (cmtc&&!cmt[cmtc-1]) cmtc--;
    for (;cmtc-->0;cmt++) {
      if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*cmt)>>4])<0) return -1;
      if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*cmt)&15])<0) return -1;
    }
    
    if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  }
  return 0;
}

/* Map bin from text.
 */
 
int eggdev_map_from_maptxt(struct sr_convert_context *ctx) {
  int dstc0=ctx->dst->c;
  if (sr_encode_raw(ctx->dst,"\0EMP\0\0",6)<0) return -1; // Placeholders for width and height.
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1;
  int w=0,h=0; // Nonzero if we've acquired the first line.
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) {
      if (w) break; // First empty line after the cells image separates cells and commands.
      continue; // Empty lines grudgingly permitted before the cells image.
    }
    if (linec&1) return sr_convert_error_at(ctx,line,"Uneven count of hex digits.");
    int linew=linec>>1;
    
    // First line establishes the width, subsequent lines must match it.
    if (w) {
      if (linew!=w) return sr_convert_error_at(ctx,line,"Expected %d cells, found %d.",w,linew);
    } else {
      if (linew>0xff) return sr_convert_error_at(ctx,line,"Width %d exceeds limit 255.",linew);
      w=linew;
    }
    
    // Unhexdump. Spaces etc are not permitted.
    int linep=0;
    while (linep<linec) {
      int hi=sr_digit_eval(line[linep++]);
      int lo=sr_digit_eval(line[linep++]);
      if ((hi<0)||(hi>15)||(lo<0)||(lo>15)) return sr_convert_error_at(ctx,line,"Invalid hex byte '%.2s'.",line+linep-2);
      if (sr_encode_u8(ctx->dst,(hi<<4)|lo)<0) return -1;
    }
    h++;
    if (h>0xff) return sr_convert_error_at(ctx,line,"Height exceeds limit 255.");
  }
  
  // Validate dimensions and finish the header.
  if (!w||!h) return sr_convert_error(ctx,"Cells image not found.");
  if (ctx->dst->c-dstc0-6!=w*h) return sr_convert_error(ctx,"%s:%d: Oops. w=%d h=%d, cells len=%d",__FILE__,__LINE__,w,h,ctx->dst->c-dstc0-6);
  ((uint8_t*)ctx->dst->v)[dstc0+4]=w;
  ((uint8_t*)ctx->dst->v)[dstc0+5]=h;
  
  // Remainder is a cmdlist.
  struct sr_convert_context subctx=*ctx;
  subctx.src=(char*)ctx->src+decoder.p;
  subctx.srcc=decoder.c-decoder.p;
  subctx.lineno0=lineno;
  char *argv[]={0,"--ns=map"};
  subctx.argv=argv;
  subctx.argc=2;
  return eggdev_cmdlist_from_cmdltxt(&subctx);
}

/* Map text from bin.
 */
 
int eggdev_maptxt_from_map(struct sr_convert_context *ctx) {
  if (!ctx->src||(ctx->srcc<6)||memcmp(ctx->src,"\0EMP",4)) return -1;
  const uint8_t *src=ctx->src;
  int w=src[4];
  int h=src[5];
  int srcp=6;
  if ((w<1)||(h<1)) return sr_convert_error(ctx,"Invalid map dimensions %d,%d",w,h);
  if (srcp>ctx->srcc-w*h) return sr_convert_error(ctx,"Map cells overrun.");
  
  // Emit cells image.
  int yi=h;
  for (;yi-->0;) {
    int xi=w;
    for (;xi-->0;srcp++) {
      if (sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]>>4])<0) return -1;
      if (sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]&15])<0) return -1;
    }
    if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  
  // Remainder is a cmdlist.
  struct sr_convert_context subctx=*ctx;
  subctx.src=src+srcp;
  subctx.srcc=ctx->srcc-srcp;
  char *argv[]={0,"--ns=map"};
  subctx.argv=argv;
  subctx.argc=2;
  return eggdev_cmdltxt_from_cmdlist(&subctx);
}

/* Single command list arg bin from text.
 * "*" is an error here, since we don't have appropriate context to implement it.
 */
 
static int eggdev_cmdlist_compile_arg(struct sr_convert_context *ctx,const char *src,int srcc,int lineno) {
  if (srcc<1) return 0;

  /* "0x..." for a simple hex dump.
   */
  if ((srcc>=2)&&(src[0]=='0')&&((src[1]=='x')||(src[1]=='X'))) {
    if (srcc&1) return sr_convert_error_at(ctx,src,"Uneven hex dump length.");
    int srcp=2; for (;srcp<srcc;) {
      int hi=sr_digit_eval(src[srcp++]);
      int lo=sr_digit_eval(src[srcp++]);
      if ((hi<0)||(lo<0)||(hi>15)||(lo>15)) return sr_convert_error_at(ctx,src+srcp,"Invalid hex byte '%.2s'",src+srcp-2);
      if (sr_encode_u8(ctx->dst,(hi<<4)|lo)<0) return -1;
    }
    return 0;
  }
  
  /* "@N[,N,...]" for an array of u8.
   */
  if (src[0]=='@') {
    int srcp=1;
    while (srcp<srcc) {
      if (src[srcp]==',') { srcp++; continue; }
      const char *token=src+srcp;
      int tokenc=0;
      while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
      int v;
      if ((sr_int_eval(&v,token,tokenc)<2)||(v<0)||(v>255)) {
        return sr_convert_error_at(ctx,token,"Expected integer in 0..255, found '%.*s'",tokenc,token);
      }
      if (sr_encode_u8(ctx->dst,v)<0) return -1;
    }
    return 0;
  }
  
  /* Naked integers are u8.
   */
  if (
    ((src[0]>='0')&&(src[0]<='9'))||
    ((srcc>=2)&&(src[0]=='-')&&(src[1]>='0')&&(src[1]<='9'))
  ) {
    int v;
    if ((sr_int_eval(&v,src,srcc)<2)||(v<-128)||(v>255)) {
      return sr_convert_error_at(ctx,src,"Expected integer in 0..255, found '%.*s'",srcc,src);
    }
    return sr_encode_u8(ctx->dst,v);
  }
  
  /* JSON strings are verbatim UTF-8.
   */
  if (src[0]=='"') {
    for (;;) {
      int err=sr_string_eval((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,src,srcc);
      if (err<0) return sr_convert_error_at(ctx,src,"Failed to evaluate string. (does it contain whitespace? it's not allowed to)");
      if (ctx->dst->c<=ctx->dst->a-err) {
        ctx->dst->c+=err;
        return 0;
      }
      if (sr_encoder_require(ctx->dst,err)<0) return -1;
    }
  }
  
  /* "(uSIZE[:NAMESPACE])VALUE" for a single lookup, or
   * "(bSIZE[:NAMESPACE])VALUE[,VALUE,...]" for bitfields lookup.
   */
  if (src[0]=='(') {
    int srcp=1;
    if (srcp>=srcc) return sr_convert_error_at(ctx,src,"Invalid token '%.*s'",srcc,src);
    char mode=src[srcp++];
    if ((mode!='u')&&(mode!='b')) return sr_convert_error_at(ctx,src,"Expected 'u' or 'b', found '%c' ('%.*s')",mode,srcc,src);
    int size=0;
    while ((srcp<srcc)&&(src[srcp]>='0')&&(src[srcp]<='9')) {
      size*=10;
      size+=src[srcp++]-'0';
      if (size>256) break;
    }
    if ((size<8)||(size>32)||(size&7)) return sr_convert_error_at(ctx,src,"Expected size 8, 16, 24, or 32. Found %d.",size);
    size>>=3; // Rephrase as bytes.
    const char *ns=0;
    int nsc=0;
    if ((srcp<srcc)&&(src[srcp]==':')) {
      srcp++;
      ns=src+srcp;
      while ((srcp<srcc)&&(src[srcp]!=')')) { srcp++; nsc++; }
    }
    if ((srcp>=srcc)||(src[srcp++]!=')')) return sr_convert_error_at(ctx,src,"Expected ')'");
    int value=0;
    if (mode=='u') {
      if (eggdev_symbol_eval(&value,src+srcp,srcc-srcp,EGGDEV_NSTYPE_NS,ns,nsc)<0) {
        return sr_convert_error_at(ctx,src,"Failed to resolve symbol '%.*s' in namespace '%.*s'",srcc-srcp,src+srcp,nsc,ns);
      }
    } else if (mode=='b') {
      while (srcp<srcc) {
        if (src[srcp]==',') { srcp++; continue; }
        const char *token=src+srcp;
        int tokenc=0;
        while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
        int bit;
        if (eggdev_symbol_eval(&bit,token,tokenc,EGGDEV_NSTYPE_NS,ns,nsc)<0) {
          return sr_convert_error_at(ctx,token,"Failed to resolve symbol '%.*s' in namespace '%.*s'",tokenc,token,nsc,ns);
        }
        value|=1<<bit;
      }
    }
    return sr_encode_intbe(ctx->dst,value,size);
  }
  
  /* "TYPE:NAME" for a resource ID lookup.
   */
  int sepp=-1;
  int i=0; for (;i<srcc;i++) if (src[i]==':') { sepp=i; break; }
  if (sepp>=0) {
    const char *tname=src,*rname=src+sepp+1;
    int tnamec=sepp,rnamec=srcc-sepp-1,rid;
    if ((eggdev_symbol_eval(&rid,rname,rnamec,EGGDEV_NSTYPE_RES,tname,tnamec)<0)||(rid<0)||(rid>0xffff)) {
      return sr_convert_error_at(ctx,rname,"Failed to evaluate '%.*s' as resource ID.",srcc,src);
    }
    return sr_encode_intbe(ctx->dst,rid,2);
  }

  return sr_convert_error_at(ctx,src,"Unexpected token '%.*s' in command list.",srcc,src);
}

/* Get command list namespace from conversion context.
 */
 
static int eggdev_cmdlist_get_ns(void *dstpp,const struct sr_convert_context *ctx) {
  // First the easy way: If "--ns=ABC" was provided, that's the answer.
  int argc=sr_convert_arg(dstpp,ctx,"ns",2);
  if (argc>0) return argc;
  // Otherwise, use the last directory of the input path.
  if (ctx->refname) {
    const char *src=ctx->refname;
    int srcp=0;
    const char *dir=0,*base=src;
    int dirc=0,basec=0;
    for (;src[srcp];srcp++) {
      if (src[srcp]=='/') {
        dir=base;
        dirc=basec;
        base=src+srcp+1;
        basec=0;
      } else {
        basec++;
      }
    }
    if (dirc>0) {
      *(const void**)dstpp=dir;
      return dirc;
    }
  }
  // And finally, i dunno. cmdlist are possible without a namespace (tho inconvenient), maybe that's what they want.
  return 0;
}

/* Command list bin from text.
 */
 
int eggdev_cmdlist_from_cmdltxt(struct sr_convert_context *ctx) {
  const char *ns=0;
  int nsc=eggdev_cmdlist_get_ns(&ns,ctx);
  if (nsc<0) nsc=0;
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { linec--; line++; }
    if (!linec||(line[0]=='#')) continue;
    int linep=0,tokenc;
    const char *token;
    
    // Opcode.
    token=line+linep;
    tokenc=0;
    while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    int opcode;
    if ((eggdev_symbol_eval(&opcode,token,tokenc,EGGDEV_NSTYPE_CMD,ns,nsc))<0) {
      return sr_convert_error_at(ctx,token,"Unknown opcode '%.*s'",tokenc,token);
    }
    if ((opcode<1)||(opcode>0xff)) return sr_convert_error_at(ctx,token,"'%.*s' yields invalid opcode %d, must be in 1..255.",tokenc,token,opcode);
    if (sr_encode_u8(ctx->dst,opcode)<0) return -1;
    
    // Track start of payload. For 0xe0..0xff, emit a placeholder payload length.
    if (opcode>=0xe0) {
      if (sr_encode_u8(ctx->dst,0)<0) return -1;
    }
    int payloadp=ctx->dst->c;
    
    // Generic arguments.
    int pad=0;
    while (linep<linec) {
      token=line+linep;
      tokenc=0;
      while ((linep<linec)&&((unsigned char)line[linep++]>0x20)) tokenc++;
      while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
      if ((tokenc==1)&&(token[0]=='*')) {
        pad=1;
        if (linep<linec) return sr_convert_error_at(ctx,token,"'*' must be the final argument.");
        break;
      }
      int err=eggdev_cmdlist_compile_arg(ctx,token,tokenc,lineno);
      if (err<0) return err;
    }
    
    // Validate payload length, or pad out if we're doing that.
    int expectlen;
    switch (opcode&0xe0) {
      case 0x00: expectlen=0; break;
      case 0x20: expectlen=2; break;
      case 0x40: expectlen=4; break;
      case 0x60: expectlen=8; break;
      case 0x80: expectlen=12; break;
      case 0xa0: expectlen=16; break;
      case 0xc0: expectlen=20; break;
      case 0xe0: {
          expectlen=ctx->dst->c-payloadp;
          if (expectlen>0xff) return sr_convert_error_at(ctx,line,"%d-byte payload exceeds limit 255.",expectlen);
          ((uint8_t*)ctx->dst->v)[payloadp-1]=expectlen;
        } break;
      default: return -1;
    }
    if (pad) {
      int havelen=ctx->dst->c-payloadp;
    }
    int havelen=ctx->dst->c-payloadp;
    if ((havelen<expectlen)&&pad) {
      if (sr_encode_zero(ctx->dst,expectlen-havelen)<0) return -1;
      havelen=expectlen;
    }
    if (havelen!=expectlen) return sr_convert_error_at(ctx,line,"Expected %d bytes payload, found %d. (opcode 0x%02x)",expectlen,havelen,opcode);
  }
  return 0;
}

/* Command list text from bin.
 */
 
int eggdev_cmdltxt_from_cmdlist(struct sr_convert_context *ctx) {
  const char *ns=0;
  int nsc=eggdev_cmdlist_get_ns(&ns,ctx);
  if (nsc<0) nsc=0;
  const uint8_t *src=ctx->src;
  int srcp=0;
  while (srcp<ctx->srcc) {
    uint8_t opcode=src[srcp++];
    if (!opcode) return sr_convert_error(ctx,"Illegal NUL in command list.");
    int paylen=0;
    switch (opcode&0xe0) {
      case 0x00: paylen=0; break;
      case 0x20: paylen=2; break;
      case 0x40: paylen=4; break;
      case 0x60: paylen=8; break;
      case 0x80: paylen=12; break;
      case 0xa0: paylen=16; break;
      case 0xc0: paylen=20; break;
      case 0xe0: {
          if (srcp>=ctx->srcc) return sr_convert_error(ctx,"Command list overrun.");
          paylen=src[srcp++];
        } break;
    }
    if (srcp>ctx->srcc-paylen) return sr_convert_error(ctx,"Command list overrun.");
    
    if (sr_encoder_require(ctx->dst,32)<0) return -1;
    int err=eggdev_symbol_repr((char*)ctx->dst->v+ctx->dst->c,ctx->dst->a-ctx->dst->c,opcode,EGGDEV_NSTYPE_CMD,ns,nsc);
    if ((err<=0)||(ctx->dst->c>ctx->dst->a-err)) return -1;
    ctx->dst->c+=err;
    
    if (paylen) {
      /* TODO There's an opportunity here for shared_symbols to declare the preferred argument layout,
       * and if our payload matches it, emit sensible symbols.
       * I think that would be a whole lot of buck for not very much bang, so just doing hex dumps for now.
       * We might want those argument format declarations anyway for the editor (more bang), and then maybe
       * wouldn't be so much effort to use it here too.
       */
      if (sr_encode_raw(ctx->dst," 0x",3)<0) return -1;
      int i=paylen;
      for (;i-->0;srcp++) {
        if (sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]>>4])<0) return -1;
        if (sr_encode_u8(ctx->dst,"0123456789abcdef"[src[srcp]&15])<0) return -1;
      }
    }
    
    if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  }
  return 0;
}

/* JSON from binary save.
 */
 
int eggdev_json_from_save(struct sr_convert_context *ctx) {
  const uint8_t *src=ctx->src;
  int srcp=0;
  int jsonctx=sr_encode_json_object_start(ctx->dst,0,0);
  while (srcp<ctx->srcc) {
    if (srcp>ctx->srcc-3) return sr_convert_error(ctx,"Malformed save file.");
    int kc=src[srcp++];
    int vc=src[srcp++]<<8;
    vc|=src[srcp++];
    if (srcp>ctx->srcc-vc-kc) return sr_convert_error(ctx,"Malformed save file.");
    const char *k=(char*)(src+srcp); srcp+=kc;
    const char *v=(char*)(src+srcp); srcp+=vc;
    if (sr_encode_json_string(ctx->dst,k,kc,v,vc)<0) return -1;
  }
  if (sr_encode_json_end(ctx->dst,jsonctx)<0) return -1;
  sr_encode_u8(ctx->dst,0x0a); // Just to be polite.
  return 0;
}

/* Binary save from JSON.
 */
 
int eggdev_save_from_json(struct sr_convert_context *ctx) {
  struct sr_decoder decoder={.v=ctx->src,.c=ctx->srcc};
  int jsonctx=sr_decode_json_object_start(&decoder);
  if (jsonctx<0) return sr_convert_error(ctx,"Malformed JSON.");
  const char *k;
  int kc;
  while ((kc=sr_decode_json_next(&k,&decoder))>0) {
    if (kc>0xff) return sr_convert_error(ctx,"Key too long (%d, limit 255).",kc);
    sr_encode_u8(ctx->dst,kc);
    int vlenp=ctx->dst->c;
    sr_encode_intbe(ctx->dst,0,2);
    sr_encode_raw(ctx->dst,k,kc);
    int vp=ctx->dst->c;
    if (sr_decode_json_string_to_encoder(ctx->dst,&decoder)<0) return sr_convert_error(ctx,"Malformed JSON.");
    int vlen=ctx->dst->c-vp;
    if ((vlen<0)||(vlen>0xffff)) return sr_convert_error(ctx,"Value too long (%d, limit 65535).",vlen);
    ((uint8_t*)ctx->dst->v)[vlenp]=vlen>>8;
    ((uint8_t*)ctx->dst->v)[vlenp+1]=vlen;
  }
  return sr_encoder_assert(ctx->dst);
}
