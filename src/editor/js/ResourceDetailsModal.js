/* ResourceDetailsModal.js
 * Delete, rename, or pick an editor.
 * Resolves with null or {
 *   action: "rename" | "delete" | "edit" | "copy"
 *   path?: string # if "rename"
 *   editor?: Class # if "edit"
 * }
 */
 
import { Dom } from "./Dom.js";
import { Actions } from "./Actions.js";

export class ResourceDetailsModal {
  static getDependencies() {
    return [HTMLElement, Dom, Actions, Window];
  }
  constructor(element, dom, actions, window) {
    this.element = element;
    this.dom = dom;
    this.actions = actions;
    this.window = window;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(res) {
    this.element.innerHTML = "";
    
    this.dom.spawn(this.element, "DIV", ["path"], res.path);
    
    const fsopRow = this.dom.spawn(this.element, "FORM", ["fsopRow"], { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    const pathInput = this.dom.spawn(fsopRow, "INPUT", { type: "text", value: res.path, name: "npath", "on-input": () => this.onNpathInput() });
    this.dom.spawn(fsopRow, "INPUT", ["renameOrDelete"], { type: "submit", value: "Rename", "on-click": e => {
      e.preventDefault();
      this.onRenameOrDelete();
    }});
    this.dom.spawn(fsopRow, "INPUT", { type: "button", value: "Copy...", "on-click": () => this.onCopy() });
    
    this.dom.spawn(this.element, "DIV", "Open with...");
    const editors = this.dom.spawn(this.element, "DIV", ["editors"]);
    for (const editor of this.actions.editors) {
      const pref = editor.checkResource(res);
      this.dom.spawn(editors, "INPUT",
        ["editor", (pref === 2) ? "great" : (pref === 1) ? "good" : "bad"],
        { type: "button", value: editor.name, "on-click": () => this.onEdit(res, editor) }
      );
    }
    
    this.window.setTimeout(() => {
      pathInput.focus();
      pathInput.select();
    }, 20);
  }
  
  onNpathInput() {
    const npath = this.element.querySelector("input[name='npath']").value;
    const button = this.element.querySelector(".renameOrDelete");
    if (npath) button.value = "Rename";
    else button.value = "Delete";
  }
  
  onRenameOrDelete() {
    const npath = this.element.querySelector("input[name='npath']").value;
    if (npath) this.resolve({ action: "rename", path: npath });
    else this.resolve({ action: "delete" });
    this.element.remove();
  }
  
  onCopy() {
    this.resolve({ action: "copy" });
    this.element.remove();
  }
  
  onEdit(res, editor) {
    this.resolve({ action: "edit", editor });
    this.element.remove();
  }
}
