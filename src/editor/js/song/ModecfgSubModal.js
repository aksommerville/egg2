/* ModecfgSubModal.js
 * Edit a channel's payload as a straight hex dump.
 */
 
import { Dom } from "../Dom.js";
import { eauModecfgDecodeSub, eauModecfgEncodeSub } from "./eauSong.js";
import { EnvUi } from "./EnvUi.js";
import { ScalarUi } from "./ScalarUi.js";

export class ModecfgSubModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.channel = null;
    this.song = null;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* Build UI.
   *************************************************************************/
  
  setup(channel, song) {
    this.channel = channel;
    this.song = song;
    this.model = eauModecfgDecodeSub(this.channel.payload);
    
    this.element.innerHTML = "";
    let autofocus = null;
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    
    this.spawnFieldController(form, "level", EnvUi);
    this.spawnFieldController(form, "widthLo", ScalarUi, "u16", "hz");
    this.spawnFieldController(form, "widthHi", ScalarUi, "u16", "hz");
    this.spawnFieldController(form, "stageCount", ScalarUi, "u8");
    this.spawnFieldController(form, "gain", ScalarUi, "u8.8");
    
    autofocus = this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    if (autofocus) {
      this.window.requestAnimationFrame(() => {
        autofocus.focus();
      });
    }
  }
  
  spawnFieldController(parent, fldname, cls, arg1, arg2) {
    const controller = this.dom.spawnController(parent, cls);
    controller.setup(this.model[fldname], fldname, arg1, arg2);
    controller.ondirty = v => {
      this.model[fldname] = v;
    }
  }
  
  /* Events.
   ***************************************************************************/
   
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    // (this.model) stays fresh, just encode it.
    const payload = eauModecfgEncodeSub(this.model);
    this.resolve(payload);
    this.element.remove();
  }
}
