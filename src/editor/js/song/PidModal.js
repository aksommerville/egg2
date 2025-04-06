/* PidModal.js
 * Prompt to select a fully-qualified program ID (ie 21 bits).
 * Proposes ones defined in the SDK's instrument set.
 */
 
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";

export class PidModal {
  static getDependencies() {
    return [HTMLElement, Dom, SharedSymbols, Window];
  }
  constructor(element, dom, sharedSymbols, window) {
    this.element = element;
    this.dom = dom;
    this.sharedSymbols = sharedSymbols;
    this.window = window;
    
    this.instruments = null; // Song
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(channel) {
    this.sharedSymbols.getInstruments().then(instruments => this.setupSync(channel, instruments));
  }
  
  setupSync(channel, instruments) {
    this.instruments = instruments;
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["prompt"], `Replace channel ${channel.chid} with GM program:`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.stopPropagation();
      event.preventDefault();
    }});
    const table = this.dom.spawn(form, "TABLE");
    let tr, defaultInput;
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Pid:");
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name: "pid", min: 0, max: 0x1fffff, value: 0, "on-input": () => this.onPidChanged() })
      );
    }
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Name:");
      this.dom.spawn(tr, "TD",
        defaultInput = this.dom.spawn(null, "INPUT", { type: "text", name: "name", list: "names", "on-input": () => this.onNameChanged() })
      );
    }
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": event => {
      event.preventDefault();
      event.stopPropagation();
      this.onSubmit();
    }});
    const nameList = this.dom.spawn(this.element, "DATALIST", { id: "names" });
    for (const channel of instruments.channels) {
      if (!channel?.name) continue;
      this.dom.spawn(nameList, "OPTION", { value: channel.name });
    }
    //TODO (instruments.rangeNames) could be used for drill-down menus, to bucket by category. Worth it?
    this.onPidChanged(); // Acquire name for initial program.
    this.window.requestAnimationFrame(() => {
      defaultInput.focus();
      defaultInput.select();
    });
  }
  
  onPidChanged() {
    const pid = +this.element.querySelector("input[name='pid']").value;
    const name = this.element.querySelector("input[name='name']");
    if (this.instruments) {
      const channel = this.instruments.channels[pid];
      if (channel) {
        name.value = channel.name;
        return;
      }
    }
    name.value = "";
  }
  
  onNameChanged() {
    const name = this.element.querySelector("input[name='name']").value;
    const pid = this.element.querySelector("input[name='pid']");
    if (this.instruments) {
      const channel = this.instruments.channels.find(c => c?.name === name);
      if (channel) {
        pid.value = channel.chid;
        return;
      }
    }
    pid.value = -1;
  }
  
  onSubmit() {
    const pid = +this.element.querySelector("input[name='pid']").value;
    if (pid < 0) this.resolve(null);
    else this.resolve(pid);
    this.element.remove();
  }
}
