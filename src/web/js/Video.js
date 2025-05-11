/* Video.js
 * Manages rendering, texture loads.
 * We don't create the canvas (that's Runtime), but we do everything else that happens to it.
 */
 
const TEX_SIZE_LIMIT = 4096;
 
export class Video {
  constructor(rt) {
    this.rt = rt;
    this.texv = []; // {gltexid,border,w,h,fbid}. Sparse, indexed by texid.
    this.canvas = null;
    this.ctx = null;
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
    this.ctx = this.canvas.getContext("webgl");
    this.texv[1] = {
      gltexid: 0,//TODO
      border: 0,
      w, h,
      fbid: 0,//TODO
    };
  }
  
  stop() {
  }
  
  beginFrame() {
  }
  
  endFrame() {
  }
  
  /* Egg Platform API.
   ******************************************************************************************/
   
  egg_video_get_screen_size(wp, hp) {
    wp >>= 2;
    hp >>= 2;
    const m32 = this.rt.exec.mem32;
    if ((wp < 0) || (wp >= m32.length) || (hp < 0) || (hp >= m32.length)) return;
    const bounds = this.canvas.getBoundingClientRect();
    m32[wp] = bounds.width;
    m32[hp] = bounds.height;
  }
  
  egg_video_fb_from_screen(xp, yp) {
    console.log(`TODO Video.egg_video_fb_from_screen ${xp},${yp}`);
  }
  
  egg_video_screen_from_fb(xp, yp) {
    console.log(`TODO Video.egg_video_screen_from_fb ${xp},${yp}`);
  }
  
  egg_texture_del(texid) {
    if (texid <= 1) return; // Deleting framebuffer not allowed.
    const tex = this.texv[texid];
    if (!tex) return;
    this.texv[texid] = null;
    //TODO Delete GL texture and framebuffer if present
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
      //TODO Drop framebuffer
    }
    //TODO Upload image to GL texture.
    tex.w = image.naturalWidth;
    tex.h = image.naturalHeight;
    return 0;
  }
  
  egg_texture_load_raw(texid, w, h, stride, srcp, srcc) {
    console.log(`TODO Video.egg_texture_load_raw ${texid},${w},${h},${stride},${srcp},${srcc}`);
    return -1;
  }
  
  egg_texture_get_pixels(dstp, dsta, texid) {
    console.log(`TODO Video.egg_texture_get_pixels ${dstp},${dsta},${texid}`);
    return 0;
  }
  
  egg_texture_clear(texid) {
    console.log(`TODO Video.egg_texture_clear ${texid}`);
  }
  
  egg_render(unp, vtxp, vtxc) {
    console.log(`TODO Video.egg_render ${unp},${vtxp},${vtxc}`);
  }
}
