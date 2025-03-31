/* SongEditor.js
 * TODO
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class SongEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
  }
  
  onRemoveFromDom() {
  }
  
  static checkResource(res) {
    if (res.format === "wav") return 0;
    if (res.type === "song") return 2;
    if (res.type === "sound") return 2;
    return 0;
  }
  
  setup(res) {
    console.log(`TODO SongEditor.setup`, res);
  }
}
