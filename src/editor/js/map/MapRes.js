/* MapRes.js
 * Model for a "map" resource.
 */
 
import { CommandList } from "../std/CommandList.js";
 
export class MapRes {
  constructor(src, rid) {
    this.rid = rid || 0;
    if (!src) this._init();
    else if (src instanceof MapRes) this._copy(src);
    else if (typeof(src) === "string") this._decode(src);
    else if (src instanceof Uint8Array) this._decode(new TextDecoder("utf8").decode(src));
    else throw new Error(`Inappropriate input to MapRes`);
  }
  
  _init() {
    this.cmd = new CommandList();
    this.w = 1;
    this.h = 1;
    this.v = new Uint8Array([0]);
  }
  
  _copy(src) {
    this.cmd = new CommandList(src.cmd);
    this.w = src.w;
    this.h = src.h;
    this.v = new Uint8Array(src.v);
  }
  
  // From string.
  _decode(src) {
    if (!src) { // Empty is ok; it implicitly gets a size of 1,1.
      this._init();
      return;
    }
    this.w = 0;
    this.h = 0;
    let tmp = ""; // Cells hex dump, operative characters only.
    let srcp = 0, lineno = 1;
    for (; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      
      // Blank line terminates the image, or if we haven't started the image yet, skip it.
      if (!line || line.startsWith("#")) {
        if (!this.w) continue;
        break;
      }
      
      // First line establishes image width, subsequent lines must match it.
      if (this.w) {
        if (line.length != this.w << 1) throw new Error(`${lineno}: Expected line length ${this.w << 1}, found ${line.length}`);
      } else if ((line.length & 1) || (line.length > 510)) {
        throw new Error(`${lineno}: Invalid width ${line.length} for map cells image.`);
      } else {
        this.w = line.length >> 1;
      }
      
      // Confirm it's all hex digits and append to the temporary image.
      if (!line.match(/^[0-9a-fA-F]*$/)) throw new Error(`${lineno}: Invalid character in cells image.`);
      tmp += line;
      this.h++;
    }
    // Apply cells image.
    if ((this.w < 1) || (this.h < 1) || (this.w > 255) || (this.h > 255)) {
      throw new Error(`Invalid map size ${this.w},${this.h}. Limit 255,255.`);
    }
    this.v = new Uint8Array(this.w * this.h);
    for (let vp=0, tmpp=0; tmpp<tmp.length; vp++, tmpp+=2) {
      this.v[vp] = parseInt(tmp.substring(tmpp, tmpp+2), 16);
    }
    
    // Remainder is a command list.
    this.cmd = new CommandList(src.substring(srcp));
  }
  
  // To Uint8Array.
  encode() {
    let dst = "";
    for (let y=0, vp=0; y<this.h; y++) {
      for (let x=0; x<this.w; x++, vp++) {
        dst += this.v[vp].toString(16).padStart(2, '0');
      }
      dst += "\n";
    }
    dst += "\n"; // Blank line to separate image from commands.
    dst += this.cmd.encodeText();
    return new TextEncoder("utf8").encode(dst);
  }
  
  /* Resize in place to (nw,nh).
   * If provided (ax,ay) are the anchor point in old coordinates, which copy to (0,0) after the resize.
   * We apply that anchor to the first "@" argument of every command, too.
   * You may request the current dimensions, and use (ax,ay) to effect a shuffle.
   */
  resize(nw, nh, ax, ay) {
    if ((nw < 1) || (nh < 1) || (nw > 0xff) || (nh > 0xff)) throw new Error(`Invalid map size ${nw},${nh}`);
    if ((nw === this.w) && (nh === this.h) && !ax && !ay) return;
    const nv = new Uint8Array(nw * nh);
    this.copyCells(nv, nw, nh, this.v, this.w, this.h, ax || 0, ay || 0, this.w, this.h);
    this.w = nw;
    this.h = nh;
    this.v = nv;
    if (ax || ay) {
      for (const command of this.cmd.commands) {
        for (let i=1; i<command.length; i++) {
          if (!command[i].startsWith("@")) continue;
          const v = command[i].substring(1).split(',').map(v => +v);
          if (v.length >= 1) v[0] -= ax || 0;
          if (v.length >= 2) v[1] -= ay || 0;
          command[i] = "@" + v.join(",");
          break;
        }
      }
    }
  }
}
