/* Exec.js
 * Manages the WebAssembly context.
 * We are the single point of contact defining the Egg Platform API, but actual implementation of it is deferred to other classes.
 */
 
export class Exec {
  constructor(rt) {
    this.rt = rt;
    this.mem8 = this.mem32 = this.memf64 = [];
    this.fntab = [];
    this.textDecoder = new TextDecoder("utf8");
    this.textEncoder = new TextEncoder("utf8");
  }
  
  load(serial) {
    const options = { env: {

      egg_terminate: (status) => this.rt.egg_terminate(status),
      egg_log: msgp => this.rt.egg_log(msgp),
      egg_time_real: () => this.rt.egg_time_real(),
      egg_time_local: (dstp, dsta) => this.rt.egg_time_local(dstp, dsta),
      egg_prefs_get: k => this.rt.egg_prefs_get(k),
      egg_prefs_set: (k, v) => this.rt.egg_prefs_set(k, v),
      egg_rom_get: (p, a) => this.rt.egg_rom_get(p, a),
      egg_rom_get_res: (dstp, dsta, tid, rid) => this.rt.egg_rom_get_res(dstp, dsta, tid, rid),
      egg_store_get: (vp, va, kp, kc) => this.rt.egg_store_get(vp, va, kp, kc),
      egg_store_set: (kp, kc, vp, vc) => this.rt.egg_store_set(kp, kc, vp, vc),
      egg_store_key_by_index: (kp, ka, p) => this.rt.egg_store_key_by_index(kp, ka, p),
      
      egg_input_configure: () => this.rt.input.egg_input_configure(),
      egg_input_get_all: (dst, dsta) => this.rt.input.egg_input_get_all(dst, dsta),
      egg_input_get_one: playerid => this.rt.input.egg_input_get_one(playerid),
      egg_input_set_mode: m => this.rt.input.egg_input_set_mode(m),
      egg_input_get_mouse: (xp, yp) => this.rt.input.egg_input_get_mouse(xp, yp),
      
      egg_play_sound: (rid, trim, pan) => this.rt.audio.egg_play_sound(rid, trim, pan),
      egg_play_song: (songid, rid, repeat, trim, pan) => this.rt.audio.egg_play_song(songid, rid, repeat, trim, pan),
      egg_song_set: (songid, chid, prop, v) => this.rt.audio.egg_song_set(songid, chid, prop, v),
      egg_song_event_note_on: (s, c, n, v) => this.rt.audio.egg_song_event_note_on(s, c, n, v),
      egg_song_event_note_off: (s, c, n) => this.rt.audio.egg_song_event_note_off(s, c, n),
      egg_song_event_note_once: (s, c, n, v, d) => this.rt.audio.egg_song_event_note_once(s, c, n, v, d),
      egg_song_event_wheel: (s, c, v) => this.rt.audio.egg_song_event_wheel(s, c, v),
      egg_song_get_playhead: (s) => this.rt.audio.egg_song_get_playhead(s),
      
      egg_texture_del: texid => this.rt.video.egg_texture_del(texid),
      egg_texture_new: () => this.rt.video.egg_texture_new(),
      egg_texture_get_size: (wp, hp, texid) => this.rt.video.egg_texture_get_size(wp, hp, texid),
      egg_texture_load_image: (texid, imgid) => this.rt.video.egg_texture_load_image(texid, imgid),
      egg_texture_load_raw: (texid, w, h, stride, src, srcc) => this.rt.video.egg_texture_load_raw(texid, w, h, stride, src, srcc),
      egg_texture_get_pixels: (dstp, dsta, texid) => this.rt.video.egg_texture_get_pixels(dstp, dsta, texid),
      egg_texture_clear: texid => this.rt.video.egg_texture_clear(texid),
      egg_render: (unp, vtxp, vtxc) => this.rt.video.egg_render(unp, vtxp, vtxc),
    }};
    return WebAssembly.instantiate(serial, options).then(result => {
      const yoink = name => {
        if (!result.instance.exports[name]) {
          throw new Error(`ROM does not export required symbol '${name}'`);
        }
        this[name] = result.instance.exports[name];
      };
      yoink("memory");
      yoink("egg_client_quit");
      yoink("egg_client_init");
      yoink("egg_client_update");
      yoink("egg_client_render");
      this.mem8 = new Uint8Array(this.memory.buffer);
      this.mem32 = new Uint32Array(this.memory.buffer);
      this.memf64 = new Float64Array(this.memory.buffer);
      this.fntab = result.instance.exports.__indirect_function_table;
    });
  }
  
  /* Memory access.
   *******************************************************************/
  
  /* Uint8Array view of some portion of memory, or null if OOB.
   * Zero length is legal but not useful.
   * Negative length to run to the limit.
   */
  getMemory(p, c) {
    if (p < 0) return null;
    if (p > this.mem8.length) return null;
    if (c < 0) c = this.mem8.length - p;
    if (p > this.mem8.length - c) return null;
    return new Uint8Array(this.mem8.buffer, this.mem8.byteOffset + p, c);
  }
  
  /* UTF-8 string from (p) to the first NUL, but never longer than (limit).
   * Omit (limit) for no explicit limit.
   * Empty string if OOB or misencoded.
   */
  getString(p, limit) {
    if (p < 0) return "";
    let c = 0;
    if (typeof(limit) === "number") {
      while ((c < limit) && this.mem8[p + c]) c++;
    } else {
      while (this.mem8[p + c]) c++;
    }
    try {
      return this.textDecoder.decode(this.getMemory(p, c));
    } catch (e) {
      return "";
    }
  }
  
  setString(p, a, src) {
    src = this.textEncoder.encode(src);
    if (a < 0) return src.length;
    const dst = this.getMemory(p, a);
    if (src.length > dst.length) return src.length;
    dst.set(src);
    return src.length;
  }
}
