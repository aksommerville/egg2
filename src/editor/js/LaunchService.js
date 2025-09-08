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
    
    this.webpath = "";
    this.webpathPromise = null;
  }
  
  launch() {
    this.requireWebPath().then(path => {
      const url = `/api/buildfirst${path}`;
      const modal = this.dom.spawnModal(LaunchModal);
      modal.iframe.src = url;
    }).catch(e => this.dom.modalError(e));
  }
  
  requireWebPath() {
    if (this.webpath) return Promise.resolve(this.webpath);
    if (this.webpathPromise) return this.webpathPromise;
    return this.webpathPromise = this.comm.httpText("GET", "/api/webpath").then(rsp => {
      this.webpathPromise = null;
      return this.webpath = rsp;
    });
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
