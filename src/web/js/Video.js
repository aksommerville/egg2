/* Video.js
 * Manages rendering, texture loads.
 * We don't create the canvas (that's Runtime), but we do everything else that happens to it.
 */
 
export class Video {
  constructor(rt) {
    this.rt = rt;
  }
  
  /* Egg Platform API.
   ******************************************************************************************/
   
  egg_video_get_screen_size(wp, hp) {
    console.log(`TODO Video.egg_video_get_screen_size ${wp},${hp}`);
  }
  
  egg_video_fb_from_screen(xp, yp) {
    console.log(`TODO Video.egg_video_fb_from_screen ${xp},${yp}`);
  }
  
  egg_video_screen_from_fb(xp, yp) {
    console.log(`TODO Video.egg_video_screen_from_fb ${xp},${yp}`);
  }
  
  egg_texture_del(texid) {
    console.log(`TODO Video.egg_texture_del ${texid}`);
  }
  
  egg_texture_new() {
    console.log(`TODO Video.egg_texture_new`);
    return -1;
  }
  
  egg_texture_get_size(wp, hp, texid) {
    console.log(`TODO Video.egg_texture_get_size ${wp},${hp},${texid}`);
  }
  
  egg_texture_load_image(texid, imgid) {
    console.log(`TODO Video.egg_texture_load_image ${texid},${imgid}`);
    return -1;
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
