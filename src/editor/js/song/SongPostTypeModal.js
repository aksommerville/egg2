/* SongPostTypeModal.js
 * Asks for the type of a new post stage.
 * Far as this editor is concerned, post stageid is immutable after creation.
 * This is really a trivial modal. We take no input, and resolve with an integer 0..255 (or null if cancelled).
 */
 
import { Dom } from "../Dom.js";
import { EAU_POST_STAGE_NAMES } from "./eauSong.js";

export class SongPostTypeModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    // No sense using an "OK" button; just submit when they pick the type.
    const select = this.dom.spawn(this.element, "SELECT", { "on-change": () => this.onSubmit() });
    for (let stageid=0; stageid<0x100; stageid++) {
      this.dom.spawn(select, "OPTION", { value: stageid }, EAU_POST_STAGE_NAMES[stageid] || stageid.toString());
    }
    //this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
  }
  
  onSubmit() {
    const stageid = +this.element.querySelector("select")?.value;
    if (isNaN(stageid)) return;
    this.resolve(stageid);
    this.element.remove();
  }
}
