/* Video.js
 * Manages rendering, texture loads.
 * We don't create the canvas (that's Runtime), but we do everything else that happens to it.
 */
 
const TEX_SIZE_LIMIT = 4096;
 
export class Video {
  constructor(rt) {
    this.rt = rt;
    this.texv = []; // {gltexid,border,w,h,fbid,fbready}. Sparse, indexed by texid.
    this.canvas = null;
    this.gl = null;
    this.buffer = null;
    this.vbuf = new Uint8Array(48).buffer; // struct egg_render raw: 12 bytes, 4 of them.
    this.vbufs16 = new Uint16Array(this.vbuf);
    this.tex_current = null;
  }
  
  start() {
    this.texv = [];
    this.canvas = document.getElementById("eggfb");
    if (this.canvas?.tagName !== "CANVAS") throw new Error(`Canvas not found.`);
    const [w, h] = (this.rt.rom.getMeta("fb") || "640x360").split("x").map(v => +v);
    if (isNaN(w) || (w < 1) || (w > TEX_SIZE_LIMIT) || isNaN(h) || (h < 1) || (h > TEX_SIZE_LIMIT)) {
      throw new Error(`Invalid framebuffer size.`);
    }
    this.canvas.width = w;
    this.canvas.height = h;
    this.gl = this.canvas.getContext("webgl");
    this.texv[1] = {
      gltexid: 0,
      border: 0,
      w, h,
      fbid: 0,
    };
    this.egg_texture_load_raw(1, w, h, 0, null);
    this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
    this.gl.enable(this.gl.BLEND);
    if (!(this.buffer = this.gl.createBuffer())) throw new Error(`Failed to create WebGL vertex buffer.`);
    this.compileShaders();
  }
  
  stop() {
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
  }
  
  beginFrame() {
    this.tex_current = null;
  }
  
