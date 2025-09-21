/* CommandListCommandModal.js
 * Triggered by the "?" button in CommandListEditor.
 */
 
import { Dom } from "../Dom.js";

export class CommandListCommandModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setupFresh(symv) {
    this.element.innerHTML = "";
    for (const sym of symv) {
      const row = this.dom.spawn(this.element, "DIV", ["sym"]);
      this.dom.spawn(row, "INPUT", { type: "button", value: `${sym.k} ${sym.comment || ""}`, "on-click": () => this.onChooseSym(sym) });
    }
  }
  
  setupExisting(command, sym) {
    this.element.innerHTML = "";
    if (sym) {
      this.dom.spawn(this.element, "DIV", ["desc"], `${sym.k} ${sym.comment || ""}`);
    } else {
      this.dom.spawn(this.element, "DIV", ["desc"], "Unknown key.");
    }
    this.dom.spawn(this.element, "INPUT", { type: "text", name: "v", value: command.join(" ") });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSaveCommand() });
  }
  
  defaultValueFromComment(src) {
    // There is no formal definition of the schema format.
    // Just picking off a few things that I routinely do.
    // No matter what, the user will be visiting these and rewriting. We just try to make it obvious and convenient.
    const match = src.match(/^(u\d+):(.*)$/);
    if (!match) return src;
    const type = match[1];
    const name = match[2];
    if ((type === "u16") && (name === "position")) return "@0,0";
    if ((type === "u16") && name.endsWith("id")) return name.substring(0, name.length - 2) + ":0";
    return `(${type})${name}`;
  }
  
  onChooseSym(sym) {
    const command = (sym.comment || "").split(/[,\s]+/g).map(v => this.defaultValueFromComment(v));
    command.splice(0, 0, sym.k);
    this.resolve(command);
    this.element.remove();
  }
  
  onSaveCommand() {
    const v = this.element.querySelector("input[name='v']").value;
    this.resolve(v.split(/\s+/g).filter(v => v));
    this.element.remove();
  }
}
