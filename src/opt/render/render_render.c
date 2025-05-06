#include "render_internal.h"

/* GLSL.
 */
 
static const char render_vshader_RAW[]=
  "uniform vec2 uscreensize;\n"
  "uniform vec2 usrcsize;\n"
  "uniform float udstborder;\n"
  "uniform float usrcborder;\n"
  "uniform vec4 utint;\n"
  "uniform float ualpha;\n"
  "attribute vec2 apos;\n"
  "attribute vec4 acolor;\n"
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "vec2 npos=vec2(\n"
      "((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,\n"
      "((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0\n"
    ");\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vcolor=vec4(mix(acolor.rgb,utint.rgb,utint.a),acolor.a*ualpha);\n"
  "}\n"
"";

static const char render_fshader_RAW[]=
  "varying vec4 vcolor;\n"
  "void main() {\n"
    "gl_FragColor=vcolor;\n"
  "}\n"
"";

static const char render_vshader_TEX[]=
  "uniform vec2 uscreensize;\n"
  "uniform vec2 usrcsize;\n"
  "uniform float udstborder;\n"
  "uniform float usrcborder;\n"
  "uniform vec4 utint;\n"
  "uniform float ualpha;\n"
  "attribute vec2 apos;\n"
  "attribute vec2 atexcoord;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "vec2 npos=vec2(\n"
      "((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,\n"
      "((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0\n"
    ");\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    "vtexcoord=atexcoord/usrcsize;\n"
  "}\n"
"";

static const char render_fshader_TEX[]=
  "uniform sampler2D usampler;\n"
  "uniform vec4 utint;\n"
  "uniform float ualpha;\n"
  "varying vec2 vtexcoord;\n"
  "void main() {\n"
    "vec4 tcolor=texture2D(usampler,vtexcoord);\n"
    "gl_FragColor=vec4(mix(tcolor.rgb,utint.rgb,utint.a),tcolor.a*ualpha);\n"
  "}\n"
"";

static const char render_vshader_TILE[]=
  "uniform vec2 uscreensize;\n"
  "uniform vec2 usrcsize;\n"
  "uniform float udstborder;\n"
  "uniform float usrcborder;\n"
  "uniform vec4 utint;\n"
  "uniform float ualpha;\n"
  "attribute vec2 apos;\n"
  "attribute float atileid;\n"
  "attribute float axform;\n"
  "void main() {\n"
    "vec2 npos=vec2(\n"
      "((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,\n"
      "((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0\n"
    ");\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    //TODO
  "}\n"
"";

static const char render_fshader_TILE[]=
  "void main() {\n"
    "gl_FragColor=vec4(0.0,0.0,0.0,1.0);\n"//TODO
  "}\n"
"";

static const char render_vshader_FANCY[]=
  "uniform vec2 uscreensize;\n"
  "uniform vec2 usrcsize;\n"
  "uniform float udstborder;\n"
  "uniform float usrcborder;\n"
  "uniform vec4 utint;\n"
  "uniform float ualpha;\n"
  "attribute vec2 apos;\n"
  "attribute float atileid;\n"
  "attribute float arotation;\n"
  "attribute float asize;\n"
  "attribute vec4 atint;\n"
  "attribute vec4 aprimary;\n"
  "void main() {\n"
    "vec2 npos=vec2(\n"
      "((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,\n"
      "((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0\n"
    ");\n"
    "gl_Position=vec4(npos,0.0,1.0);\n"
    //TODO
  "}\n"
"";

static const char render_fshader_FANCY[]=
  "void main() {\n"
    "gl_FragColor=vec4(0.0,0.0,0.0,1.0);\n"//TODO
  "}\n"
"";

/* Cleanup program.
 */
 
void render_program_cleanup(struct render *render,struct render_program *program) {
  if (program->programid) {
    glDeleteProgram(program->programid);
  }
}

/* Compile half of one program.
 * <0 for error.
 */
 
static int render_program_compile(struct render *render,const char *name,int pid,int type,const GLchar *src,GLint srcc) {
  GLint sid=glCreateShader(type);
  if (!sid) return -1;

  GLchar preamble[256];
  GLint preamblec=0;
  #define VERBATIM(v) { memcpy(preamble+preamblec,v,sizeof(v)-1); preamblec+=sizeof(v)-1; }
  VERBATIM("#version ")
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION/100)%10;
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION/10 )%10;
  preamble[preamblec++]='0'+(EGG_GLSL_VERSION    )%10;
  #if USE_macos
    VERBATIM("\n")
  #else
    VERBATIM("\nprecision mediump float;\n")
  #endif
  #undef VERBATIM
  const GLchar *srcv[2]={preamble,src};
  GLint srccv[2]={preamblec,srcc};
  glShaderSource(sid,2,srcv,srccv);
  
  glCompileShader(sid);
  GLint status=0;
  glGetShaderiv(sid,GL_COMPILE_STATUS,&status);
  if (status) {
    glAttachShader(pid,sid);
    glDeleteShader(sid);
    return 0;
  }
  GLuint loga=0,logged=0;
  glGetShaderiv(sid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetShaderInfoLog(sid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to compile %s shader for program '%s':\n%.*s\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to compile %s shader for program '%s', no further detail available.\n",(type==GL_VERTEX_SHADER)?"vertex":"fragment",name);
  glDeleteShader(sid);
  return -1;
}

/* Initialize one program.
 */
 
static int render_program_init(
  struct render_program *program,
  struct render *render,
  const char *vsrc,int vsrcc,
  const char *fsrc,int fsrcc,
  const char *name,
  const char **attrnamev
) {
  program->name=name;
  if ((program->programid=glCreateProgram())<1) {
    if ((program->programid=glCreateProgram())<1) return -1;
  }
  
  if (render_program_compile(render,name,program->programid,GL_VERTEX_SHADER,vsrc,vsrcc)<0) return -1;
  if (render_program_compile(render,name,program->programid,GL_FRAGMENT_SHADER,fsrc,fsrcc)<0) return -1;
  if (attrnamev) {
    int attrix=0;
    for (;attrnamev[attrix];attrix++) {
      glBindAttribLocation(program->programid,attrix,attrnamev[attrix]);
    }
  }
  
  glLinkProgram(program->programid);
  GLint status=0;
  glGetProgramiv(program->programid,GL_LINK_STATUS,&status);
  if (status) {
    program->u_screensize=glGetUniformLocation(program->programid,"uscreensize");
    program->u_srcsize=glGetUniformLocation(program->programid,"usrcsize");
    program->u_dstborder=glGetUniformLocation(program->programid,"udstborder");
    program->u_srcborder=glGetUniformLocation(program->programid,"usrcborder");
    program->u_tint=glGetUniformLocation(program->programid,"utint");
    program->u_alpha=glGetUniformLocation(program->programid,"ualpha");
    program->u_sampler=glGetUniformLocation(program->programid,"usampler");
    return 0;
  }
  
  GLuint loga=0,logged=0;
  glGetProgramiv(program->programid,GL_INFO_LOG_LENGTH,&loga);
  if (loga>0) {
    GLchar *log=malloc(loga+1);
    if (log) {
      GLsizei logc=0;
      glGetProgramInfoLog(program->programid,loga,&logc,log);
      if ((logc>0)&&(logc<=loga)) {
        while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
        fprintf(stderr,"Failed to link GLSL program '%s':\n%.*s\n",name,logc,log);
        logged=1;
      }
      free(log);
    }
  }
  if (!logged) fprintf(stderr,"Failed to link GLSL program '%s', no further detail available.\n",name);
  return -1;
}

/* Initialize programs.
 */
 
int render_programs_init(struct render *render) {
  #define INIT1(tag,...) { \
    const char *attrnamev[]={__VA_ARGS__,0}; \
    if (render_program_init( \
      render->programv+RENDER_PROGRAM_##tag, \
      render, \
      render_vshader_##tag,sizeof(render_vshader_##tag)-1, \
      render_fshader_##tag,sizeof(render_fshader_##tag)-1, \
      #tag, \
      attrnamev \
    )<0) return -1; \
  }
  INIT1(RAW,"apos","acolor")
  INIT1(TEX,"apos","atexcoord")
  INIT1(TILE,"apos","atileid","axform")
  INIT1(FANCY,"apos","atileid","arotation","asize","atint","aprimary")
  #undef INIT1
  return 0;
}

