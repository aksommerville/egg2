/* ModecfgModalTrivial.js
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";

export class ModecfgModalTrivial {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.unitForField = {
      maxlevel: "/ 65535",
      minlevel: "/ 65535",
      minhold: "ms",
      rlstime: "ms",
      wheelrange: "cents",
    };
    
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
  setup(mode, modecfg, chid) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.model = decodeModecfg(mode, modecfg);
    this.buildUi();
  }
  
  /* UI.
   ***********************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", `Trivial voice for channel ${this.chid}:`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => { e.preventDefault(); e.stopPropagation(); } });
    const table = this.dom.spawn(form, "TABLE");
    // Every field in our model is u16, a happy coincidence.
    for (const name of ["maxlevel", "minlevel", "minhold", "rlstime", "wheelrange"]) {
      const tr = this.dom.spawn(table, "TR");
      this.dom.spawn(tr, "TD", ["k"], name);
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name, min: 0, max: 0xffff, value: this.model[name] })
      );
      this.dom.spawn(tr, "TD", ["unit"], this.unitForField[name] || '');
    }
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    const firstInput = this.element.querySelector("input");
    firstInput.focus();
    firstInput.select();
  }
   
  /* Events.
   *****************************************************************************/
   
  onSubmit(event) {
    const model = { ...this.model };
    for (const input of this.element.querySelectorAll("input[type='number']")) {
      model[input.name] = +input.value;
    }
    this.resolve(encodeModecfg(model));
    this.element.remove();
    event.preventDefault();
  }
}
