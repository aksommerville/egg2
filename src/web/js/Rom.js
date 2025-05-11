/* Rom.js
 */
 
export const
  EGG_TID_metadata = 1,
  EGG_TID_code = 2,
  EGG_TID_strings = 3,
  EGG_TID_image = 4,
  EGG_TID_song = 5,
  EGG_TID_sound = 6,
  EGG_TID_tilesheet = 7,
  EGG_TID_decalsheet = 8,
  EGG_TID_map = 9,
  EGG_TID_sprite = 10;
 
export class Rom {
  constructor(src) {
    this.serial = src;
  
    /* Decode serial into an array of resources.
     */
    if (!(src instanceof Uint8Array)) throw new Error("Expected Uint8Array");
    this.resv = []; // {tid,rid,v}. Sorted by (tid,rid). (v) is Uint8Array pointing into (src).
    if ((src.length < 4) || (src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x52) || (src[3] !== 0x4d)) { // "\0ERM"
      throw new Error("Invalid ROM");
    }
    let srcp = 4, tid = 1, rid = 1;
    for (;;) {
      const lead = src[srcp++];
      if (!lead) break; // EOF or OOB. We won't force an error for OOB even though it is technically illegal.
      switch (lead & 0xc0) {
        case 0x00: { // TID
            tid += lead;
            rid = 1;
          } break;
        case 0x40: { // RID
            rid += (((lead & 0x3f) << 8) | src[srcp++]) + 1;
          } break;
        case 0x80: { // RES
            let len = (lead & 0x3f) << 16;
            len |= src[srcp++] << 8;
            len |= src[srcp++];
            len++;
            if ((tid > 0xff) || (rid > 0xffff) || (srcp > src.length - len)) {
              throw new Error("Invalid ROM");
            }
            this.resv.push({ tid, rid, v: new Uint8Array(src.buffer, src.byteOffset + srcp, len) });
            rid++;
            srcp += len;
          } break;
        default: throw new Error("Invalid ROM"); // All other leading bytes are reserved.
      }
    }
    
    /* Split out metadata:1 and store it.
     */
    this.td = new TextDecoder("utf8");
    this.meta = {};
    src = this.getRes(EGG_TID_metadata, 1);
    if (src && (src.length >= 4) && (src[0] === 0x00) && (src[1] === 0x45) && (src[2] === 0x4d) && (src[3] === 0x44)) {
      for (srcp=4;;) {
        const kc = src[srcp++];
        if (!kc) break;
        const vc = src[srcp++];
        if (srcp > src.length - vc - kc) break;
        const k = this.td.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, kc));
        srcp += kc;
        const v = this.td.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, vc));
        srcp += vc;
        this.meta[k] = v;
      }
    }
  }
  
  /* Uint8Array or null.
   */
  getRes(tid, rid) {
    let lo=0, hi=this.resv.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.resv[ck];
           if (tid < q.tid) hi = ck;
      else if (tid > q.tid) lo = ck + 1;
      else if (rid < q.rid) hi = ck;
      else if (rid > q.rid) lo = ck + 1;
      else return q.v;
    }
    return null;
  }
  
  /* String, empty if absent.
   * (lang) is numeric; zero to skip language lookup.
   * Do not include "$" in the key if you're using (lang).
   */
  getMeta(k, lang) {
    if (lang) {
      const ix = +this.meta[k + "$"];
      if (ix) {
        const v = this.getString(lang, 1, ix);
        if (v) return v;
      }
    }
    return this.meta[k] || "";
  }
  
  getString(lang, rid, ix) {
    return this.readString(this.getRes(EGG_TID_strings, (lang << 6) | rid), ix);
  }
  
  /* Return string from an encoded strings resource.
   * Empty string if not found.
   */
  readString(src, ix) {
    if (ix < 1) return "";
    if (!src || (src.length < 4) || (src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x53) || (src[3] !== 0x54)) return "";
    let srcp = 4;
    for (;;) {
      const len = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - len) return "";
      if (!--ix) return this.td.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, len));
      srcp += len;
    }
  }
}