/* Render.
 */

void render_render(struct render *render,const struct egg_render_uniform *uniform,const void *vtxv,int vtxc) {
  if (!uniform||!vtxv) return;
  
  /* Select program and finalize vertex count.
   */
  struct render_program *program=0;
  int glmode=GL_POINTS;
  switch (uniform->mode) {
    case EGG_RENDER_POINTS: glmode=GL_POINTS; goto _raw_or_tex_;
    case EGG_RENDER_LINES: glmode=GL_LINES; goto _raw_or_tex_;
    case EGG_RENDER_LINE_STRIP: glmode=GL_LINE_STRIP; goto _raw_or_tex_;
    case EGG_RENDER_TRIANGLES: glmode=GL_TRIANGLES; goto _raw_or_tex_;
    case EGG_RENDER_TRIANGLE_STRIP: glmode=GL_TRIANGLE_STRIP; goto _raw_or_tex_;
      _raw_or_tex_: {
        if (uniform->srctexid) {
          program=render->programv+RENDER_PROGRAM_TEX;
        } else {
          program=render->programv+RENDER_PROGRAM_RAW;
        }
        vtxc/=sizeof(struct egg_render_raw);
      } break;
    case EGG_RENDER_TILE: {
        if (!uniform->srctexid) return;
        program=render->programv+RENDER_PROGRAM_TILE;
        vtxc/=sizeof(struct egg_render_tile);
      } break;
    case EGG_RENDER_FANCY: {
        if (!uniform->srctexid) return;
        program=render->programv+RENDER_PROGRAM_FANCY;
        vtxc/=sizeof(struct egg_render_tile);
      } break;
  }
  if (!program||(vtxc<1)) return;
  
  /* If a source texture is requested, acquire it.
   */
  struct render_texture *srctex=0;
  if (uniform->srctexid) {
    int p=render_texturev_search(render,uniform->srctexid);
    if (p<0) return;
    srctex=render->texturev+p;
  }
  
  /* Acquire destination texture and ensure it can accept output.
   */
  struct render_texture *dsttex=0;
  if (uniform->dsttexid) {
    int p=render_texturev_search(render,uniform->dsttexid?uniform->dsttexid:1);
    if (p<0) return;
    dsttex=render->texturev+p;
    if (render_texture_require_fb(render,dsttex)<0) return;
  }
  
  /* Bind to the output texture if it's not currently bound.
   */
  GLfloat screenw,screenh;
  if (dsttex) {
    if (dsttex->gltexid!=render->current_dsttexid) {
      glBindFramebuffer(GL_FRAMEBUFFER,dsttex->fbid);
      glViewport(dsttex->border,dsttex->border,dsttex->w,dsttex->h);
      render->current_dsttexid=dsttex->gltexid;
    }
    //TODO does this need to include the border?
    screenw=dsttex->w;
    screenh=dsttex->h;
  } else {
    if (render->current_dsttexid) {
      glBindFramebuffer(GL_FRAMEBUFFER,0);
      glViewport(0,0,render->winw,render->winh);
      render->current_dsttexid=0;
    }
    screenw=render->winw;
    screenh=render->winh;
  }
  
  /* Bind to the program if it's not currently bound, and set uniforms.
   */
  if (program->programid!=render->current_programid) {
    glUseProgram(program->programid);
    render->current_programid=program->programid;
  }
  glUniform2f(program->u_screensize,screenw,screenh);
  if (dsttex) {
    glUniform1f(program->u_dstborder,dsttex->border);
  } else {
    glUniform1f(program->u_dstborder,0.0f);
  }
  if (srctex) {
    glUniform2f(program->u_srcsize,srctex->w,srctex->h);//TODO border?
    glUniform1f(program->u_srcborder,srctex->border);
    glUniform1i(program->u_sampler,0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,srctex->gltexid);
  }
  if (uniform->tint) {
    const uint8_t r=uniform->tint>>24,g=uniform->tint>>16,b=uniform->tint>>8,a=uniform->tint;
    glUniform4f(program->u_tint,r/255.0f,g/255.0f,b/255.0f,a/255.0f);
  } else {
    glUniform4f(program->u_tint,0.0f,0.0f,0.0f,0.0f);
  }
  glUniform1f(program->u_alpha,uniform->alpha/255.0f);
  
  /* Prepare vertex pointers, and do it.
   */
  switch ((int)(program-render->programv)) {
    case RENDER_PROGRAM_RAW: {
        const struct egg_render_raw *V=vtxv;
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_render_raw),&V->x);
        glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,1,sizeof(struct egg_render_raw),&V->r);
        glDrawArrays(glmode,0,vtxc);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
      } break;
      
    case RENDER_PROGRAM_TEX: {
        const struct egg_render_raw *V=vtxv;
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_render_raw),&V->x);
        glVertexAttribPointer(1,2,GL_SHORT,0,sizeof(struct egg_render_raw),&V->tx);
        glDrawArrays(glmode,0,vtxc);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
      } break;
      
    case RENDER_PROGRAM_TILE: {
        const struct egg_render_tile *V=vtxv;
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_render_tile),&V->x);
        glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_render_tile),&V->tileid);
        glVertexAttribPointer(2,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_render_tile),&V->xform);
        glDrawArrays(GL_POINTS,0,vtxc);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
      } break;
      
    case RENDER_PROGRAM_FANCY: {
        const struct egg_render_fancy *V=vtxv;
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(0,2,GL_SHORT,0,sizeof(struct egg_render_fancy),&V->x);
        glVertexAttribPointer(1,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_render_fancy),&V->tileid);
        glVertexAttribPointer(2,1,GL_UNSIGNED_BYTE,1,sizeof(struct egg_render_fancy),&V->rotation);
        glVertexAttribPointer(3,1,GL_UNSIGNED_BYTE,0,sizeof(struct egg_render_fancy),&V->size);
        glVertexAttribPointer(4,4,GL_UNSIGNED_BYTE,1,sizeof(struct egg_render_fancy),&V->tr);
        glVertexAttribPointer(5,4,GL_UNSIGNED_BYTE,1,sizeof(struct egg_render_fancy),&V->pr);
        glDrawArrays(GL_POINTS,0,vtxc);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
        glDisableVertexAttribArray(4);
        glDisableVertexAttribArray(5);
      } break;
  }
}
