/* NewResourceModal.js
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class NewResourceModal {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
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
    this.dom.spawn(this.element, "H2", "New Resource");
    const form = this.dom.spawn(this.element, "FORM");
    const table = this.dom.spawn(form, "TABLE");
    this.spawnRow(table, "Type", "type"); //TODO datalist with existing types
    this.spawnRow(table, "Name", "name");
    this.spawnRow(table, "ID", "rid");
    this.spawnRow(table, "Path", "path");
    this.dom.spawn(form, "DIV", ["bottomRow"],
      this.dom.spawn(null, "INPUT", { type: "submit", value: "OK", "on-click": event => {
        event.stopPropagation();
        event.preventDefault();
        this.resolve(this.element.querySelector("input[name='path']").value);
        this.element.remove();
      }})
    );
  }
  
  spawnRow(table, label, k) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["key"], label);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    return this.dom.spawn(tdv, "INPUT", { type: "text", name: k, "on-input": () => this.onInput(k) });
  }
  
  populateSplitFromPath() {
    const path = this.element.querySelector("input[name='path']").value || "";
    const split = this.data.evalPath(path);
    this.element.querySelector("input[name='type']").value = split?.type || "";
    this.element.querySelector("input[name='name']").value = split?.name || "";
    if (split?.lang) {
      this.element.querySelector("input[name='rid']").value = split.lang + "-" + split.rid;
    } else {
      this.element.querySelector("input[name='rid']").value = split?.rid || "";
    }
  }
  
  populatePathFromSplit() {
    const type = this.element.querySelector("input[name='type']").value || "";
    const name = this.element.querySelector("input[name='name']").value || "";
    const rid = this.element.querySelector("input[name='rid']").value || "";
    const path = this.data.combinePath({ type, name, rid });
    this.element.querySelector("input[name='path']").value = path;
  }
  
  autoselectRid() {
    const type = this.element.querySelector("input[name='type']").value || "";
    let rid = this.data.unusedId(type);
    if ((type === "strings") && (rid >= 1) && (rid < 0x40)) rid = this.data.defaultLanguage + "-" + rid;
    this.element.querySelector("input[name='rid']").value = rid;
  }
  
  onInput(k) {
    if (k === "path") {
      this.populateSplitFromPath();
    } else {
      if (k === "type") this.autoselectRid();
      this.populatePathFromSplit();
    }
  }
}
