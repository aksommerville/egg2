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
  }
  
  start() {
    this.selectLanguage();
    this.populateDocument();
    this.loadWasmAndImages().then(() => {
      const err = this.exec.egg_client_init();
      if (err < 0) throw `Error ${err} from game's initializer.`;
    }).catch(e => {
      this.reportError(e);
    });
  }
  
  stop() {
  }
  
  reportError(error) {
    console.error(error);
    this.stop();
    alert(error?.message || error);
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
    console.log(`TODO Runtime.egg_terminate ${status}`);
  }
  
  egg_log(msgp) {
    console.log(`TODO Runtime.egg_log ${msgp}`);
  }
  
  egg_time_real() {
    console.log(`TODO Runtime.egg_time_real`);
    return 123.456;
  }
  
  egg_time_local(dstp, dsta) {
    console.log(`TODO Runtime.egg_time_local ${dstp},${dsta}`);
  }
  
  egg_prefs_get(k) {
    console.log(`TODO Runtime.egg_prefs_get ${k}`);
    return 0;
  }
  
  egg_prefs_set(k, v) {
    console.log(`TODO Runtime.egg_prefs_set ${k},${v}`);
    return -1;
  }
  
  egg_rom_get(p, a) {
    console.log(`TODO Runtime.egg_rom_get ${p},${a}`);
    return 0;
  }
  
  egg_rom_get_res(dstp, dsta, tid, rid) {
    console.log(`TODO Runtime.egg_rom_get_res ${dstp},${dsta},${tid},${rid}`);
    return 0;
  }
  
  egg_store_get(vp, va, kp, kc) {
    console.log(`TODO Runtime.egg_store_get ${vp},${va},${kp},${kc}`);
    return 0;
  }
  
  egg_store_set(kp, kc, vp, vc) {
    console.log(`TODO Runtime.egg_store_set ${kp},${kc},${vp},${vc}`);
    return -1;
  }
  
  egg_store_key_by_index(kp, ka, p) {
    console.log(`TODO Runtime.egg_store_key_by_index ${kp},${ka},${p}`);
    return 0;
  }
}
