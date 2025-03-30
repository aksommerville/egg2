/* RootUi.js
 * Top of the controller hierarchy.
 */
 
import { Dom } from "./Dom.js";
import { SidebarUi } from "./SidebarUi.js";
import { Data } from "./Data.js";
import { Actions } from "./Actions.js";
import { SharedSymbols } from "./SharedSymbols.js";

export class RootUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, Data, Actions, SharedSymbols];
  }
  constructor(element, dom, window, data, actions, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.data = data;
    this.actions = actions;
    this.sharedSymbols = sharedSymbols;
    
    this.sidebar = this.dom.spawnController(this.element, SidebarUi);
    this.dom.spawn(this.element, "DIV", ["workbench"]);
    
    this.hashListener = e => this.onHashChange(e);
    this.window.addEventListener("hashchange", this.hashListener);
    Promise.all([
      this.data.whenLoaded(),
      this.sharedSymbols.whenLoaded(),
    ]).then(() => {
      this.onHashChange({ newURL: this.window.location.href, fakeFirstEvent: true });
    });
  }
  
  onRemoveFromDom() {
    if (this.hashListener) {
      this.window.removeEventListener("hashchange", this.hashListener);
      this.hashListener = null;
    }
  }
  
  onHashChange(event) {
    const sepp = event?.newURL?.indexOf("#");
    if ((typeof(sepp) !== "number") || (sepp < 0)) return;
    const hash = event.newURL.substring(sepp + 1);
    const req = {};
    for (const field of hash.split("&")) {
      const split = field.split("=");
      const k = decodeURIComponent(split[0]);
      const v = decodeURIComponent(split.slice(1).join("="));
      req[k] = v;
    }
    const res = this.data.resv.find(r => r.path === req.path);
    if (!res) {
      this.dom.modalError(`Resource not found.`);
      return;
    }
    let editor = null;
    if (req.editor) {
      editor = this.actions.editors.find(e => e.name === req.editor);
    } else {
      editor = this.actions.pickEditorForResourceSync(res);
    }
    if (!editor) {
      this.dom.modalError(`Editor not found.`);
      return;
    }
    const workbench = this.element.querySelector(".workbench");
    workbench.innerHTML = "";
    const controller = this.dom.spawnController(workbench, editor);
    controller.setup(res);
  }
}
