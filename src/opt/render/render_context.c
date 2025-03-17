#include "render_internal.h"

/* Delete.
 */
 
void render_del(struct render *render) {
  if (!render) return;
  free(render);
}

/* New.
 */
 
struct render *render_new() {
  struct render *render=calloc(1,sizeof(struct render));
  if (!render) return 0;
  return render;
}

/* Begin frame.
 */

void render_begin(struct render *render) {
  //TODO
}

/* End frame, and draw the main.
 */
 
void render_commit(struct render *render) {
  //TODO
}

/* Final projection.
 */
static int _fbw=0,_fbh=0;//XXX
 
int render_set_framebuffer_size(struct render *render,int fbw,int fbh) {
  //TODO
  _fbw=fbw;_fbh=fbh;//XXX
  return 0;
}

void render_set_size(struct render *render,int winw,int winh) {
  //TODO
}

void render_coords_win_from_fb(struct render *render,int *x,int *y) {
  //TODO
}

void render_coords_fb_from_win(struct render *render,int *x,int *y) {
  //TODO
}

/* Texture objects.
 */

void render_texture_del(struct render *render,int texid) {
  //TODO
}

int render_texture_new(struct render *render) {
  //TODO
  return -1;
}

void render_texture_get_size(int *w,int *h,struct render *render,int texid) {
  //TODO
  *w=_fbw;*h=_fbh;//XXX just so our demo doesn't complain, until we get the real thing running
}

int render_texture_load_raw(struct render *render,int texid,int w,int h,int stride,const void *src,int srcc) {
  //TODO
  return -1;
}

int render_texture_get_pixels(void *dst,int dsta,struct render *render,int texid) {
  //TODO
  return -1;
}

void render_texture_clear(struct render *render,int texid) {
  //TODO
}

/* Render.
 */

void render_render(struct render *render,const struct egg_render_uniform *uniform,const void *vtxv,int vtxc) {
  //TODO
}
