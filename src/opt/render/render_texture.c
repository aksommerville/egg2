#include "render_internal.h"

/* Border?
 */
 
static int render_texture_border_size(struct render *render) {
  //TODO Let texture borders be configurable by the user. Maybe they are needed in places other than MacOS.
  //TODO For MacOS, try to guess the tilesize, and border must be more than half of that.
  #if USE_macos
    return 16;
  #endif
  return 0;
}

/* Cleanup.
 */
 
static void render_texture_cleanup(struct render *render,struct render_texture *texture) {
  glDeleteTextures(1,(GLuint*)&texture->gltexid);
  if (texture->fbid) {
    glDeleteFramebuffers(1,(GLuint*)&texture->fbid);
  }
}

/* Drop framebuffer if there is one.
 */
 
static void render_texture_drop_fb(struct render *render,struct render_texture *texture) {
  if (!texture->fbid) return;
  glDeleteFramebuffers(1,(GLuint*)&texture->fbid);
  texture->fbid=0;
}

/* Create framebuffer if there isn't one.
 */
 
int render_texture_require_fb(struct render *render,struct render_texture *texture) {
  if (texture->fbid) return 0;
  glGenFramebuffers(1,&texture->fbid);
  if (!texture->fbid) {
    glGenFramebuffers(1,&texture->fbid);
    if (!texture->fbid) return -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture->gltexid,0);
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  return 0;
}

/* Texture list.
 */
 
