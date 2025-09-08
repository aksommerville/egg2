/* HexEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class HexEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.res = null;
    this.serial = [];
    this.page = 1; // one-based
    this.pageSize = 256;
    this.hexDirtyTimeout = null;
    this.textDirtyTimeout = null;
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.hexDirtyTimeout) {
      this.window.clearTimeout(this.hexDirtyTimeout);
      this.hexDirtyTimeout = null;
    }
    if (this.textDirtyTimeout) {
      this.window.clearTimeout(this.textDirtyTimeout);
      this.textDirtyTimeout = null;
    }
  }
  
  static checkResource(res) {
    // We're a special editor. We can open absolutely anything, and we are never the preferred choice.
    return 1;
  }
  
  setup(res) {
    this.res = res;
    this.serial = new Uint8Array(this.res.serial);
    this.populateUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const nav = this.dom.spawn(this.element, "DIV", ["nav"]);
    this.dom.spawn(nav, "DIV", ["pageStart"], "Page");
    this.dom.spawn(nav, "INPUT", { type: "number", name: "page", min: 1, value: 1, "on-input": e => this.onManualPage(e) });
    this.dom.spawn(nav, "DIV", ["pageSep"], "of");
    this.dom.spawn(nav, "DIV", ["pageCount"], "1");
    
    const content = this.dom.spawn(this.element, "DIV", ["content"]);
    this.dom.spawn(content, "TEXTAREA", { name: "offset", readonly: "readonly" });
    this.dom.spawn(content, "TEXTAREA", { name: "hex", "on-beforeinput": e => this.onHexDirty(event) });
    this.dom.spawn(content, "TEXTAREA", { name: "text", readonly: "readonly" });
    
    const digested = this.dom.spawn(this.element, "DIV", ["digested"]);
    
    this.populateUi();
  }
  
  populateUi() {
    const resSize = this.serial.length;
    const rowLength = 16;
    this.element.querySelector("input[name='page']").value = this.page;
    this.element.querySelector(".pageCount").value = Math.ceil(resSize / this.pageSize);
    const startp = (this.page - 1) * this.pageSize;
    const rowCount = Math.ceil(this.pageSize / rowLength);
    
    let tmp = "";
    for (let i=rowCount, p=startp; i-->0; p+=rowLength) {
      tmp += p.toString(16).padStart(8, '0') + "\n";
    }
    this.element.querySelector("textarea[name='offset']").value = tmp;
    this.populateHex();
    this.populateText();
  }
  
  populateHex() {
    const rowLength = 16;
    const startp = (this.page - 1) * this.pageSize;
    const rowCount = Math.ceil(this.pageSize / rowLength);
    let tmp = "";
    for (let ri=rowCount, p=startp; ri-->0; ) {
      for (let ci=rowLength; ci-->0; p++) {
        if (p >= this.serial.length) break;
        tmp += " " + this.serial[p].toString(16).padStart(2, '0');
      }
      tmp += "\n";
    }
    this.element.querySelector("textarea[name='hex']").value = tmp;
  }
  
  populateText() {
    const rowLength = 16;
    const startp = (this.page - 1) * this.pageSize;
    const rowCount = Math.ceil(this.pageSize / rowLength);
    let tmp = "";
    for (let ri=rowCount, p=startp; ri-->0; ) {
      for (let ci=rowLength; ci-->0; p++) {
        if (p >= this.serial.length) break;
        const v = this.serial[p];
        if ((v < 0x20) || (v >= 0x7f)) {
          tmp += ".";
        } else {
          tmp += String.fromCharCode(v);
        }
      }
      tmp += "\n";
    }
    this.element.querySelector("textarea[name='text']").value = tmp;
  }
  
  encode() {
    const src = this.element.querySelector("textarea[name='hex']").value;
    const dst = new Uint8Array(src.length >> 1);
    let srcp=0, dstc=0, hi=-1;
    for (; srcp<src.length; srcp++) {
      let digit = src.charCodeAt(srcp);
           if ((digit >= 0x30) && (digit <= 0x39)) digit = digit - 0x30;
      else if ((digit >= 0x41) && (digit <= 0x46)) digit = digit - 0x41 + 10;
      else if ((digit >= 0x61) && (digit <= 0x66)) digit = digit - 0x61 + 10;
      else continue;
      if (hi < 0) hi = digit;
      else {
        dst[dstc++] = (hi << 4) | digit;
        hi = -1;
      }
    }
    if (hi >= 0) dst[dstc++] = hi << 4;
    return new Uint8Array(dst.buffer, 0, dstc);
  }
  
  decodeHex(src) {
    const digits = src.replace(/\s+/g, "");
    if (digits.length & 1) return null;
    const len = digits.length >> 1;
    if (len > this.pageSize) return null;
    const lastPageId = Math.ceil((this.res.serial.length + 1) / this.pageSize);
    if (this.page > lastPageId) return null;
    if (len < this.pageSize) {
      if (this.page !== lastPageId) return null;
    }
    const dst = new Uint8Array(len);
    for (let dstp=0, srcp=0; dstp<len; dstp++, srcp+=2) {
      const v = parseInt(digits.substring(srcp, srcp+2), 16);
      if (isNaN(v)) return null;
      dst[dstp] = v;
    }
    return dst;
  }
  
  onManualPage(event) {
    this.page = +event.target.value || 1;
    this.populateUi();
  }
  
  onHexDirty(event) {
    // If text is dirty, its timeout must expire before we allow editing hex.
    if (this.textDirtyTimeout) {
      event.preventDefault();
      return;
    }
    if (this.hexDirtyTimeout) this.window.clearTimeout(this.hexDirtyTimeout);
    this.hexDirtyTimeout = this.window.setTimeout(() => {
      this.hexDirtyTimeout = null;
      this.onHexDirtyAfter();
    }, 500);
  }
  
  onHexDirtyAfter() {
    const hexElement = this.element.querySelector("textarea[name='hex']");
    const nserial = this.decodeHex(hexElement.value);
    if (!nserial) {
      hexElement.classList.add("invalid");
      return;
    }
    hexElement.classList.remove("invalid");
    const nend = (this.page - 1) * this.pageSize + nserial.length;
    if ((nserial.length < this.pageSize) || (nend > this.serial.length)) {
      const nc = (this.page - 1) * this.pageSize + nserial.length;
      const nfull = new Uint8Array(nc);
      nfull.set(new Uint8Array(this.serial.buffer, this.serial.byteOffset, (this.page - 1) * this.pageSize));
      new Uint8Array(nfull.buffer, nfull.byteOffset + (this.page - 1) * this.pageSize, nserial.length).set(nserial);
      this.serial = nfull;
    } else {
      new Uint8Array(this.serial.buffer, this.serial.byteOffset + (this.page - 1) * this.pageSize, nserial.length).set(nserial);
    }
    this.populateText();
    this.data.dirty(this.res.path, () => this.serial);
  }
}
