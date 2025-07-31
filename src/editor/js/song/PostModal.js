/* PostModal.js
 * Edit an entire post pipe.
 */
 
import { Dom } from "../Dom.js";

export class PostModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.post = [];
    this.raw = false;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(post, raw) {
    this.post = post;
    this.raw = raw;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    //TODO
  }
}