int render_texturev_search(const struct render *render,int texid) {
  int lo=0,hi=render->texturec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct render_texture *q=render->texturev+ck;
         if (texid<q->texid) hi=ck;
    else if (texid>q->texid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

void render_texturev_remove(struct render *render,int p,int c) {
  if ((p<0)||(c<1)||(p>render->texturec-c)) return;
  struct render_texture *texture=render->texturev+p;
  int i=c;
  for (;i-->0;texture++) render_texture_cleanup(render,texture);
  render->texturec-=c;
  memmove(render->texturev+p,render->texturev+p+c,sizeof(struct render_texture)*(render->texturec-p));
}

/* New texture.
 */

struct render_texture *render_texturev_insert(struct render *render,int p,int texid) {
  if ((p<0)||(p>render->texturec)) return 0;
  if (p&&(texid<=render->texturev[p-1].texid)) return 0;
  if ((p<render->texturec)&&(texid>=render->texturev[p].texid)) return 0;
  if (render->texturec>=render->texturea) {
    int na=render->texturea+16;
    if (na>INT_MAX/sizeof(struct render_texture)) return 0;
    void *nv=realloc(render->texturev,sizeof(struct render_texture)*na);
    if (!nv) return 0;
    render->texturev=nv;
    render->texturea=na;
  }
  
  // Create the OpenGL texture.
  GLuint gltexid=0;
  glGenTextures(1,&gltexid);
  if (!gltexid) {
    glGenTextures(1,&gltexid);
    if (!gltexid) return 0;
  }
  glBindTexture(GL_TEXTURE_2D,gltexid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  
  // Create our wrapper object.
  struct render_texture *texture=render->texturev+p;
  memmove(texture+1,texture,sizeof(struct render_texture)*(render->texturec-p));
  render->texturec++;
  memset(texture,0,sizeof(struct render_texture));
  texture->texid=texid;
  texture->gltexid=gltexid;
  if (texid==1) texture->border=render_texture_border_size(render);
  return texture;
}

/* Texture objects.
 */

void render_texture_del(struct render *render,int texid) {
  if (texid<=1) return; // Not allowed to delete texture 1, and <=0 are illegal.
  int p=render_texturev_search(render,texid);
  if (p<0) return;
  render_texturev_remove(render,p,1);
}

int render_texture_new(struct render *render) {
  if (!render||(render->texid_next<=1)||(render->texid_next>=INT_MAX)) return -1;
  int texid=render->texid_next++;
  struct render_texture *texture=render_texturev_insert(render,render->texturec,texid);
  if (!texture) return -1;
  return texid;
}

void render_texture_get_size(int *w,int *h,struct render *render,int texid) {
  int p=render_texturev_search(render,texid);
  if (p<0) return;
  const struct render_texture *texture=render->texturev+p;
  if (w) *w=texture->w;
  if (h) *h=texture->h;
}

/* Copy pixels in memory.
 */

static void render_image_copy(uint8_t *dst,int dststride,const uint8_t *src,int srcstride,int w,int h) {
  int cpc=w<<2;
  for (;h-->0;dst+=dststride,src+=srcstride) memcpy(dst,src,cpc);
}

/* Upload texture, post-validation.
 * (src) may be null.
 */
 
static int render_texture_upload(struct render *render,struct render_texture *texture,int w,int h,const void *src) {
  glBindTexture(GL_TEXTURE_2D,texture->gltexid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,src);
  texture->w=w;
  texture->h=h;
  return 0;
}

/* Load texture.
 */

int render_texture_load_raw(struct render *render,int texid,int w,int h,int stride,const void *src,int srcc) {
  if (!srcc) src=0;
  if ((w<1)||(w>RENDER_FB_LIMIT)) return -1;
  if ((h<1)||(h>RENDER_FB_LIMIT)) return -1;
  int p=render_texturev_search(render,texid);
  if (p<0) return -1;
  struct render_texture *texture=render->texturev+p;
  
  /* Special handling for texture 1:
   *  - Null source only allowed on the first call (when texture->w,h zero).
   *  - Dimensions must match.
   *  - (src) must be provided.
   *  - Don't drop the framebuffer.
   *  - Record dimensions in the context.
   */
  if (texid==1) {
    if (!src) {
      if (texture->w||texture->h) return -1;
      if (render_texture_upload(render,texture,w+texture->border*2,h+texture->border*2,0)<0) return -1;
      texture->w=w;
      texture->h=h;
    } else {
      if ((w!=texture->w)||(h!=texture->h)) return -1;
      int minstride=w<<2;
      if (stride<1) stride=minstride;
      else if (stride<minstride) return -1;
      if (srcc<stride*h) return -1;
      int err;
      if (stride!=minstride) {
        if (render_scratch_require(render,minstride*h)<0) return -1;
        render_image_copy(render->scratch,minstride,src,stride,w,h);
        err=render_texture_upload(render,texture,w,h,render->scratch);
      } else {
        err=render_texture_upload(render,texture,w,h,src);
      }
      if (err<0) return err;
      texture->border=0;
    }
    render->fbw=texture->w;
    render->fbh=texture->h;
    render->dstdirty=1;
    return 0;
  }
  
  /* If (src) is provided:
   *  - No border.
   *  - Validate or default (stride).
   *  - (srcc) required and must be asserted.
   *  - Drop framebuffer if present.
   *  - Reallocate at minimum stride if it isn't already. Easier than dealing with OpenGL's packing parameters.
   *  - Upload.
   */
  if (src) {
    int minstride=w<<2;
    if (stride<1) stride=minstride;
    else if (stride<minstride) return -1;
    if (srcc<stride*h) return -1;
    render_texture_drop_fb(render,texture);
    int err;
    if (stride!=minstride) {
      if (render_scratch_require(render,minstride*h)<0) return -1;
      render_image_copy(render->scratch,minstride,src,stride,w,h);
      err=render_texture_upload(render,texture,w,h,render->scratch);
    } else {
      err=render_texture_upload(render,texture,w,h,src);
    }
    if (err<0) return err;
    texture->border=0;
    
  /* If (src) null:
   *  - (srcc) must be zero.
   *  - Use a border if we need to.
   *  - Upload null. OpenGL allows this, to just allocate the space.
   *  - One expects there will be a framebuffer. But we don't need to create it just yet.
   */
  } else {
    render_texture_drop_fb(render,texture);
    texture->border=render_texture_border_size(render);
    if (render_texture_upload(render,texture,w+texture->border*2,h+texture->border*2,0)<0) return -1;
    texture->w=w;
    texture->h=h;
  }

  return 0;
}

/* Read pixels off texture.
 */

int render_texture_get_pixels(void *dst,int dsta,struct render *render,int texid) {
  int p=render_texturev_search(render,texid);
  if (p<0) return -1;
  struct render_texture *texture=render->texturev+p;
  int stride=texture->w<<2;
  int dstc=stride*texture->h;
  if (dstc>dsta) return -1;

  if (render_texture_require_fb(render,texture)<0) return -1;
  glFlush();
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glReadPixels(texture->border,texture->border,texture->w,texture->h,GL_RGBA,GL_UNSIGNED_BYTE,dst);

  return dstc;
}

/* Clear texture.
 * Arguably a "render" op as opposed to a "texture" one.
 */

void render_texture_clear(struct render *render,int texid) {
  int p=render_texturev_search(render,texid);
  if (p<0) return;
  struct render_texture *texture=render->texturev+p;
  if (render_texture_require_fb(render,texture)<0) return;
  glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
  glViewport(0,0,texture->w+texture->border*2,texture->h+texture->border*2);
  glClearColor(0.0f,0.0f,0.0f,0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

/* Set current target.
 */
 
int render_to_texture(struct render *render,struct render_texture *texture) {
  if (texture) {
    glBindFramebuffer(GL_FRAMEBUFFER,texture->fbid);
    glViewport(0,0,texture->w+texture->border*2,texture->h+texture->border*2);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glViewport(0,0,render->winw,render->winh);
  }
  return 0;
}
