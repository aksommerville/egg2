/* EnvPointModal.js
 * Exact editing of one point in an envelope, also the only way to delete points.
 */
 
import { Dom } from "../Dom.js";

export class EnvPointModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.point = null;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* (point) comes from EnvUi, should be {t,v} both u16.
   * We'll eventually resolve with one of:
   *  - null: cancelled.
   *  - "delete": request to delete point.
   *  - A modified copy of (point).
   */
  setup(point) {
    this.point = point;
    this.element.innerHTML = "";
    let autofocus = null;
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => { e.preventDefault(); e.stopPropagation(); },
    });
    const table = this.dom.spawn(form, "TABLE");
    let tr;
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", ["key"], "Time ms");
      this.dom.spawn(tr, "TD", ["value"],
        autofocus = this.dom.spawn(null, "INPUT", { name: "time", type: "number", min: 0, value: point.t })
      );
    }
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", ["key"], "Value");
      this.dom.spawn(tr, "TD", ["value"],
        this.dom.spawn(null, "INPUT", { name: "value", type: "number", min: 0, max: 0xffff, value: point.v })
      );
    }
    this.dom.spawn(form, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "Delete Point", "on-click": () => this.onDelete() }),
      this.dom.spawn(null, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) }),
    );
    if (autofocus) {
      this.window.requestAnimationFrame(() => {
        autofocus.focus();
        autofocus.select();
      });
    }
  }
  
  readModelFromDom() {
    const model = {...this.point}; // In case there are other fields, eg "lockt".
    model.t = +this.element.querySelector("input[name='time']").value;
    model.v = +this.element.querySelector("input[name='value']").value;
    return model;
  }
  
  onDelete() {
    this.resolve("delete");
    this.element.remove();
  }
  
  onSubmit(event) {
    event.preventDefault();
    this.resolve(this.readModelFromDom());
    this.element.remove();
  }
}
