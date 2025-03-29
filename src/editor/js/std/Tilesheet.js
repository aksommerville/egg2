/* Tilesheet.js
 * Live model for a tilesheet resource.
 */
 
import { Encoder } from "../Encoder.js";
 
export class Tilesheet {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof Tilesheet) this._copy(src);
    else if (src instanceof Uint8Array) this._decode(new TextDecoder("utf8").decode(src));
    else if (typeof(src) === "string") this._decode(src);
    else throw new Error(`Unexpected input for Tilesheet.`);
  }
  
  _init() {
    this.tables = {}; // Key is table name; value is Uint8Array(256).
  }
  
  _copy(src) {
    this.tables = {};
    for (const k of Object.keys(src.tables)) {
      this.tables[k] = new Uint8Array(src.tables[k]);
    }
  }
  
  // String only.
  _decode(src) {
    this.tables = {};
    let table = null; // Uint8Array(256) if we're in the middle of one.
    let tablep = 0;
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || (line[0] === '#')) continue;
      if (!table) {
        if (!line.match(/^[0-9a-zA-Z_]+$/)) {
          throw new Error(`${lineno}: Invalid table name ${JSON.stringify(line)}`);
        }
        table = new Uint8Array(256);
        this.tables[line] = table;
        tablep = 0;
      } else {
        if (!line.match(/^[0-9a-fA-F]{32}$/)) {
          throw new Error(`${lineno}: Expected exactly 32 hex digits.`);
        }
        for (let linep=0; linep<32; linep+=2) {
          table[tablep++] = parseInt(line.substring(linep, linep+2), 16);
        }
        if (tablep >= 0x100) {
          table = null;
          tablep = 0;
        }
      }
    }
    if (table) throw new Error(`Incomplete tilesheet table.`);
  }
  
  encode() {
    let dst = "";
    for (const k of Object.keys(this.tables)) {
      const v = this.tables[k];
      if (this.tableIsEmpty(v)) continue;
      dst += k + "\n";
      for (let y=0, p=0; y<16; y++) {
        for (let x=0; x<16; x++, p++) {
          const b = v[p];
          dst += (b >> 4).toString(16);
          dst += (b & 15).toString(16);
        }
        dst += "\n";
      }
      dst += "\n";
    }
    return new TextEncoder("utf8").encode(dst);
  }
  
  tableIsEmpty(src) {
    for (let i=0; i<256; i++) if (src[i]) return false;
    return true;
  }
}
