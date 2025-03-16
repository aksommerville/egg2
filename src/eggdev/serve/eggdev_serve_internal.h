#ifndef EGGDEV_SERVE_INTERNAL_H
#define EGGDEV_SERVE_INTERNAL_H

#include "eggdev/eggdev_internal.h"
#include "opt/http/http.h"

/* Path resolution.
 *******************************************************************************/

struct eggdev_http_paths {

  /* Full local path.
   * Typically the request path, minus matching prefix, plus htdocs root.
   * If the htdocs root was a Zip file, this is only the Zip file itself.
   */
  char *local;
  int localc;
  
  /* When not (writing), we verify the file's existence before returning.
   * If (writing), this is always zero.
   * Otherwise it's [fdcb?] and you probably want to ignore anything but 'f'.
   */
  char ftype;
  
  /* Not (writing) and the htdocs root is a Zip, we put the remainder of the path here.
   * This is the portion you need to extract from the Zip file.
   * No leading slash.
   */
  char *inner;
  int innerc;
  
  /* Portion of the request path that matched our htdocs.
   * Points into (g.htdocsv).
   */
  const char *reqpfx;
  int reqpfxc;
  
  /* Portion of the local path that came from htdocs.
   * Points into (g.htdocsv).
   */
  const char *localpfx;
  int localpfxc;
  
  /* The input you provided, just in case.
   * When we get empty input, we substitute "/", and that IS reflected here.
   */
  const char *reqpath;
  int reqpathc;
};

void eggdev_http_paths_cleanup(struct eggdev_http_paths *paths);

int eggdev_http_paths_resolve(
  struct eggdev_http_paths *paths,
  const char *reqpath,int reqpathc,
  int writing
);

#endif
