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
  }
  
  launch() {
    const url = `/api/buildfirst/build/index.html`;
    const modal = this.dom.spawnModal(LaunchModal);
    modal.iframe.src = url;
  }
}

export class LaunchModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.iframe = this.dom.spawn(this.element, "IFRAME");
  }
}
