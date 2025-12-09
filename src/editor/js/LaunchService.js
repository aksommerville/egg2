/* LaunchService.js
 */
 
import { Dom } from "./Dom.js";
import { Comm } from "./Comm.js";
 
export class LaunchService {
  static getDependencies() {
    return [Window, Dom, Comm];
  }
  constructor(window, dom, comm) {
    this.window = window;
    this.dom = dom;
    this.comm = comm;
    
    this.window.addEventListener("message", event => {
      if (event?.data?.event === "eggGameTerminated") {
        if (this.modal) {
          this.modal.element.remove();
        }
      }
    });
  }
  
  launch() {
    const url = `/api/buildfirst/build/index.html`;
    const modal = this.dom.spawnModal(LaunchModal);
    modal.iframe.src = url;
    this.modal = modal;
    this.modal.onremove = () => {
      this.modal = null;
    };
  }
}

export class LaunchModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.onremove = () => {};
    
    this.iframe = this.dom.spawn(this.element, "IFRAME");
  }
  
  onRemoveFromDom() {
    this.onremove();
  }
}
