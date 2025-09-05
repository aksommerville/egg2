#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

#include "render.h"
#include "egg/egg.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#if USE_ismac
  #include <OpenGL/OpenGL.h>
  #define GL_GLEXT_PROTOTYPES 1
  #include <OpenGL/gl.h>
#else
  #include <GLES2/gl2.h>
#endif

//TODO EGG_GLSL_VERSION: Document, and set appropriate defaults via config.mk.
#ifndef EGG_GLSL_VERSION
  #if USE_macos
    #define EGG_GLSL_VERSION 120
  #else
    #define EGG_GLSL_VERSION 100
  #endif
#endif

#define RENDER_FB_LIMIT 4096
#define RENDER_WIN_LIMIT 4096

/* Our programs don't align exactly to the Egg render modes, but pretty close.
 * All programs respect uniform (tint,alpha).
 * RAW and TEX are both suitable for the Egg modes POINTS, LINES, LINE_STRIP, TRIANGLES, TRIANGLE_STRIP.
 */
#define RENDER_PROGRAM_RAW   0 /* egg_render_raw, no texture */
#define RENDER_PROGRAM_TEX   1 /* egg_render_raw, texture */
#define RENDER_PROGRAM_TILE  2 /* egg_render_tile, texture */
#define RENDER_PROGRAM_FANCY 3 /* egg_render_fancy, texture */
#define RENDER_PROGRAM_COUNT 4

struct render_texture {
  int texid; // As exposed to clients.
  int w,h;
  int gltexid;
  int fbid; // Zero if read-only.
  int border; // Extra border on all sides to dodge MacOS point-sprite culling problems. Not included in (w,h).
};

struct render_program {
  int programid;
  int u_screensize; // vec2
  int u_srcsize; // vec2, input texture
  int u_dstborder; // float, output texture
  int u_srcborder; // float, input texture
  int u_tint; // vec4
  int u_alpha; // float
  int u_sampler; // int
  const char *name; // static
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
  
  struct render_program programv[RENDER_PROGRAM_COUNT];
  
  // Current selected textures and programs (GL IDs).
  int current_dsttexid,current_srctexid,current_programid;
};

// Populates (dstx,dsty,dstw,dsth) if needed.
void render_require_projection(struct render *render);

int render_scratch_require(struct render *render,int c);

int render_texturev_search(const struct render *render,int texid);
struct render_texture *render_texturev_insert(struct render *render,int p,int texid);
void render_texturev_remove(struct render *render,int p,int c);
int render_texture_require_fb(struct render *render,struct render_texture *texture);

/* Start targetting (texture), null for the main.
 */
int render_to_texture(struct render *render,struct render_texture *texture);

void render_program_cleanup(struct render *render,struct render_program *program);
int render_programs_init(struct render *render);

#endif
