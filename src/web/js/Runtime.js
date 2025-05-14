/* Runtime.js
 * Top level of Egg's web runtime.
 * You give us an encoded ROM (Uint8Array), tell us "start", and that's it.
 * We clear and replace the document's body.
 */
 
import { Rom, EGG_TID_code, EGG_TID_image } from "./Rom.js";
import { Exec } from "./Exec.js";
import { Video } from "./Video.js";
import { Audio } from "./Audio.js";
import { Input } from "./Input.js";
 
export class Runtime {
  constructor(serial) {
    this.rom = new Rom(serial);
    this.lang = 174; // en
    this.images = []; // Image object, indexed by rid, sparse.
    this.exec = new Exec(this);
    this.video = new Video(this);
    this.audio = new Audio(this);
    this.input = new Input(this);
    this.pendingFrame = null;
    this.terminated = false;
    this.clientInit = false;
    this.exitStatus = 0;
    this.storePrefix = (this.rom.getMeta("title") || "eggGame") + ".";
    const pfx = this.rom.getMeta("persistKey");
    if (pfx) this.storePrefix += pfx + ".";
    this.lastUpdateTime = 0;
    this.minUpdateTime     = 0.012000; // 83 hz; if we update faster than this, we'll deliberately skip frames.
    this.defaultUpdateTime = 0.016666; // 60 hz; if we need to make something up.
    this.maxUpdateTime     = 0.020000; // 50 hz; if we update slower than this, we'll lie about the elapsed time.
  }
  
  start() {
    this.terminated = false;
    this.clientInit = false;
    this.exitStatus = 0;
    this.selectLanguage();
    this.populateDocument();
    this.loadWasmAndImages().then(() => {
      this.video.start();
      this.audio.start();
      this.input.start();
      const err = this.exec.egg_client_init();
      this.clientInit = true;
      if (err < 0) throw `Error ${err} from game's initializer.`;
      this.lastUpdateTime = Date.now() / 1000;
      this.pendingFrame = requestAnimationFrame(() => this.update());
    }).catch(e => {
      this.reportError(e);
      this.stop();
    });
  }
  
  stop() {
    if (this.clientInit && this.exec.egg_client_quit) {
      this.exec.egg_client_quit(this.exitStatus);
      this.clientInit = false;
    }
    this.video.stop();
    this.audio.stop();
    this.input.stop();
    this.terminated = true;
    if (this.pendingFrame) {
      cancelAnimationFrame(this.pendingFrame);
      this.pendingFrame = null;
    }
  }
  
  reportError(error) {
    console.error(error);
    this.stop();
    alert(error?.message || error);
  }
  
  /* Update.
   ****************************************************************************/
   
  update() {
    this.pendingFrame = null;
    if (this.terminated) return this.stop();

    this.audio.update();
    this.input.update();
    if (this.terminated) return this.stop();
    
    const now = Date.now() / 1000;
    let elapsed = now - this.lastUpdateTime;
    if (elapsed < 0) {
      // Clock broken. Use the default interval and hope it sorts itself out.
      elapsed = this.defaultUpdateTime;
    } else if (elapsed < this.minUpdateTime) {
      // Too short elapsed. eg high-frequency monitor. Don't record the new time, just request another frame and leave.
      this.pendingFrame = requestAnimationFrame(() => this.update());
      return;
    } else if (elapsed > this.maxUpdateTime) {
      // Too long elapsed. Report our maximum to the client, and beyond that, carry on.
      elapsed = this.maxUpdateTime;
    }
    this.lastUpdateTime = now;
    
    this.exec.egg_client_update(elapsed);
    if (this.terminated) return this.stop();
    
    this.video.beginFrame();
    this.exec.egg_client_render();
    this.video.endFrame();
    
    this.pendingFrame = requestAnimationFrame(() => this.update());
  }
  
  /* Default language.
   **************************************************************************/
  
  selectLanguage() {
    const user = Array.from(new Set(
      (navigator.languages?.map(l => this.langEval(l)) || [])
        .concat(this.langEval(navigator.language)).filter(v => v)
    ));
    const game = this.rom.getMeta("lang").split(',').map(v => this.langEval(v.trim()));
    // Both lists are hopefully in preference order.
    // Use the first from (user) which is present anywhere in (game).
    for (const lang of user) {
      if (game.includes(lang)) {
        this.lang = lang;
        return;
      }
    }
    // None? OK, that happens. Use the game's most preferred.
    if (game.length > 0) {
      this.lang = game[0];
      return;
    }
    // Game didn't express a preference. Whatever, use the user's first preference.
    if (user.length > 0) {
      this.lang = user[0];
      return;
    }
    // Neither user nor game expressed a preference, so leave it unset. (it initializes to English)
  }
  
