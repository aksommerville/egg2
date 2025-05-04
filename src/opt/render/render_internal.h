#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

#include "render.h"
#include "egg/egg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <GLES2/gl2.h>

#define RENDER_FB_LIMIT 4096
#define RENDER_WIN_LIMIT 4096

struct render_texture {
  int texid; // As exposed to clients.
  int w,h;
  int gltexid;
  int fbid; // Zero if read-only.
  int border; // Extra border on all sides to dodge MacOS point-sprite culling problems. Not included in (w,h).
};

struct render {
  int fbw,fbh,winw,winh;
  int dstdirty;
  int dstx,dsty,dstw,dsth; // Framebuffer position in window space.
  
  struct render_texture *texturev;
  int texturec,texturea;
  int texid_next;
  
  // For downloading bordered textures, or other transient use.
  void *scratch;
  int scratcha;
};

// Populates (dstx,dsty,dstw,dsth) if needed.
void render_require_projection(struct render *render);

int render_scratch_require(struct render *render,int c);

int render_texturev_search(const struct render *render,int texid);
struct render_texture *render_texturev_insert(struct render *render,int p,int texid);
void render_texturev_remove(struct render *render,int p,int c);
int render_texture_require_fb(struct render *render,struct render_texture *texture);

#endif
