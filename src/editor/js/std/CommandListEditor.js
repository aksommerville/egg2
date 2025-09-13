/* CommandListEditor.js
 * Can be used as a primary resource editor or inline with some other editor.
 * Any resource type that has "CMD_*" symbols defined, and doesn't have its own editor, we will be used instead of TextEditor.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Injector } from "../Injector.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { CommandList } from "./CommandList.js";

export class CommandListEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, SharedSymbols];
  }
  constructor(element, dom, data, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.sharedSymbols = sharedSymbols;
    
    this.ondirty = null; // For owner to assign, in the sub-resource case.
    this.onhelp = null; // (tokens, index) => boolean. Return true if you handle it.
    
    this.res = null; // OPTIONAL. If present, we are a primary resource editor, otherwise we're owned by somebody.
    this.model = null;
    this.ns = "";
  }
  
  static checkResource(res) {
    if (!res.type) return 0;
    const injector = Injector.getGlobalSingleton();
    if (injector) {
      const sharedSymbols = injector.instantiate(SharedSymbols);
      if (sharedSymbols.symv.find(s => ((s.nstype === "CMD") && (s.ns === res.type)))) return 1;
    }
    return 0;
  }
  
  /* (modelOrRes) can be a resource from Data, in which case (ns) is ignored, and we interact with Data directly.
   * Otherwise it's a CommandList model which we will modify in place, and we'll call (this.ondirty) after each modification.
   * It's safe to call again to force a refresh, and you're free to kick us into a different mode then (um, but why?).
   */
  setup(modelOrRes, ns) {
    if (modelOrRes instanceof CommandList) {
      this.res = null;
      this.model = modelOrRes;
      this.ns = ns;
      // Drop empties from the end.
      while (this.model.commands.length && !this.model.commands[this.model.commands.length - 1].length) this.model.commands.splice(this.model.commands.length - 1, 1);
    } else {
      this.res = modelOrRes;
      this.model = new CommandList(modelOrRes.serial);
      this.ns = modelOrRes.type;
    }
    this.buildUi();
    // Pretend they clicked "+" immediately. If they do nothing with the new row, it's noop.
    this.onAdd();
  }
  
  /* UI.
   ******************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const list = this.dom.spawn(this.element, "DIV", ["list"]);
    for (let p=0; p<this.model.commands.length; p++) {
      this.spawnRow(list, p);
    }
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
  }
  
  spawnRow(parent, p) {
    const row = this.dom.spawn(parent, "DIV", ["row", "command"], { "data-index": p });
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": () => this.onDelete(p) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "^", "on-click": () => this.onMove(p, -1) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "v", "on-click": () => this.onMove(p, 1) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "?", "on-click": () => this.onHelp(p) });
    return this.dom.spawn(row, "INPUT", { type: "text", name: "command", value: this.model.commands[p].join(" "), "on-input": e => this.onInput(p, e) });
  }
  
  /* Events.
   ********************************************************************************/
   
  dirty() {
    if (this.res) {
      this.data.dirty(this.res.path, () => this.model.encode());
    } else {
      this.ondirty?.();
    }
  }
  
  onDelete(p) {
    if (!this.model || (p < 0) || (p >= this.model.commands.length)) return;
    this.model.commands.splice(p, 1);
    this.buildUi(); // Can't just delete the row; the indices have changed and they're baked into lambdas at buildUi().
    this.dirty();
  }
  
  onMove(p, d) {
    if (!this.model) return;
    const np = p + d;
    if ((np < 0) || (np >= this.model.commands.length)) return;
    const pcmd = this.model.commands[p];
    const ncmd = this.model.commands[np];
    this.model.commands[np] = pcmd;
    this.model.commands[p] = ncmd;
    this.element.querySelector(`.row.command[data-index='${p}'] input[name='command']`).value = ncmd.join(" ");
    this.element.querySelector(`.row.command[data-index='${np}'] input[name='command']`).value = pcmd.join(" ");
    this.dirty();
  }
  
  onHelp(p) {
    if (!this.model || (p < 0) || (p >= this.model.commands.length)) return;
    if (this.onhelp?.(this.model.commands[p], p)) return;
    //TODO Generic command list details?
  }
  
  onInput(p, e) {
    if (this.model.replaceCommand(p, e.target.value)) {
      this.dirty();
    }
  }
  
  onAdd() {
    this.model.commands.push([]);
    const input = this.spawnRow(this.element.querySelector(".list"), this.model.commands.length - 1);
    input.focus();
  }
}