  endFrame() {
    const srctex = this.texv[1];
    if (!srctex) return;

    const fbw=this.canvas.width, fbh=this.canvas.height;
    const sv = this.vbufs16;
    sv[ 0] = 0;   sv[ 1] = 0;   sv[ 2] = srctex.border; sv[ 3] = srctex.border + srctex.h;
    sv[ 6] = 0;   sv[ 7] = fbh; sv[ 8] = srctex.border; sv[ 9] = srctex.border;
    sv[12] = fbw; sv[13] = 0;   sv[14] = srctex.border + srctex.w; sv[15] = srctex.border + srctex.h;
    sv[18] = fbw; sv[19] = fbh; sv[20] = srctex.border + srctex.w; sv[21] = srctex.border;
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vbuf, this.gl.STREAM_DRAW);
    
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    this.gl.viewport(0, 0, fbw, fbh);
    this.gl.useProgram(this.pgm_tex);
    this.gl.uniform2f(this.u_tex.uscreensize, fbw, fbh);
    this.gl.uniform2f(this.u_tex.usrcsize, fbw + srctex.border * 2, fbh + srctex.border * 2);
    this.gl.uniform1f(this.u_tex.udstborder, 0);
    this.gl.uniform1f(this.u_tex.usrcborder, srctex.border);
    this.gl.uniform1i(this.u_tex.usampler, 0);
    this.gl.activeTexture(this.gl.TEXTURE0);
    this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.gltexid);
    this.gl.uniform4f(this.u_tex.utint, 0.0, 0.0, 0.0, 0.0);
    this.gl.uniform1f(this.u_tex.ualpha, 1.0);
    this.gl.disable(this.gl.BLEND);
    this.gl.enableVertexAttribArray(0);
    this.gl.enableVertexAttribArray(1);
    this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
    this.gl.vertexAttribPointer(1, 2, this.gl.SHORT, false, 12, 4);
    this.gl.drawArrays(this.gl.TRIANGLE_STRIP, 0, 4);
    this.gl.disableVertexAttribArray(0);
    this.gl.disableVertexAttribArray(1);
    this.gl.enable(this.gl.BLEND);
  }
  
  /* Shaders.
   *******************************************************************************************/
   
  compileShaders() {
    // Program and attribute names match the native GLES implementation exactly.
    this.pgm_raw = this.compileShader("raw", ["apos", "acolor"], ["uscreensize", "udstborder", "utint", "ualpha"]);
    this.pgm_tex = this.compileShader("tex", ["apos", "atexcoord"], ["uscreensize", "usrcsize", "udstborder", "usrcborder", "utint", "ualpha", "usampler"]);
    this.pgm_tile = this.compileShader("tile", ["apos", "atileid", "axform"], ["uscreensize", "usrcsize", "udstborder", "usrcborder", "utint", "ualpha", "usampler"]);
    this.pgm_fancy = this.compileShader("fancy", ["apos", "atileid", "axform", "arotation", "asize", "atint", "aprimary"], ["uscreensize", "usrcsize", "udstborder", "usrcborder", "utint", "ualpha", "usampler"]);
  }
  
  compileShader(name, aNames, uNames) {
    const pid = this.gl.createProgram();
    if (!pid) throw new Error(`Failed to create new WebGL program for ${JSON.stringify(name)}`);
    try {
      this.compileShader1(name, pid, this.gl.VERTEX_SHADER, Video.glsl[name + "_v"]);
      this.compileShader1(name, pid, this.gl.FRAGMENT_SHADER, Video.glsl[name + "_f"]);
      for (let i=0; i<aNames.length; i++) {
        this.gl.bindAttribLocation(pid, i, aNames[i]);
      }
      this.gl.linkProgram(pid);
      if (!this.gl.getProgramParameter(pid, this.gl.LINK_STATUS)) {
        const log = this.gl.getProgramInfoLog(pid);
        throw new Error(`Failed to link program ${JSON.stringify(name)}:\n${log}`);
      }
      this.gl.useProgram(pid);
      const udst = this["u_" + name] = {};
      for (const uName of uNames) {
        udst[uName] = this.gl.getUniformLocation(pid, uName);
      }
    } catch (e) {
      this.gl.deleteProgram(pid);
      throw e;
    }
    return pid;
  }
  
  compileShader1(name, pid, type, src) {
    const sid = this.gl.createShader(type);
    if (!sid) throw new Error(`Failed to create new WebGL shader for ${JSON.stringify(name)}`);
    try {
      this.gl.shaderSource(sid, src);
      this.gl.compileShader(sid);
      if (!this.gl.getShaderParameter(sid, this.gl.COMPILE_STATUS)) {
        const log = this.gl.getShaderInfoLog(sid);
        throw new Error(`Failed to link ${(type === this.gl.VERTEX_SHADER) ? "vertex" : "fragment"} shader for ${JSON.stringify(name)}:\n${log}`);
      }
      this.gl.attachShader(pid, sid);
    } finally {
      this.gl.deleteShader(sid);
    }
  }
  
  requireTexture(tex) {
    if (tex.gltexid) return 1;
    if (!(tex.gltexid = this.gl.createTexture())) return 0;
    this.gl.bindTexture(this.gl.TEXTURE_2D, tex.gltexid);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
    this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
    return 1;
  }
  
  requireFramebuffer(tex) {
    if (tex.fbready) return 1;
    this.requireTexture(tex);
    if (!tex.fbid) {
      if (!(tex.fbid = this.gl.createFramebuffer())) return 0;
    }
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, tex.fbid);
    this.gl.framebufferTexture2D(this.gl.FRAMEBUFFER, this.gl.COLOR_ATTACHMENT0, this.gl.TEXTURE_2D, tex.gltexid, 0);
    const status = this.gl.checkFramebufferStatus(this.gl.FRAMEBUFFER);
    if (status !== this.gl.FRAMEBUFFER_COMPLETE) return 0;
    tex.fbready = true;
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, null);
    return 1;
  }
  
  dropFramebuffer(tex) {
    if (tex.fbid) this.gl.deleteFramebuffer(tex.fbid);
    tex.fbready = false;
  }
  
  /* Egg Platform API.
   ******************************************************************************************/
  
  egg_texture_del(texid) {
    if (texid <= 1) return; // Deleting framebuffer not allowed.
    const tex = this.texv[texid];
    if (!tex) return;
    this.texv[texid] = null;
    if (tex.gltexid) this.gl.deleteTexture(tex.gltexid);
    if (tex.fbid) this.gl.deleteFramebuffer(tex.fbid);
  }
  
  egg_texture_new() {
    let texid = 2;
    for (;this.texv[texid]; texid++) ;
    const tex = {
      gltexid: 0,
      border: 0,
      w: 0,
      h: 0,
      fbid: 0,
    };
    if (!this.requireTexture(tex)) return -1;
    this.texv[texid] = tex;
    return texid;
  }
  
  egg_texture_get_size(wp, hp, texid) {
    wp >>= 2;
    hp >>= 2;
    const m32 = this.rt.exec.mem32;
    if ((wp < 0) || (wp >= m32.length) || (hp < 0) || (hp >= m32.length)) return;
    const tex = this.texv[texid];
    if (!tex) return;
    m32[wp] = tex.w;
    m32[hp] = tex.h;
  }
  
  egg_texture_load_image(texid, imgid) {
    const tex = this.texv[texid];
    if (!tex) return -1;
    const image = this.rt.images[imgid];
    if (!image) return -1;
    if (texid === 1) {
      // Loading images to texture 1 is allowed, but dimensions must match.
      if ((tex.w !== image.naturalWidth) || (tex.h !== image.naturalHeight)) return -1;
    } else {
      this.dropFramebuffer(tex);
    }
    if (!this.requireTexture(tex)) return -1;
    this.gl.bindTexture(this.gl.TEXTURE_2D, tex.gltexid);
    this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, this.gl.RGBA, this.gl.UNSIGNED_BYTE, image);
    tex.w = image.naturalWidth;
    tex.h = image.naturalHeight;
    return 0;
  }
  
  egg_texture_load_raw(texid, w, h, stride, srcp, srcc) {
    if ((w < 1) || (w > TEX_SIZE_LIMIT) || (h < 1) || (h > TEX_SIZE_LIMIT)) return -1;
    const tex = this.texv[texid];
    if (!tex) return -1;
    if (texid === 1) {
      // Uploading to texture 1 is allowed only if the dimensions match.
      if ((tex.w !== w) || (tex.h !== h)) return -1;
    }
    const minstride = w * 4;
    if (stride < 1) stride = minstride;
    else if (stride < minstride) return -1;
    const reqlen = stride * h;
    if (srcc < reqlen) return -1;
    let src = null;
    if (srcp || srcc) {
      if (!(src = this.rt.exec.getMemory(srcp, srcc))) return -1;
    }
    if (!this.requireTexture(tex)) return -1;
    this.gl.bindTexture(this.gl.TEXTURE_2D, tex.gltexid);
    if (src) {
      if (stride > minstride) {
        console.error(`TODO Video.egg_texture_load_raw: Rewrite with minimum stride`);
        return -1;
      }
      tex.border = 0;
      this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, w, h, 0, this.gl.RGBA, this.gl.UNSIGNED_BYTE, src);
    } else {
      tex.border = 32; // TODO Decide more carefully whether we need a border.
      this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, w + tex.border * 2, h + tex.border * 2, 0, this.gl.RGBA, this.gl.UNSIGNED_BYTE, null);
    }
    tex.w = w;
    tex.h = h;
    return 0;
  }
  
  egg_texture_get_pixels(dstp, dsta, texid) {
    console.log(`TODO Video.egg_texture_get_pixels ${dstp},${dsta},${texid}`);//TODO
    return 0;
  }
  
  egg_texture_clear(texid) {
    const tex = this.texv[texid];
    if (!tex) return;
    if (!this.requireFramebuffer(tex)) return;
    this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, tex.fbid);
    this.gl.clearColor(0.0, 0.0, 0.0, 0.0);
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
  }

  readUniforms(p) {
    if (!p) return null;
    const src = this.rt.exec.getMemory(p, 18);
    return {
      mode: src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24),
      dsttexid: src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24),
      srctexid: src[8] | (src[9] << 8) | (src[10] << 16) | (src[11] << 24),
      ta: src[12], // (tint) is 32 bits little-endian, ie MSB is Red.
      tb: src[13],
      tg: src[14],
      tr: src[15],
      alpha: src[16],
      filter: src[17],
    };
  }
  
  egg_render(unp, vtxp, vtxc) {
  
    // Acquire uniforms and vertices.
    const un = this.readUniforms(unp);
    if (!un) return;
    if (!un.dsttexid) return;
    const vtxv = this.rt.exec.getMemory(vtxp, vtxc);
    if (!vtxv) return;
    
    // Determine program, vertex size, and vertex count.
    let glmode = this.gl.POINTS;
    let vtxsize, pgm, ul;
    switch (un.mode) {
      case 1: vtxsize = 12; break;
      case 2: vtxsize = 12; glmode = this.gl.LINES; break;
      case 3: vtxsize = 12; glmode = this.gl.LINE_STRIP; break;
      case 4: vtxsize = 12; glmode = this.gl.TRIANGLES; break;
      case 5: vtxsize = 12; glmode = this.gl.TRIANGLE_STRIP; break;
      case 6: vtxsize = 6; pgm = this.pgm_tile; break;
      case 7: vtxsize = 16; pgm = this.pgm_fancy; break;
      default: return;
    }
    if ((vtxc = Math.floor(vtxc / vtxsize)) < 1) return;
    if (!pgm) {
      if (un.srctexid) pgm = this.pgm_tex;
      else pgm = this.pgm_raw;
    }
    if (pgm === this.pgm_raw) ul = this.u_raw;
    else if (pgm === this.pgm_tex) ul = this.u_tex;
    else if (pgm === this.pgm_tile) ul = this.u_tile;
    else ul = this.u_fancy;
  
    // If a source texture is requested, acquire it.
    let srctex = null;
    if (un.srctexid) {
      if (!(srctex = this.texv[un.srctexid])) return;
    }
  
    // Acquire destination texture and ensure it can accept output.
    let dsttex = this.texv[un.dsttexid];
    if (!dsttex || !this.requireFramebuffer(dsttex)) return;
  
    // Bind to the output texture if it's not currently bound.
    if (dsttex !== this.tex_current) {
      this.gl.bindFramebuffer(this.gl.FRAMEBUFFER, dsttex.fbid);
      this.gl.viewport(dsttex.border, dsttex.border, dsttex.w, dsttex.h);
      this.tex_current = dsttex;
    }
  
    // Bind to the program if it's not currently bound, and set uniforms.
    this.gl.useProgram(pgm);
    this.gl.uniform2f(ul.uscreensize, dsttex.w, dsttex.h);
    this.gl.uniform1f(ul.udstborder, 0);
    if (srctex) {
      this.gl.uniform2f(ul.usrcsize, srctex.w, srctex.h);
      this.gl.uniform1f(ul.usrcborder, srctex.border);
      this.gl.uniform1i(ul.usampler, 0);
      this.gl.activeTexture(this.gl.TEXTURE0);
      this.gl.bindTexture(this.gl.TEXTURE_2D, srctex.gltexid);
      if (un.filter) {
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
      } else {
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
      }
    }
    this.gl.uniform4f(ul.utint, un.tr / 255.0, un.tg / 255.0, un.tb / 255.0, un.ta / 255.0);
    this.gl.uniform1f(ul.ualpha, un.alpha / 255.0);
  
    // Prepare vertex pointers, and do it.
    this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffer);
    this.gl.bufferData(this.gl.ARRAY_BUFFER, vtxv, this.gl.STREAM_DRAW);
    if (pgm === this.pgm_raw) {
      this.gl.enableVertexAttribArray(0);
      this.gl.enableVertexAttribArray(1);
      this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
      this.gl.vertexAttribPointer(1, 4, this.gl.UNSIGNED_BYTE, true, 12, 8);
      this.gl.drawArrays(glmode, 0, vtxc);
      this.gl.disableVertexAttribArray(0);
      this.gl.disableVertexAttribArray(1);
      
    } else if (pgm === this.pgm_tex) {
      this.gl.enableVertexAttribArray(0);
      this.gl.enableVertexAttribArray(1);
      this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 12, 0);
      this.gl.vertexAttribPointer(1, 2, this.gl.SHORT, false, 12, 4);
      this.gl.drawArrays(glmode, 0, vtxc);
      this.gl.disableVertexAttribArray(0);
      this.gl.disableVertexAttribArray(1);
      
    } else if (pgm === this.pgm_tile) {
      this.gl.enableVertexAttribArray(0);
      this.gl.enableVertexAttribArray(1);
      this.gl.enableVertexAttribArray(2);
      this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 6, 0);
      this.gl.vertexAttribPointer(1, 1, this.gl.UNSIGNED_BYTE, false, 6, 4);
      this.gl.vertexAttribPointer(2, 1, this.gl.UNSIGNED_BYTE, false, 6, 5);
      this.gl.drawArrays(glmode, 0, vtxc);
      this.gl.disableVertexAttribArray(0);
      this.gl.disableVertexAttribArray(1);
      this.gl.disableVertexAttribArray(2);
      
    } else if (pgm === this.pgm_fancy) {
      this.gl.enableVertexAttribArray(0);
      this.gl.enableVertexAttribArray(1);
      this.gl.enableVertexAttribArray(2);
      this.gl.enableVertexAttribArray(3);
      this.gl.enableVertexAttribArray(4);
      this.gl.enableVertexAttribArray(5);
      this.gl.enableVertexAttribArray(6);
      this.gl.vertexAttribPointer(0, 2, this.gl.SHORT, false, 16, 0); // apos
      this.gl.vertexAttribPointer(1, 1, this.gl.UNSIGNED_BYTE, false, 16, 4); // atileid
      this.gl.vertexAttribPointer(2, 1, this.gl.UNSIGNED_BYTE, false, 16, 5); // axform
      this.gl.vertexAttribPointer(3, 1, this.gl.UNSIGNED_BYTE, true, 16, 6); // arotation
      this.gl.vertexAttribPointer(4, 1, this.gl.UNSIGNED_BYTE, false, 16, 7); // asize
      this.gl.vertexAttribPointer(5, 4, this.gl.UNSIGNED_BYTE, true, 16, 8); // atint
      this.gl.vertexAttribPointer(6, 4, this.gl.UNSIGNED_BYTE, true, 16, 12); // aprimary
      this.gl.drawArrays(glmode, 0, vtxc);
      this.gl.disableVertexAttribArray(0);
      this.gl.disableVertexAttribArray(1);
      this.gl.disableVertexAttribArray(2);
      this.gl.disableVertexAttribArray(3);
      this.gl.disableVertexAttribArray(4);
      this.gl.disableVertexAttribArray(5);
      this.gl.disableVertexAttribArray(6);
    }
  }
}