  langEval(src) {
    if (!src) return 0;
    if (src.length < 2) return 0;
    let a = src.charCodeAt(0); if ((a >= 0x41) && (a <= 0x5a)) a += 0x20; a -= 0x60;
    let b = src.charCodeAt(1); if ((b >= 0x41) && (b <= 0x5a)) b += 0x20; b -= 0x60;
    if ((a < 1) || (a > 26) || (b < 1) || (b > 26)) return 0;
    return (a << 5) | b;
  }
  
  /* Modify DOM at startup.
   ***************************************************************************/
  
  populateDocument() {
    document.title = this.rom.getMeta("title", this.lang);
    // In theory we could yoink and apply the favicon, but don't: (1) It should already done at build time, and (2) we would need a base64 encoder.
    document.body.innerHTML = '<canvas id="eggfb"></canvas>';
  }
  
  /* Loading of async things (wasm and images).
   ***************************************************************************/
   
  loadWasmAndImages() {
    const promises = [];
    let gotCode = false;
    for (const res of this.rom.resv) {
      if ((res.tid === EGG_TID_code) && (res.rid === 1)) {
        promises.push(this.exec.load(res.v));
        gotCode = true;
      } else if (res.tid === EGG_TID_image) {
        promises.push(this.loadImage(res.rid, res.v));
      }
    }
    if (!gotCode) return Promise.reject("code:1 not found");
    return Promise.all(promises);
  }
  
  loadImage(rid, src) {
    return new Promise((resolve, reject) => {
      const blob = new Blob([src]);
      const url = URL.createObjectURL(blob);
      const image = new Image();
      image.onload = () => {
        this.images[rid] = image;
        URL.revokeObjectURL(url);
        resolve();
      };
      image.onerror = e => {
        URL.revokeObjectURL(url);
        reject(e);
      };
      image.src = url;
    });
  }
  
  /* Egg Platform API.
   *******************************************************************************/
  
  egg_terminate(status) {
    this.terminated = true;
    this.exitStatus = status;
  }
  
  egg_log(msgp) {
    const msg = this.exec.getString(msgp, 1024);
    console.log(`GAME: ${msg}`);
  }
  
  egg_time_real() {
    return Date.now() / 1000;
  }
  
  egg_time_local(dstp, dsta) {
    if (dsta < 1) return;
    dstp >>= 2;
    const m32 = this.exec.mem32;
    if ((dstp < 0) || (dstp > m32.length - dsta)) return;
    const d = new Date();
    m32[dstp++] = d.getFullYear(); if (dsta < 2) return;
    m32[dstp++] = 1 + d.getMonth(); if (dsta < 3) return;
    m32[dstp++] = d.getDate(); if (dsta < 4) return;
    m32[dstp++] = d.getHours(); if (dsta < 5) return;
    m32[dstp++] = d.getMinutes(); if (dsta < 6) return;
    m32[dstp++] = d.getSeconds(); if (dsta < 7) return;
    m32[dstp] = d.getMilliseconds();
  }
  
  egg_prefs_get(k) {
    switch (k) {
      case 1: break; // TODO prefs LANG
      case 2: break; // TODO prefs MUSIC
      case 3: break; // TODO prefs SOUND
    }
    return 0;
  }
  
  egg_prefs_set(k, v) {
    switch (k) {
      case 1: break; // TODO prefs LANG
      case 2: break; // TODO prefs MUSIC
      case 3: break; // TODO prefs SOUND
    }
    return -1;
  }
  
  egg_rom_get(p, a) {
    const cpc = Math.min(a, this.rom.serial.length);
    const dst = this.exec.getMemory(p, cpc);
    if (cpc < this.rom.serial.length) dst.set(new Uint8Array(this.rom.serial.buffer, this.rom.serial.byteOffset, cpc));
    else dst.set(this.rom.serial);
    return this.rom.serial.length;
  }
  
  egg_rom_get_res(dstp, dsta, tid, rid) {
    const src = this.rom.getRes(tid, rid);
    if (!src) return 0;
    const cpc = Math.min(a, src.length);
    const dst = this.exec.getMemory(p, cpc);
    if (cpc < src.length) dst.set(new Uint8Array(src.buffer, src.byteOffset, cpc));
    else dst.set(src);
    return src.length;
  }
  
  egg_store_get(vp, va, kp, kc) {
    const k = this.storePrefix + this.exec.getString(kp, kc);
    const v = localStorage.getItem(k) || "";
    return this.exec.setString(vp, va, v);
  }
  
  egg_store_set(kp, kc, vp, vc) {
    //TODO Storage limits.
    const k = this.storePrefix + this.exec.getString(kp, kc);
    const v = this.exec.getString(vp, vc);
    if (v) localStorage.setItem(k, v);
    else localStorage.removeItem(k);
    return 0;
  }
  
  egg_store_key_by_index(kp, ka, p) {
    if (p < 0) return 0;
    for (let realp=0; ; realp++) {
      const k = localStorage.key(realp);
      if (!k.startsWith(this.storePrefix)) continue;
      if (!p--) return this.exec.setString(kp, ka, k);
    }
    return 0;
  }
}
