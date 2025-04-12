/* ScalarUi.js
 * Input for a single scalar field, used by Modecfg modals.
 */
 
import { Dom } from "../Dom.js";

export class ScalarUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    // Owner should replace directly. We call at high frequency during edits.
    this.ondirty = v => {};
  }
  
  /* (v) is a number.
   * (name) is a string.
   * (dataType) is one of: "u8" "u16" "u0.8" "u8.8"
   * (unit) is optional, the unit name for display, eg "hz".
   */
  setup(v, name, dataType, unit) {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["key"], name);
    const inputOptions = {
      type: "number",
      min: 0,
      value: v,
      "on-input": e => this.ondirty(+e.target.value || 0),
    };
    switch (dataType) {
      case "u8": inputOptions.max = 0xff; break;
      case "u16": inputOptions.max = 0xffff; break;
      case "u0.8": inputOptions.max = 1.0; inputOptions.step = 1 / 256; break;
      case "u8.8": inputOptions.max = 256.0; inputOptions.step = 1 / 256; break;
    }
    this.dom.spawn(this.element, "INPUT", inputOptions);
    if (unit) {
      this.dom.spawn(this.element, "DIV", ["advice"], unit);
    }
  }
}
