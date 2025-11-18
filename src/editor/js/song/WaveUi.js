/* WaveUi.js
 */
 
import { Dom } from "../Dom.js";

export class WaveUi {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.wave = [];
    this.cb = v => {};
  }
  
  setup(wave, cb) {
    this.wave = wave;
    this.cb = cb;
    this.buildUi();
  }
  
  /* UI.
   ********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", "TODO: WaveUi");
  }
}
