#include "eggdev_serve_internal.h"

/* Cleanup.
 */
 
void eggdev_http_paths_cleanup(struct eggdev_http_paths *paths) {
  if (paths->local) free(paths->local);
  if (paths->inner) free(paths->inner);
}

/* Set strings.
 */
 
static int eggdev_http_paths_set_local(struct eggdev_http_paths *paths,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (paths->local) free(paths->local);
  paths->local=nv;
  paths->localc=srcc;
  return 0;
}
 
static int eggdev_http_paths_set_inner(struct eggdev_http_paths *paths,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc+1))) return -1;
    memcpy(nv,src,srcc);
    nv[srcc]=0;
  }
  if (paths->inner) free(paths->inner);
  paths->inner=nv;
  paths->innerc=srcc;
  return 0;
}

/* Populate (local,inner,ftype,reqpfx,localpfx) from (g.htdocs) according to (reqpath).
 * Checks all htdocs, and only returns if the file actually exists.
 */
 
static int eggdev_http_paths_find_htdocs_reading(struct eggdev_http_paths *paths) {
  int i=g.htdocsc;
  while (i-->0) {
    const char *ht=g.htdocsv[i];
    int sepp=-1,htc=0;
    for (;ht[htc];htc++) {
      if ((sepp<0)&&(ht[htc]==':')) sepp=htc;
    }
    
    /* If htdocs has a request prefix, it must match.
     * That may be the entire request path.
     * But if there's more request path, the remainder must start with a slash.
     */
    const char *htpost=ht;
    int htpostc=htc;
    const char *reqpost=paths->reqpath;
    int reqpostc=paths->reqpathc;
    if (sepp>=0) {
      if (sepp>paths->reqpathc) continue;
      if (memcmp(ht,paths->reqpath,sepp)) continue;
      if ((sepp<paths->reqpathc)&&(paths->reqpath[sepp]!='/')) continue;
      htpost=ht+sepp+1;
      htpostc=htc-sepp-1;
      reqpost=paths->reqpath+sepp;
      reqpostc=paths->reqpathc-sepp;
    }
    
    /* If (htpost) ends ".zip", we're not going to unpack it to test whether the file exists.
     * We assume every file does exist, within the archive.
     * This is why we strongly recommend a request prefix for Zipped htdocs.
     */
    if ((htpostc>=4)&&!memcmp(htpost+htpostc-4,".zip",4)) {
      while ((reqpostc>=1)&&(reqpost[0]=='/')) { reqpost++; reqpostc--; }
      if (eggdev_http_paths_set_local(paths,htpost,htpostc)<0) return -1;
      if (eggdev_http_paths_set_inner(paths,reqpost,reqpostc)<0) return -1;
      paths->ftype='f';
      if (sepp>=0) {
        paths->reqpfx=ht;
        paths->reqpfxc=sepp;
      } else {
        paths->reqpfx="";
        paths->reqpfxc=0;
      }
      paths->localpfx=htpost;
      paths->localpfxc=htpostc;
      return 0;
    }
    
    /* Try the composed path.
     */
    char local[1024];
    int localc=path_join(local,sizeof(local),htpost,htpostc,reqpost,reqpostc);
    if ((localc<1)||(localc>=sizeof(local))) continue;
    while ((localc>1)&&(local[localc-1]=='/')) localc--;
    local[localc]=0;
    char ftype=file_get_type(local);
    if (!ftype) continue;
    
    /* File does exist. Commit and return.
     */
    if (eggdev_http_paths_set_local(paths,local,localc)<0) return -1;
    paths->ftype=ftype;
    if (sepp>=0) {
      paths->reqpfx=ht;
      paths->reqpfxc=sepp;
    } else {
      paths->reqpfx="";
      paths->reqpfxc=0;
    }
    paths->localpfx=htpost;
    paths->localpfxc=htpostc;
    return 0;
  }
  return -1;
}

/* Populate (local,reqpfx,localpfx) from (g.htdocs) according to (reqpath).
 * 
 */
 
static int eggdev_http_paths_find_htdocs_writing(struct eggdev_http_paths *paths) {

  /* Find the (g.htdocsv) entry corresponding to (g.writeable).
   * Maybe we should cache this? It can't change during the program's run.
   */
  if (!g.writeable||!g.writeable[0]) return -1;
  int writeablec=0;
  while (g.writeable[writeablec]) writeablec++;
  const char *ht=0;
  int htc=0,htsepp=-1;
  int i=g.htdocsc;
  while (i-->0) {
    const char *q=g.htdocsv[i];
    int qc=0,qsepp=-1;
    for (;q[qc];qc++) {
      if ((qsepp<0)&&(q[qc]==':')) qsepp=qc;
    }
    const char *qpost=q;
    int qpostc=qc;
    if (qsepp>=0) {
      qpost+=qsepp+1;
      qpostc-=qsepp+1;
    }
    if (qpostc!=writeablec) continue;
    if (memcmp(qpost,g.writeable,writeablec)) continue;
    ht=q;
    htc=qc;
    htsepp=qsepp;
    break;
  }
  if (!ht) return -1;
  
  if (htsepp>=0) {
    paths->reqpfx=ht;
    paths->reqpfxc=htsepp;
    paths->localpfx=ht+htsepp+1;
    paths->localpfxc=htc-htsepp-1;
  } else {
    paths->reqpfx="";
    paths->reqpfxc=0;
    paths->localpfx=ht;
    paths->localpfxc=htc;
  }
  
  char tmp[1024];
  int tmpc=path_join(tmp,sizeof(tmp),paths->localpfx,paths->localpfxc,paths->reqpath+paths->reqpfxc,paths->reqpathc-paths->reqpfxc);
  if ((tmpc<1)||(tmpc>=sizeof(tmp))) return -1;
  if (eggdev_http_paths_set_local(paths,tmp,tmpc)<0) return -1;
  return 0;
}

/* Resolve, main entry point.
 */

int eggdev_http_paths_resolve(
  struct eggdev_http_paths *paths,
  const char *reqpath,int reqpathc,
  int writing
) {
  if (paths->local||paths->inner) return -1;

  /* Measure input, and replace empty with "/".
   */
  if (!reqpath) reqpathc=0; else if (reqpathc<0) { reqpathc=0; while (reqpath[reqpathc]) reqpathc++; }
  if (!reqpathc) {
    reqpath="/";
    reqpathc=1;
  }
  
  /* If it doesn't start with slash, or contains a double-dot entry, reject it.
   */
  if (reqpath[0]!='/') return -1;
  int p=1;
  while (p<reqpathc) {
    if (reqpath[p]=='/') { p++; continue; }
    const char *elem=reqpath+p;
    int elemc=0;
    while ((p<reqpathc)&&(reqpath[p++]!='/')) elemc++;
    if ((elemc==2)&&!memcmp(elem,"..",2)) return -1;
  }
  
  paths->reqpath=reqpath;
  paths->reqpathc=reqpathc;
  
  /* Search htdocs.
   */
  if (writing) {
    return eggdev_http_paths_find_htdocs_writing(paths);
  } else {
    return eggdev_http_paths_find_htdocs_reading(paths);
  }
}
