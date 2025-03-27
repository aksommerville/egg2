/* TextEditor.js
 * Any resource encoded UTF-8, we can edit it as plain text.
 * This is the simplest possible editor, so it's a good template for custom ones.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class TextEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.textEncoder = new TextEncoder("utf8");
    this.textDecoder = new TextDecoder("utf8");
    this.buildUi();
  }
  
  static checkResource(res) {
    // Most editors should lean on (res.type) or (res.format).
    // For TextEditor, we'll say anything with a zero in the first 32 bytes is invalid, and everything else is acceptable.
    // We never return 2 ie "we're the preferred editor".
    let i=res.serial.length;
    if (i > 32) i = 32;
    while (i-- > 0) {
      if (!res.serial[i]) return 0;
    }
    return 1;
  }
  
  setup(res) {
    this.res = res;
    this.element.querySelector("textarea").value = this.textDecoder.decode(res.serial);
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
    return this.textEncoder.encode(src);
  }
}
