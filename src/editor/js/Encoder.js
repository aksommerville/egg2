/* Encoder.js
 * Helper for composing a binary file.
 */
 
export class Encoder {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof Encoder) this._copy(src);
    else throw new Error(`Inappropriate input to Encoder.`);
  }
  
  _init() {
    this.c = 0;
    this.v = []; // Uint8Array unless empty; length>=c
  }
  
  _copy(src) {
    this.c = src.c;
    this.v = new Uint8Array(src.v);
  }
  
  /* Unrelated helper for checking the first few bytes of a Uint8Array.
   */
  static checkSignature(src, sig) {
    if (src.length < sig.length) return false;
    for (let i=sig.length; i-->0; ) {
      if (sig.charCodeAt(i) !== src[i]) return false;
    }
    return true;
  }
}
