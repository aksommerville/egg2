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
    this.v = new Uint8Array(1024);
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
  
  finish() {
    const nv = new Uint8Array(this.c);
    const srcview = new Uint8Array(this.v.buffer, 0, this.c);
    nv.set(srcview);
    return nv;
  }
  
  require(addc) {
    if (addc < 1) return;
    if (this.c <= this.v.length - this.c) return;
    const na = ((this.c + addc + 1024) & ~1023);
    const nv = new Uint8Array(na);
    const dstview = new Uint8Array(nv.buffer, 0, this.v.length);
    dstview.set(this.v);
    this.v = nv;
  }
  
  u8(src) {
    this.require(1);
    this.v[this.c++] = src;
  }
  
  u16be(src) {
    this.require(2);
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u24be(src) {
    this.require(3);
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u32be(src) {
    this.require(4);
    this.v[this.c++] = src >> 24;
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  u16le(src) {
    this.require(2);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
  }
  
  u24le(src) {
    this.require(3);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src >> 16;
  }
  
  u32le(src) {
    this.require(4);
    this.v[this.c++] = src;
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src >> 16;
    this.v[this.c++] = src >> 24;
  }
  
  // Fixed-point numbers are always big-endian.
  
  u0_8(src) {
    src = Math.max(0, Math.min(0xff, Math.round(src * 255)));
    this.require(1);
    this.v[this.c++] = src;
  }
  
  u8_8(src) {
    src = Math.max(0, Math.min(0xffff, Math.round(src * 256)));
    this.require(2);
    this.v[this.c++] = src >> 8;
    this.v[this.c++] = src;
  }
  
  raw(src) {
    if (typeof(src) === "string") src = new TextEncoder("utf8").encode(src);
    if (src instanceof ArrayBuffer) src = new Uint8Array(src);
    if (src.hasOwnProperty("length") && !src.length) return; // Allow things like empty array.
    if (!(src instanceof Uint8Array)) throw new Error(`Encoder.raw expected string, ArrayBuffer, or Uint8Array`);
    this.require(src.length);
    const dstview = new Uint8Array(this.v.buffer, this.c, src.length);
    dstview.set(src);
    this.c += src.length;
  }
  
  vlq(src) {
    if ((typeof(src) !== "number") || (src < 0)) throw new Error(`Invalid input for VLQ: ${src}`);
    if (src < 0x80) {
      this.u8(src);
    } else if (src < 0x4000) {
      this.u8(0x80 | (src >> 7));
      this.u8(src & 0x7f);
    } else if (src < 0x200000) {
      this.u8(0x80 | (src >> 14));
      this.u8(0x80 | ((src >> 7) & 0x7f));
      this.u8(src & 0x7f);
    } else if (src < 0x10000000) {
      this.u8(0x80 | (src >> 21));
      this.u8(0x80 | ((src >> 14) & 0x7f));
      this.u8(0x80 | ((src >> 7) & 0x7f));
      this.u8(src & 0x7f);
    } else throw new Error(`Invalid input for VLQ: ${src}`);
  }
}
