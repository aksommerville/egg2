/* Decalsheet.js
 * We don't care whether decal IDs are unique or what they resolve to.
 * They are always strings to us. Per spec, it can be an integer 1..255 or a symbol from NS_decal_*.
 */
 
export class Decalsheet {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof Decalsheet) this._copy(src);
    else if (typeof(src) === "string") this._decode(src);
    else if (src instanceof Uint8Array) this._decode(new TextDecoder("utf8").decode(src));
    else throw new Error(`Inappropriate input to Decalsheet`);
  }
  
  _init() {
    this.decals = []; // {id:string,x,y,w,h,comment?:Uint8Array}
  }
  
  _copy(src) {
    this.decals = src.decals.map(d => {
      d = {...d};
      if (d.comment) d.comment = new Uint8Array(d.comment);
      return d;
    });
  }
  
  // From string.
  _decode(src) {
    this.decals = [];
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      const tokens = line.split(/\s+/g);
      if (tokens.length < 5) throw new Error(`${lineno}: Expected 'ID X Y W H ...'`);
      const decal = {
        id: tokens[0],
        x: +tokens[1] || 0,
        y: +tokens[2] || 0,
        w: +tokens[3] || 0,
        h: +tokens[4] || 0,
      };
      if (tokens.length > 5) {
        try {
          decal.comment = this.evalHexDump(tokens.slice(5).join(""));
        } catch (e) {
          throw new Error(`${lineno}: Invalid hex dump in decalsheet.`);
        }
      }
      this.decals.push(decal);
    }
  }
  
  evalHexDump(src) {
    if (src.length & 1) throw null;
    const dst = new Uint8Array(src.length >> 1);
    let dstp=0, srcp=0;
    for (; srcp<src.length; dstp++, srcp+=2) {
      dst[dstp] = parseInt(src.substring(srcp, srcp+2), 16);
    }
    return dst;
  }
  
  // To Uint8Array.
  encode() {
    let dst = "";
    for (const decal of this.decals) {
      // It would break formatting if (decal.id) is the empty string, which is its default value for new ones.
      // Force those to "0" instead. That's also illegal, but only at compile.
      dst += `${decal.id || 0} ${decal.x} ${decal.y} ${decal.w} ${decal.h}`;
      if (decal.comment) {
        dst += " ";
        for (let i=0; i<decal.comment.length; i++) {
          const v = decal.comment[i];
          dst += v.toString(16).padStart(2, '0');
        }
      }
      dst += "\n";
    }
    return new TextEncoder("utf8").encode(dst);
  }
}
