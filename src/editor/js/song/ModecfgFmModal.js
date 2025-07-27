/* ModecfgFmModal.js
 * Edit a channel's payload as a straight hex dump.
 */
 
import { Dom } from "../Dom.js";
//TODO import { eauModecfgDecodeFm, eauModecfgEncodeFm } from "./eauSong.js";
import { ScalarUi } from "./ScalarUi.js";
import { EnvUi } from "./EnvUi.js";
import { WaveUi } from "./WaveUi.js";

export class ModecfgFmModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.channel = null;
    this.song = null;
    this.model = null; // eauSong.js:eauModecfgDecodeFm()
    
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
    //TODO this.model = eauModecfgDecodeFm(this.channel.payload);
    
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    
    this.envs = [];
    this.spawnFieldController(form, "level", EnvUi);
    this.spawnFieldController(form, "wave", WaveUi);
    this.spawnFieldController(form, "pitchEnv", EnvUi);
    this.spawnFieldController(form, "wheelRange", ScalarUi, "u16", "cents");
    this.spawnFieldController(form, "modRate", ScalarUi, "u8.8", "rel");
    this.spawnFieldController(form, "modRange", ScalarUi, "u8.8");
    this.spawnFieldController(form, "rangeEnv", EnvUi);
    this.spawnFieldController(form, "rangeLfoRate", ScalarUi, "u8.8", "qnotes");
    this.spawnFieldController(form, "rangeLfoDepth", ScalarUi, "u0.8");
    
    // Sync time scale across envelope editors.
    for (const env of this.envs) {
      env.ontimescale = (o, r) => {
        for (const other of this.envs) {
          if (env === other) continue;
          other.setTimeScale(o, r);
        }
      };
    }
    
    const submit = this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    this.window.requestAnimationFrame(() => {
      submit.focus();
    });
  }
  
  spawnFieldController(parent, fldname, cls, arg1, arg2) {
    const controller = this.dom.spawnController(parent, cls);
    controller.setup(this.model[fldname], fldname, arg1, arg2);
    controller.ondirty = v => {
      this.model[fldname] = v;
    }
    if (cls === EnvUi) {
      this.envs.push(controller);
    }
  }
  
  /* Events.
   ***************************************************************************/
   
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.model) return;
    // No need to read anything off the UI. Field controllers update our model immediately.
    //TODO const payload = eauModecfgEncodeFm(this.model);
    this.resolve(payload);
    this.element.remove();
  }
}
