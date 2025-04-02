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
   * (anchor) is one of: nw, n, ne, w, c, e, sw, s, se. "nw" if omitted.
   */
  resize(nw, nh, anchor) {
    if ((nw < 1) || (nh < 1) || (nw > 0xff) || (nh > 0xff)) throw new Error(`Invalid map size ${nw},${nh}`);
    if ((nw === this.w) && (nh === this.h)) return;
    if (!anchor) anchor = "nw";
    let dstx=0, dsty=0;
    switch (anchor) {
      case "n": case "c": case "s": dstx = (nw >> 1) - (this.w >> 1); break;
      case "ne": case "e": case "se": dstx = nw - this.w; break;
    }
    switch (anchor) {
      case "w": case "c": case "e": dsty = (nh >> 1) - (this.h >> 1); break;
      case "sw": case "s": case "se": dsty = nh - this.h; break;
    }
    const nv = new Uint8Array(nw * nh);
    this.copyCells(nv, nw, nh, this.v, this.w, this.h, dstx, dsty, this.w, this.h);
    this.w = nw;
    this.h = nh;
    this.v = nv;
    if (dstx || dsty) {
      for (const command of this.cmd.commands) {
        for (let i=1; i<command.length; i++) {
          if (!command[i].startsWith("@")) continue;
          const v = command[i].substring(1).split(',').map(v => +v);
          if (v.length >= 1) v[0] += dstx;
          if (v.length >= 2) v[1] += dsty;
          command[i] = "@" + v.join(",");
          break; // only edit the first "@" token.
        }
      }
      //TODO Entrance doors are not being updated... We can't do that from here. Does it matter?
    }
  }
  
  copyCells(dst, dstw, dsth, src, srcw, srch, dstx, dsty, w, h) {
    let srcx=0, srcy=0;
    if (dstx < 0) { w += dstx; srcx -= dstx; dstx = 0; }
    if (dsty < 0) { h += dsty; srcy -= dsty; dsty = 0; }
    if (dstx + w > dstw) w = dstw - dstx;
    if (dsty + h > dsth) h = dsth - dsty;
    if (srcx + w > srcw) w = srcw - srcx;
    if (srcy + h > srch) h = srch - srcy;
    let srcrowp = srcy * srcw + srcx;
    let dstrowp = dsty * dstw + dstx;
    for (let yi=h; yi-->0; srcrowp+=srcw, dstrowp+=dstw) {
      for (let xi=w, srcp=srcrowp, dstp=dstrowp; xi-->0; srcp++, dstp++) {
        dst[dstp] = src[srcp];
      }
    }
  }
}

/* Maps don't create POI by default.
 * MapPaint will ask for them, and store them on its own.
 * A Poi object refers to (map.cmd.commands[]) by reference.
 */
export class Poi {

  /* null if not a POI command, otherwise a Poi instance.
   */
  static fromCommand(cmd, mapid, posp) {
    if (typeof(posp) !== "number") {
      posp = cmd.findIndex(t => t.startsWith("@"));
    }
    const match = cmd[posp]?.match(/@(\d+),(\d+)/);
    if (!match) return null;
    return new Poi(cmd, +match[1], +match[2], mapid, posp);
  }
  
  constructor(cmd, x, y, mapid, posp) {
    this.cmd = cmd;
    this.kw = cmd[0];
    this.x = x;
    this.y = y;
    this.mapid = mapid;
    this.posp = posp;
    this.position = 0; // 0..3 = NW,NE,SW,SE
  }
  
  setLocation(x, y) {
    this.cmd[this.posp] = `@${x},${y}`;
  }
}
