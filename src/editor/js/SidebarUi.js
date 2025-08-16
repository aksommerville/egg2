/* SidebarUi.js
 * Left side of RootUi. Resource picker and global controls.
 */
 
import { Dom } from "./Dom.js";
import { Actions } from "./Actions.js";
import { NewResourceModal } from "./NewResourceModal.js";
import { ResourceDetailsModal } from "./ResourceDetailsModal.js";
import { Data } from "./Data.js";

export class SidebarUi {
  static getDependencies() {
    return [HTMLElement, Dom, Actions, Data, Window];
  }
  constructor(element, dom, actions, data, window) {
    this.element = element;
    this.dom = dom;
    this.actions = actions;
    this.data = data;
    this.window = window;
    
    this.openTypes = [];
    this.loadOpenTypes();
    
    this.buildUi();
    this.dirtyListener = this.data.listenDirty(state => this.onDirtyStateChanged(state));
    this.tocListener = this.data.listenToc(resv => this.onTocChanged(resv));
  }
  
  onRemoveFromDom() {
    this.data.unlistenDirty(this.dirtyListener);
    this.data.unlistenToc(this.tocListener);
  }
  
  loadOpenTypes() {
    try {
      const openTypes = JSON.parse(this.window.localStorage.getItem("egg2.SidebarUi.openTypes"));
      if ((openTypes instanceof Array) && !openTypes.find(t => (typeof(t) !== "string"))) {
        this.openTypes = openTypes;
      }
    } catch (e) {}
  }
  
  saveOpenTypes() {
    this.window.localStorage.setItem("egg2.SidebarUi.openTypes", JSON.stringify(this.openTypes));
  }
  
  highlightOpenResource(res) {
    for (const element of this.element.querySelectorAll(".res.highlight")) element.classList.remove("highlight");
    const element = this.element.querySelector(`.res[data-path='${res?.path}']`);
    if (!element) return;
    element.classList.add("highlight");
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "DIV", ["backendIndicator"]);
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddResource() });
    const actionsMenu = this.dom.spawn(topRow, "SELECT", ["actions"], { "on-change": e => this.onActionsChanged(e) });
    this.dom.spawn(actionsMenu, "OPTION", { value: "", disabled: "disabled", selected: "selected" }, "Actions...");
    for (const action of this.actions.actions) {
      this.dom.spawn(actionsMenu, "OPTION", { value: action.name }, action.label || action.name);
    }
    
    const toc = this.dom.spawn(this.element, "DIV", ["toc"]);
    this.populateToc();
  }
  
  /* Rebuild the list of resources.
   */
  populateToc() {
    const buckets = {};
    for (const res of this.data.resv) {
      let bucket = buckets[res.type];
      if (!bucket) bucket = buckets[res.type] = [];
      const bits = res.path.split("/");
      const base = bits[bits.length - 1];
      bucket.push({
        ...res,
        base,
      });
    }
    for (const bucket of Object.values(buckets)) {
      bucket.sort((a, b) => {
        if (a.rid < b.rid) return -1;
        if (a.rid > b.rid) return 1;
        if (a.lang < b.lang) return -1;
        if (a.lang > b.lang) return 1;
        if (a.base < b.base) return -1;
        if (a.base > b.base) return 1;
        return 0;
      });
    }
    const types = Object.keys(buckets);
    types.sort();
    const toc = this.element.querySelector(".toc");
    toc.innerHTML = "";
    for (const type of types) {
      const bucket = buckets[type];
      const details = this.dom.spawn(toc, "DETAILS", ["type"], { "data-type": type, "on-toggle": () => this.onDetailsToggled() });
      if (this.openTypes.includes(type)) details.open = true;
      this.dom.spawn(details, "SUMMARY", ["type"], type);
      for (const res of bucket) {
        this.dom.spawn(details, "DIV", ["res"], {
          "on-click": e => this.onClickRes(e, res),
          "on-contextmenu": e => this.onRightClickRes(e, res),
          "data-path": res.path,
        }, res.base);
      }
    }
  }
  
  /* Events.
   ***************************************************************************************/
   
  onDirtyStateChanged(state) {
    const indicator = this.element.querySelector(".backendIndicator");
    indicator.classList.remove("dirty");
    indicator.classList.remove("clean");
    indicator.classList.remove("pending");
    indicator.classList.remove("error");
    indicator.classList.add(state);
  }
  
  onTocChanged(resv) {
    this.populateToc();
  }
  
  onActionsChanged(event) {
    const actionsMenu = this.element.querySelector(".actions");
    const name = actionsMenu.value;
    actionsMenu.value = "";
    const action = this.actions.actions.find(a => a.name === name);
    if (!action) {
      console.log(`Action ${JSON.stringify(name)} not found.`);
      return;
    }
    action.fn();
  }
  
  onAddResource() {
    const modal = this.dom.spawnModal(NewResourceModal);
    modal.result.then(path => {
      if (!path) return;
      this.data.createResource(path).then(res => {
        this.actions.editResource(path);
      }).catch(e => this.dom.modalError(e));
    });
  }
  
  onClickRes(event, res) {
    this.actions.editResource(res.path);
  }
  
  onRightClickRes(event, res) {
    event.preventDefault();
    event.stopPropagation();
    const modal = this.dom.spawnModal(ResourceDetailsModal);
    modal.setup(res);
    modal.result.then(rsp => {
      if (!rsp) return;
      if (rsp.action === "delete") {
        return this.data.deleteResource(res.path);
      } else if (rsp.action === "rename") {
        return this.data.renameResource(res.path, rsp.path);
      } else if (rsp.action === "edit") {
        this.actions.editResource(res.path, rsp.editor?.name);
      } else if (rsp.action === "copy") {
        this.copyResource(res);
      }
    }).catch(e => this.dom.modalError(e));
  }
  
  copyResource(res) {
    const modal = this.dom.spawnModal(NewResourceModal);
    const defaults = {
      msg: `Copy ${res.type}:${res.rid}...`,
      type: res.type,
      rid: this.data.unusedId(res.type),
    };
    if (res.name) defaults.name = res.name + "_copy";
    modal.setup(defaults);
    modal.result.then(path => {
      if (!path) return;
      return this.data.createResource(path).then(nres => {
        this.actions.editResource(path);
      });
    }).catch(e => this.dom.modalError(e));
  }
  
  onDetailsToggled() {
    this.openTypes = Array.from(this.element.querySelectorAll("details.type[open]")).map(e => e.getAttribute("data-type")).filter(v => v);
    this.saveOpenTypes();
  }
}
