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
  struct render_program *program=render->programv;
  int i=RENDER_PROGRAM_COUNT;
  for (;i-->0;program++) render_program_cleanup(render,program);
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

  render->scale=1.0;
  
  // Reserve texid 1 for the framebuffer.
  render->texid_next=2;
  struct render_texture *fb=render_texturev_insert(render,0,1);
  if (!fb) {
    render_del(render);
    return 0;
  }
  
  if (render_programs_init(render)<0) {
    render_del(render);
    return 0;
  }
  
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glEnable(0x8642); // GL_PROGRAM_POINT_SIZE, on my Mac it's required but not declared.
  glEnable(GL_BLEND);
  #ifdef GL_POINT_SPRITE
    glEnable(GL_POINT_SPRITE);
  #endif
  
  return render;
}

/* Trivial accessors.
 */

void render_set_scale(struct render *render,double scale) {
  if (!render) return;
  render->scale=scale;
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
  render->current_dsttexid=0;
  render->current_srctexid=0;
  render->current_programid=0;
  glEnable(GL_BLEND);
}

/* End frame, and draw the main.
 */
 
void render_commit(struct render *render) {
  render_require_projection(render);
  render_to_texture(render,0);
  
  /* If the framebuffer doesn't fill the output, black out.
   */
  if ((render->dstx>0)||(render->dsty>0)) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  
  /* Render with the public API.
   */
  if ((render->texturec<1)||(render->texturev[0].texid!=1)) return;
  int srcw=render->texturev[0].w;
  int srch=render->texturev[0].h;
  struct egg_render_uniform uniform={
    .mode=EGG_RENDER_TRIANGLE_STRIP,
    .dsttexid=0,
    .srctexid=1,
    .tint=0,
    .alpha=0xff,
  };
  struct egg_render_raw vtxv[]={
    {render->dstx             ,render->dsty             ,0   ,srch},
    {render->dstx             ,render->dsty+render->dsth,0   ,0   },
    {render->dstx+render->dstw,render->dsty             ,srcw,srch},
    {render->dstx+render->dstw,render->dsty+render->dsth,srcw,0   },
  };
  glDisable(GL_BLEND);
  render_render(render,&uniform,vtxv,sizeof(vtxv));
  glEnable(GL_BLEND);
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
  
  // If either axis is too small, fit it. We use linear min filter, so just keep the aspect reasonably close and fill one axis.
  // Likewise, if it's at least 4x, allow that it needn't be an integer scale-up.
  if ((scale<1)||(scale>=4)) {
    int wforh=(render->fbw*render->winh)/render->fbh;
    if (wforh<=render->winw) {
      render->dstw=wforh;
      render->dsth=render->winh;
    } else {
      render->dstw=render->winw;
      render->dsth=(render->fbh*render->winw)/render->fbw;
    }
  
  // Output at least as large as framebuffer, and less than 4x, use an integer multiple of the framebuffer size.
  } else {
    render->dstw=render->fbw*scale;
    render->dsth=render->fbh*scale;
  }
  
  // And however we scaled it, center in the output.
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
