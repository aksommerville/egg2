/* builder_generate_restoc.c
 * This is a header in the mid directory that all C files are expected to include.
 * Just one per project, serving all targets.
 * Hard to know whether it needs rebuilt, so in fact it's simpler to blindly rebuild it every time.
 *
 * IMPORTANT: We are not tracked in (builder->filev), and this means changes to the TOC will not trigger rebuilds of every C file.
 * That is very much by design.
 * If we tracked it like other headers, every build would revisit every C file, even if nothing changed.
 */
 
#include "eggdev/eggdev_internal.h"
#include "builder.h"

/* Generate text of resource TOC in memory.
 */
 
static int builder_generate_restoc_text(struct sr_encoder *dst) {
  if (sr_encode_raw(dst,
    "#ifndef EGG_RES_TOC_H\n"
    "#define EGG_RES_TOC_H\n"
  ,-1)<0) return -1;
  const struct eggdev_ns *ns=g.client.nsv;
  int nsi=g.client.nsc;
  for (;nsi-->0;ns++) {
    switch (ns->nstype) {
      case EGGDEV_NSTYPE_RESTYPE: {
          const struct eggdev_sym *sym=ns->symv;
          int symi=ns->symc;
          for (;symi-->0;sym++) {
            if (sr_encode_fmt(dst,"#define EGG_TID_%.*s %d\n",sym->kc,sym->k,sym->v)<0) return -1;
          }
        } break;
      case EGGDEV_NSTYPE_RES: {
          const struct eggdev_sym *sym=ns->symv;
          int symi=ns->symc;
          for (;symi-->0;sym++) {
            if (sr_encode_fmt(dst,"#define RID_%.*s_%.*s %d\n",ns->namec,ns->name,sym->kc,sym->k,sym->v)<0) return -1;
          }
        } break;
    }
  }
  if (sr_encode_raw(dst,"#endif\n",-1)<0) return -1;
  return 0;
}

/* Generate resource TOC, main.
 */
 
int builder_generate_restoc(struct builder *builder) {
  int err;
  char path[1024];
  int pathc=snprintf(path,sizeof(path),"%.*s/mid/egg_res_toc.h",builder->rootc,builder->root);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  if (dir_mkdirp_parent(path)<0) return -1;
  if ((err=eggdev_client_require())<0) return err;
  struct sr_encoder text={0};
  if ((err=builder_generate_restoc_text(&text))<0) {
    sr_encoder_cleanup(&text);
    return err;
  }
  err=file_write(path,text.v,text.c);
  sr_encoder_cleanup(&text);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file.\n",path);
    return -2;
  }
  return 0;
}
