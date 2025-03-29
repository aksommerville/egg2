/* HexEditor.js
 * TODO I'm writing this quick and dirty just to have it as a fallback.
 * When there's time, flesh it out and make something useful of this. In particular:
 *  - UI for row length and page length.
 *  - Display one page at a time. So we don't choke on really big resources.
 *  - Better validation and reporting.
 *  - Select a range of text and then you can edit it as a multibyte word, or UTF-8, or whatever.
 *  - Offset and ASCII columns.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class HexEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.buildUi();
  }
  
  static checkResource(res) {
    // We're a special editor. We can open absolutely anything, and we are never the preferred choice.
    return 1;
  }
  
  setup(res) {
    this.res = res;
    this.element.querySelector("textarea").value = this.repr(res.serial);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "TEXTAREA", { "on-input": () => {
      if (!this.res) return;
      this.data.dirty(this.res.path, () => this.encode());
    }});
  }
  
  encode() {
    const src = this.element.querySelector("textarea").value;
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
  
  repr(src) {
    // Ouch this is really going to be expensive for huge resources.
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += src[i].toString(16).padStart(2, '0') + " ";
    }
    return dst;
  }
}
