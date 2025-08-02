/* EnvPointModal.js
 * Resolves with "delete" or { t,v }.
 */
 
import { Dom } from "../Dom.js";

export class EnvPointModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.t = 0;
    this.v = 0;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(point) {
    this.t = point.t;
    this.v = point.v;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => { e.preventDefault(); e.stopPropagation(); },
      "method": "",
      "action": "javascript:void(0)",
    });
    const table = this.dom.spawn(form, "TABLE");
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "time"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 10000, value: this.t, name: "t" })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "value"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 65535, value: this.v, name: "v" })
      )
    );
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => {
      e.preventDefault();
      e.stopPropagation();
      this.resolve({
        t: +this.element.querySelector("input[name='t']").value,
        v: +this.element.querySelector("input[name='v']").value,
      });
      this.element.remove();
    }});
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "Delete", "on-click": () => {
      this.resolve("delete");
      this.element.remove();
    }});
  }
}
