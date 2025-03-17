/* render.h
 * Egg's default renderer.
 * We handle the OpenGL stuff and expose an interface similar to the Platform API.
 * Setting up the OpenGL context is the video driver's problem.
 */
 
#ifndef RENDER_H
#define RENDER_H

struct render;
struct egg_render_uniform;

void render_del(struct render *render);
struct render *render_new();

void render_begin(struct render *render);
void render_commit(struct render *render);

int render_set_framebuffer_size(struct render *render,int fbw,int fbh);
void render_set_size(struct render *render,int winw,int winh);
void render_coords_win_from_fb(struct render *render,int *x,int *y);
void render_coords_fb_from_win(struct render *render,int *x,int *y);

void render_texture_del(struct render *render,int texid);
int render_texture_new(struct render *render);
void render_texture_get_size(int *w,int *h,struct render *render,int texid);

int render_texture_load_raw(struct render *render,int texid,int w,int h,int stride,const void *src,int srcc);

int render_texture_get_pixels(void *dst,int dsta,struct render *render,int texid);

void render_texture_clear(struct render *render,int texid);

void render_render(struct render *render,const struct egg_render_uniform *uniform,const void *vtxv,int vtxc);

#endif
