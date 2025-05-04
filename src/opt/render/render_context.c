#include "render_internal.h"

/* Delete.
 */
 
void render_del(struct render *render) {
  if (!render) return;
  if (render->texturev) {
    render_texturev_remove(render,0,render->texturec);
    free(render->texturev);
  }
  if (render->scratch) free(render->scratch);
  free(render);
}

/* New.
 */
 
struct render *render_new() {
  struct render *render=calloc(1,sizeof(struct render));
  if (!render) return 0;
  
  // Don't let either dimensions be zero.
  render->fbw=1;
  render->fbh=1;
  render->winw=1;
  render->winh=1;
  render->dstdirty=1;
  
  // Reserve texid 1 for the framebuffer.
  render->texid_next=2;
  struct render_texture *fb=render_texturev_insert(render,0,1);
  if (!fb) {
    render_del(render);
    return 0;
  }
  
  return render;
}

/* Scratch buffer.
 */
 
int render_scratch_require(struct render *render,int c) {
  if (c<=render->scratcha) return 0;
  void *nv=realloc(render->scratch,c);
  if (!nv) return -1;
  render->scratch=nv;
  render->scratcha=c;
  return 0;
}

/* Begin frame.
 */

void render_begin(struct render *render) {
  //TODO
}

/* End frame, and draw the main.
 */
 
void render_commit(struct render *render) {
  render_require_projection(render);
  
  /* If the framebuffer doesn't fill the output, black out.
   */
  if ((render->dstx>0)||(render->dsty>0)) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  
  //TODO Draw a textured quad, texture 1.
}

/* Final projection.
 */
 
int render_set_framebuffer_size(struct render *render,int fbw,int fbh) {
  return render_texture_load_raw(render,1,fbw,fbh,0,0,0);
}

void render_set_size(struct render *render,int winw,int winh) {
  if ((winw==render->winw)&&(winh==render->winh)) return;
  if ((winw<1)||(winw>RENDER_WIN_LIMIT)) return;
  if ((winh<1)||(winh>RENDER_WIN_LIMIT)) return;
  render->winw=winw;
  render->winh=winh;
  render->dstdirty=1;
}

void render_require_projection(struct render *render) {
  if (!render->dstdirty) return;
  render->dstdirty=0;
  
  int xscale=render->winw/render->fbw;
  int yscale=render->winh/render->fbh;
  int scale=(xscale<yscale)?xscale:yscale;
  if (scale<1) scale=1;
  
  //TODO Should we enable interpolation and scale down, when the window is smaller than the framebuffer?
  //TODO Should we scale up at fractional rates above a certain scale (fourish)?
  
  render->dstw=render->fbw*scale;
  render->dsth=render->fbh*scale;
  render->dstx=(render->winw>>1)-(render->dstw>>1);
  render->dsty=(render->winh>>1)-(render->dsth>>1);
}

void render_coords_win_from_fb(struct render *render,int *x,int *y) {
  render_require_projection(render);
  *x=((*x)*render->dstw)/render->fbw+render->dstx;
  *y=((*y)*render->dsth)/render->fbh+render->dsty;
}

void render_coords_fb_from_win(struct render *render,int *x,int *y) {
  render_require_projection(render);
  *x=(((*x)-render->dstx)*render->fbw)/render->dstw;
  *y=(((*y)-render->dsty)*render->fbh)/render->dsth;
}
