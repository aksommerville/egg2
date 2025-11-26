/* ModecfgModalRaw.js
 * Just a hex editor, matching the ModecfgModal interface.
 */
 
import { Dom } from "../Dom.js";

export class ModecfgModalRaw {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    // All "Modecfg" modals must implement, and resolve with Uint8Array or null.
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* All "Modecfg" modals must implement.
   */
  setup(mode, modecfg, chid, post) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.post = post;
    
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "TEXTAREA", this.reprHex(modecfg));
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  onSubmit(event) {
    const textarea = this.element.querySelector("textarea");
    const serial = this.evalHex(textarea.value);
    this.resolve(serial);
    this.element.remove();
  }
  
  reprHex(src) {
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += src[i].toString(16).padStart(2, '0') + " ";
    }
    return dst;
  }
  
  evalHex(src) {
    let hi = -1;
    let dst = []; // Assemble temporarily into a full array.
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
           if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30;
      else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x41 + 10;
      else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x61 + 10;
      else throw new Error(`Unexpected character ${JSON.stringify(ch)} in hex dump`);
      if (hi < 0) hi = ch; else {
        dst.push((hi << 4) | ch);
        hi = -1;
      }
    }
    if (hi >= 0) throw new Error(`Uneven hex dump length.`);
    return new Uint8Array(dst);
  }
}
