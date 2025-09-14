#include "font_internal.h"

/* Delete.
 */
 
void font_page_cleanup(struct font_page *page) {
  if (page->bits) free(page->bits);
  if (page->glyphv) free(page->glyphv);
}

void font_del(struct font *font) {
  if (!font) return;
  if (font->pagev) {
    while (font->pagec-->0) font_page_cleanup(font->pagev+font->pagec);
    free(font->pagev);
  }
  free(font);
}

/* New.
 */

struct font *font_new() {
  struct font *font=calloc(1,sizeof(struct font));
  if (!font) return 0;
  return font;
}

/* Trivial accessors.
 */

int font_get_line_height(const struct font *font) {
  if (!font) return 0;
  return font->lineh;
}

/* Page list in font.
 */
 
int font_pagev_search(const struct font *font,int codepoint) {
  int lo=0,hi=font->pagec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct font_page *page=font->pagev+ck;
    if (codepoint<page->codepoint) hi=ck;
    else if (codepoint>=page->codepoint+page->glyphc) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

struct font_page *font_pagev_insert(struct font *font,int p,int codepoint) {
  if ((p<0)||(p>font->pagec)) return 0;
  if (p&&(codepoint<font->pagev[p-1].codepoint+font->pagev[p-1].glyphc)) return 0;
  if ((p<font->pagec)&&(codepoint>=font->pagev[p].codepoint)) return 0;
  if (font->pagec>=font->pagea) {
    int na=font->pagea+8;
    if (na>INT_MAX/sizeof(struct font_page)) return 0;
    void *nv=realloc(font->pagev,sizeof(struct font_page)*na);
    if (!nv) return 0;
    font->pagev=nv;
    font->pagea=na;
  }
  struct font_page *page=font->pagev+p;
  memmove(page+1,page,sizeof(struct font_page)*(font->pagec-p));
  font->pagec++;
  memset(page,0,sizeof(struct font_page));
  page->codepoint=codepoint;
  return page;
}

/* Glyph lookup.
 */
 
int font_get_glyph_width(const struct font *font,int codepoint) {
  if (!font) return 0;
  int pagep=font_pagev_search(font,codepoint);
  if (pagep<0) return 0;
  const struct font_page *page=font->pagev+pagep;
  codepoint-=page->codepoint;
  if ((codepoint<0)||(codepoint>=page->glyphc)) return 0; // font_pagev_search() wouldn't have returned it in this case, but let's be safe.
  const struct font_glyph *glyph=page->glyphv+codepoint;
  return glyph->w;
}

/* Measure string.
 */
 
int font_measure_string(const struct font *font,const char *src,int srcc) {
  if (!font) return 0;
  struct font_string_reader reader;
  font_string_reader_init(&reader,src,srcc);
  int codepoint,w=0;
  uint8_t wcache[256]={0}; // Store widths for the 8859-1 range first time we encounter each.
  while (font_string_reader_next(&codepoint,&reader)>0) {
    if (codepoint<0x100) {
      if (!wcache[codepoint]) wcache[codepoint]=font_get_glyph_width(font,codepoint);
      w+=wcache[codepoint];
    } else {
      w+=font_get_glyph_width(font,codepoint);
    }
  }
  return w;
}
  
/* String reader.
 */
 
void font_string_reader_init(struct font_string_reader *reader,const void *v,int c) {
  if (!v) c=0; else if (c<0) { c=0; while (((char*)v)[c]) c++; }
  reader->v=v;
  reader->c=c;
  reader->p=0;
}

int font_string_reader_next(int *codepoint,struct font_string_reader *reader) {
  if (reader->p>=reader->c) return 0;
  *codepoint=reader->v[reader->p++];
  if (*codepoint<0x80) return 1;
  if (!((*codepoint)&0x40)) return 1; // Unexpected trailing byte; fallback to 8859-1.
  int seqlen; // Additional length, not counting the leading byte we just read.
  uint8_t leadmask;
       if (!((*codepoint)&0x20)) { seqlen=1; leadmask=0x1f; }
  else if (!((*codepoint)&0x10)) { seqlen=2; leadmask=0x0f; }
  else if (!((*codepoint)&0x08)) { seqlen=3; leadmask=0x07; }
  else return 1; // Invalid leading byte; fallback to 8859-1.
  if (reader->p>reader->c-seqlen) return 1; // Unexpected EOF; fallback to 8859-1.
  int i=seqlen;
  while (i-->0) {
    if ((reader->v[reader->p+i]&0xc0)!=0x80) return 1; // Invalid trailing byte; fallback to 8859-1 with the lead.
  }
  // Beyond this point, the sequence is valid. We're permitting overencoding.
  (*codepoint)&=leadmask;
  for (i=seqlen;i-->0;reader->p++) {
    (*codepoint)<<=6;
    (*codepoint)|=reader->v[reader->p]&0x3f;
  }
  return 1;
}
