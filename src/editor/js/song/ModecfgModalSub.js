/* ModecfgModalSub.js
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { EnvUi } from "./EnvUi.js";

export class ModecfgModalSub {
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
  setup(mode, modecfg, chid) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.model = decodeModecfg(mode, modecfg);
    this.buildUi();
  }
  
  /* UI.
   ***************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", `Sub voice for channel ${this.chid}:`);
    
    const levelenvController = this.dom.spawnController(this.element, EnvUi);
    levelenvController.setup(this.model.levelenv, "levelenv", v => this.onLevelenvChange(v));
    
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => { e.preventDefault(); e.stopPropagation(); } });
    const table = this.dom.spawn(form, "TABLE");
    this.spawnRow(table, "widthlo", 0, 65535, 1, "hz");
    this.spawnRow(table, "widthhi", 0, 65535, 1, "hz");
    this.spawnRow(table, "stagec", 0, 255, 1, "");
    this.spawnRow(table, "gain", 0, 256, 1/256, "x");
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    
    const firstInput = this.element.querySelector("input[name='widthlo']");
    firstInput.focus();
    firstInput.select();
  }
  
  spawnRow(table, name, min, max, step, unit) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], name);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", name, min, max, step, value: this.model[name] })
    );
    if (unit) this.dom.spawn(tr, "TD", ["unit"], unit);
  }
  
  /* Events.
   ***********************************************************/
   
  onLevelenvChange(v) {
    this.model.levelenv = v;
  }
  
  onSubmit(event) {
    const model = {...this.model};
    for (const input of this.element.querySelectorAll("input[type='number']")) {
      model[input.name] = +input.value;
    }
    this.resolve(encodeModecfg(model));
    this.element.remove();
    event.preventDefault();
  }
}
