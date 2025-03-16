#include "mf_internal.h"

/* Guess format.
 * Usually this is eggdev_convert's problem, but in our case, we're looking
 * for three distinct text formats and can distinguish them in more opinionated ways.
 */
 
static int eggdev_minify_guess_fmt(const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp>=srcc) return 0;
  
  /* In an HTML file, the first non-space character must be '<'.
   * That would not be valid in CSS or JS.
   */
  if (src[srcp]=='<') return EGGDEV_FMT_html;
  
  /* CSS and JS have identical block comments, and it's very common to lead a file with one.
   * So continue skipping whitespace and block comments until the first other thing.
   * Line comments are "other" here -- they're only legal in JS.
   */
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (srcp>srcc-2) break;
    if ((src[srcp]!='/')||(src[srcp+1]!='*')) break;
    srcp+=2;
    for (;;) {
      if (srcp>srcc-2) return 0;
      if ((src[srcp]=='*')&&(src[srcp+1]=='/')) { srcp+=2; break; }
      srcp++;
    }
  }
  
  /* JS files will always begin with an identifier or line comment.
   * That's not strictly true, there are some legal constructions that lead with punctuation,
   * but I'm confident that will never happen in our JS.
   */
  if ((srcp<=srcc-2)&&!memcmp(src+srcp,"//",2)) return EGGDEV_FMT_js;
  const char *token=src+srcp;
  int tokenc=0;
  while (
    (srcp<srcc)&&(
      ((src[srcp]>='a')&&(src[srcp]<='z'))||
      ((src[srcp]>='A')&&(src[srcp]<='Z'))||
      ((src[srcp]>='0')&&(src[srcp]<='9'))||
      (src[srcp]=='_')
    )
  ) { tokenc++; srcp++; }
  
  /* No identifier, or something unexpected, call it CSS.
   * It's unreasonable to guess what a CSS file might lead with.
   * But a JS file will pretty much always begin "import", "export", "class", "const", or "function".
   * (with "import" being the heavy favorite).
   */
  if (!tokenc) return EGGDEV_FMT_css;
  if ((tokenc==6)&&!memcmp(token,"import",6)) return EGGDEV_FMT_js;
  if ((tokenc==6)&&!memcmp(token,"export",6)) return EGGDEV_FMT_js;
  if ((tokenc==5)&&!memcmp(token,"class",5)) return EGGDEV_FMT_js;
  if ((tokenc==5)&&!memcmp(token,"const",5)) return EGGDEV_FMT_js;
  if ((tokenc==8)&&!memcmp(token,"function",8)) return EGGDEV_FMT_js;
  if ((tokenc==3)&&!memcmp(token,"let",3)) return EGGDEV_FMT_js;
  if ((tokenc==3)&&!memcmp(token,"var",3)) return EGGDEV_FMT_js;
  return EGGDEV_FMT_css;
}

/* Minify in memory without context.
 */
 
int eggdev_minify_inner(struct sr_encoder *dst,const char *src,int srcc,const char *srcpath,int fmt) {
  int dstc0=dst->c;
  if (fmt<1) {
    fmt=eggdev_fmt_by_path(srcpath,-1);
    if (fmt<1) fmt=eggdev_minify_guess_fmt(src,srcc);
  }
  switch (fmt) {
  
    case EGGDEV_FMT_html: {
        struct eggdev_minify_html ctx={
          .dst=dst,
          .srcpath=srcpath,
          .src=src,
          .srcc=srcc,
        };
        int err=eggdev_minify_html(&ctx);
        if (err<0) {
          eggdev_minify_html_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying HTML.\n",srcpath);
          return -2;
        }
      } break;
      
    case EGGDEV_FMT_css: {
        struct eggdev_minify_css ctx={
          .dst=dst,
          .srcpath=srcpath,
          .src=src,
          .srcc=srcc,
        };
        int err=eggdev_minify_css(&ctx);
        if (err<0) {
          eggdev_minify_css_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying CSS.\n",srcpath);
          return -2;
        }
      } break;
      
    case EGGDEV_FMT_js: {
        struct eggdev_minify_js ctx={
          .dst=dst,
          .srcpath=srcpath,
        };
        int err=eggdev_minify_js(&ctx,src,srcc);
        if (err<0) {
          eggdev_minify_js_cleanup(&ctx);
          if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying Javascript.\n",srcpath);
          return -2;
        }
      } break;
      
    default: {
        fprintf(stderr,"%s:WARNING: Unexpected format for minification. Emitting verbatim.\n",srcpath);
        if (sr_encode_raw(dst,src,srcc)<0) return -1;
      }
  }
  return 0;
}

/* Minify, main entry point.
 */
 
int eggdev_main_minify() {
  if (!g.dstpath) {
    fprintf(stderr,"%s: Output path required.\n",g.exename);
    return -2;
  }
  if (g.srcpathc!=1) {
    fprintf(stderr,"%s: Minify expects exactly 1 input file, found %d.\n",g.exename,g.srcpathc);
    return -2;
  }
  const char *srcpath=g.srcpathv[0],*dstpath=g.dstpath;
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",srcpath);
    return -2;
  }
  struct sr_encoder dst={0};
  int err=eggdev_minify_inner(&dst,src,srcc,srcpath,0);
  free(src);
  if (err<0) {
    if (err!=-2) fprintf(stderr,"%s: Unspecified error minifying.\n",srcpath);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  
  // Not important, but add an LF to non-empty files, so I can cat them without fear.
  if (dst.c) sr_encode_u8(&dst,0x0a);
  
  if (file_write(dstpath,dst.v,dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file, %d bytes\n",dstpath,dst.c);
    sr_encoder_cleanup(&dst);
    return -2;
  }
  sr_encoder_cleanup(&dst);
  return 0;
}