Video.glsl = {

raw_v: `
precision mediump float;
uniform vec2 uscreensize;
uniform vec2 usrcsize;
uniform float udstborder;
uniform float usrcborder;
uniform vec4 utint;
uniform float ualpha;
attribute vec2 apos;
attribute vec4 acolor;
varying vec4 vcolor;
void main() {
vec2 npos=vec2(
((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,
((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0
);
gl_Position=vec4(npos,0.0,1.0);
vcolor=vec4(mix(acolor.rgb,utint.rgb,utint.a),acolor.a*ualpha);
}
`,
    
raw_f: `
precision mediump float;
varying vec4 vcolor;
void main() {
gl_FragColor=vcolor;
}
`,
    
tex_v: `
uniform vec2 uscreensize;
uniform vec2 usrcsize;
uniform float udstborder;
uniform float usrcborder;
attribute vec2 apos;
attribute vec2 atexcoord;
varying vec2 vtexcoord;
void main() {
vec2 npos=vec2(
((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,
((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0
);
gl_Position=vec4(npos,0.0,1.0);
vtexcoord=atexcoord/usrcsize;
}
`,
    
tex_f: `
precision mediump float;
uniform sampler2D usampler;
uniform vec4 utint;
uniform float ualpha;
varying vec2 vtexcoord;
void main() {
vec4 tcolor=texture2D(usampler,vtexcoord);
gl_FragColor=vec4(mix(tcolor.rgb,utint.rgb,utint.a),tcolor.a*ualpha);
}
`,
    
tile_v: `
precision mediump float;
uniform vec2 uscreensize;
uniform vec2 usrcsize;
uniform float udstborder;
uniform float usrcborder;
uniform vec4 utint;
uniform float ualpha;
attribute vec2 apos;
attribute float atileid;
attribute float axform;
varying vec2 vsrcp;
varying mat2 vmat;
void main() {
vec2 npos=vec2(
((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,
((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0
);
gl_Position=vec4(npos,0.0,1.0);
gl_PointSize=usrcsize.x/16.0;
vsrcp=vec2(
mod(atileid,16.0),
floor(atileid/16.0)
)/16.0;
if (axform<0.5) vmat=mat2( 1.0, 0.0, 0.0, 1.0); // no xform
else if (axform<1.5) vmat=mat2(-1.0, 0.0, 0.0, 1.0); // XREV
else if (axform<2.5) vmat=mat2( 1.0, 0.0, 0.0,-1.0); // YREV
else if (axform<3.5) vmat=mat2(-1.0, 0.0, 0.0,-1.0); // XREV|YREV
else if (axform<4.5) vmat=mat2( 0.0, 1.0, 1.0, 0.0); // SWAP
else if (axform<5.5) vmat=mat2( 0.0, 1.0,-1.0, 0.0); // SWAP|XREV
else if (axform<6.5) vmat=mat2( 0.0,-1.0, 1.0, 0.0); // SWAP|YREV
else if (axform<7.5) vmat=mat2( 0.0,-1.0,-1.0, 0.0); // SWAP|XREV|YREV
else vmat=mat2( 1.0, 0.0, 0.0, 1.0); // invalid; use identity
}
`,
  
tile_f: `
precision mediump float;
uniform sampler2D usampler;
uniform float ualpha;
uniform vec4 utint;
uniform float usrcborder;
uniform vec2 usrcsize;
varying vec2 vsrcp;
varying mat2 vmat;
void main() {
vec2 texcoord=gl_PointCoord;
texcoord.y=1.0-texcoord.y;
texcoord=vmat*(texcoord-0.5)+0.5;
texcoord=vsrcp+texcoord/16.0;
texcoord=vec2(
texcoord.x*(1.0-(usrcborder*2.0)/usrcsize.x)+usrcborder/usrcsize.x,
texcoord.y*(1.0-(usrcborder*2.0)/usrcsize.y)+usrcborder/usrcsize.y
);
gl_FragColor=texture2D(usampler,texcoord);
gl_FragColor=vec4(mix(gl_FragColor.rgb,utint.rgb,utint.a),gl_FragColor.a*ualpha);
}
`,

fancy_v: `
precision mediump float;
uniform vec2 uscreensize;
uniform vec2 usrcsize;
uniform float udstborder;
uniform float usrcborder;
uniform vec4 utint;
uniform float ualpha;
attribute vec2 apos;
attribute float atileid;
attribute float axform;
attribute float arotation;
attribute float asize;
attribute vec4 atint;
attribute vec4 aprimary;
varying vec2 vsrcp;
varying mat2 vmat;
varying vec4 vtint;
varying vec4 vprimary;
void main() {
vec2 npos=vec2(
((udstborder+apos.x)*2.0)/(udstborder*2.0+uscreensize.x)-1.0,
((udstborder+apos.y)*2.0)/(udstborder*2.0+uscreensize.y)-1.0
);
gl_Position=vec4(npos,0.0,1.0);
gl_PointSize=asize;
vsrcp=vec2(
mod(atileid,16.0),
floor(atileid/16.0)
)/16.0;
if (arotation>0.0) {
float scale=(asize/(usrcsize.x/16.0));
float t=arotation*-3.14159*2.0;
if (((axform>0.5)&&(axform<2.5))||((axform>4.5)&&(axform<7.5))) {
t=-t;
}
scale*=sqrt(2.0);
float cost=cos(t)*sqrt(2.0);
float sint=sin(t)*sqrt(2.0);
vmat=mat2(cost,sint,-sint,cost);
gl_PointSize*=sqrt(2.0);
} else {
vmat=mat2(1.0,0.0,0.0,1.0);
}
if (axform<0.5) ; // no xform
else if (axform<1.5) vmat=mat2(-vmat[0][0],-vmat[0][1], vmat[1][0], vmat[1][1]); // XREV
else if (axform<2.5) vmat=mat2( vmat[0][0], vmat[0][1],-vmat[1][0],-vmat[1][1]); // YREV
else if (axform<3.5) vmat=mat2(-vmat[0][0],-vmat[0][1],-vmat[1][0],-vmat[1][1]); // XREV|YREV
else if (axform<4.5) vmat=mat2( vmat[0][1], vmat[0][0], vmat[1][1], vmat[1][0]); // SWAP
else if (axform<5.5) vmat=mat2(-vmat[0][1],-vmat[0][0], vmat[1][1], vmat[1][0]); // SWAP|XREV
else if (axform<6.5) vmat=mat2( vmat[0][1], vmat[0][0],-vmat[1][1],-vmat[1][0]); // SWAP|YREV
else if (axform<7.5) vmat=mat2( vmat[0][1],-vmat[0][0],-vmat[1][1], vmat[1][0]); // SWAP|XREV|YREV
vtint=atint;
vprimary=aprimary;
}
`,

fancy_f: `
precision mediump float;
uniform sampler2D usampler;
uniform float ualpha;
uniform vec4 utint;
uniform float usrcborder;
uniform vec2 usrcsize;
varying vec2 vsrcp;
varying mat2 vmat;
varying vec4 vtint;
varying vec4 vprimary;
void main() {
vec2 texcoord=gl_PointCoord;
texcoord.y=1.0-texcoord.y;
texcoord=vmat*(texcoord-0.5)+0.5;
if ((texcoord.x<0.0)||(texcoord.y<0.0)||(texcoord.x>=1.0)||(texcoord.y>=1.0)) discard;
texcoord=vsrcp+texcoord/16.0;
texcoord=vec2(
texcoord.x*(1.0-(usrcborder*2.0)/usrcsize.x)+usrcborder/usrcsize.x,
texcoord.y*(1.0-(usrcborder*2.0)/usrcsize.y)+usrcborder/usrcsize.y
);
gl_FragColor=texture2D(usampler,texcoord);
if ((gl_FragColor.r==gl_FragColor.g)&&(gl_FragColor.g==gl_FragColor.b)) {
if (gl_FragColor.r<0.5) {
gl_FragColor=vec4(vprimary.rgb*(gl_FragColor.r*2.0),gl_FragColor.a);
} else {
gl_FragColor=vec4(mix(vprimary.rgb,vec3(1.0,1.0,1.0),(gl_FragColor.r-0.5)*2.0),gl_FragColor.a);
}
}
gl_FragColor=vec4(mix(gl_FragColor.rgb,vtint.rgb,vtint.a),gl_FragColor.a*vprimary.a);
gl_FragColor=vec4(mix(gl_FragColor.rgb,utint.rgb,utint.a),gl_FragColor.a*ualpha);
}
`,
};
