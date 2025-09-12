/* ImageAnimationService.js
 * Part of ImageEditor. Manages user-defined animation of tiles or decals within one image.
 */
 
import { SharedSymbols } from "../SharedSymbols.js";
 
/* Contains multiple faces, each a set of frames pulled from one image.
 * faces: {
 *   name: string;
 *   delay: number; // ms, for frames that don't specify.
 *   frames: {
 *     delay: number; // use face level if zero
 *     tiles: {
 *       tileid: string|number; // number for tilesheets, string for decalsheets
 *       x: number; // offset; zero is typical
 *       y: number; // ''
 *       xform: number; // 1,2,4 = XREV,YREV,SWAP
 *     }[]
 *   }[]
 * }[]
 * Encodes to a JSON array, the exact content of (faces).
 */
export class Animation {
  constructor(src) {
    this.faces = [];
    if (typeof(src) === "string") this.decode(src);
  }
  
  decode(src) {
    this.faces = JSON.parse(src);
  }
  
  encode() {
    return JSON.stringify(this.faces);
  }
  
  uniqueFaceName() {
    for (let qualifier=1;; qualifier++) {
      const name = `Untitled-${qualifier}`;
      if (!this.faces.find(f => f.name === name)) return name;
    }
  }
}
 
export class ImageAnimationService {
  static getDependencies() {
    return [Window, SharedSymbols];
  }
  constructor(window, sharedSymbols) {
    this.window = window;
    this.sharedSymbols = sharedSymbols;
  }
  
  getAnimationForImage(path) {
    if (!path) return Promise.resolve(null);
    return this.sharedSymbols.whenLoaded().then(() => {
      return this.getAnimationFromLocalStorage(this.sharedSymbols.projname, path);
    });
  }
  
  saveAnimation(path, animation) {
    const spl = path.split("/");
    const base = spl[spl.length - 1];
    const k = `egg-${this.sharedSymbols.projname}-animation-${base}`;
    if (animation) {
      this.window.localStorage.setItem(k, animation.encode());
    } else {
      this.window.localStorage.removeItem(k);
    }
  }
  
  getAnimationFromLocalStorage(projname, path) {
    const spl = path.split("/");
    const base = spl[spl.length - 1];
    const k = `egg-${projname}-animation-${base}`;
    const v = this.window.localStorage.getItem(k);
    if (!v) return null;
    return new Animation(v);
  }
}

ImageAnimationService.singleton = true;
