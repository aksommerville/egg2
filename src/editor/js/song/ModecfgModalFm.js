/* ModecfgModalFm.js
 * "FM" covers pretty much every tuned instrument. They don't necessarily involve actual FM.
 * So this is going to be more involved than most modecfg modals.
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { EnvUi } from "./EnvUi.js";
import { WaveUi } from "./WaveUi.js";

export class ModecfgModalFm {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    // All "Modecfg" modals must implement, and resolve with Uint8Array or null.
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* All "Modecfg" modals must implement.
   */
  setup(mode, modecfg, chid) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.model = decodeModecfg(mode, modecfg);
    this.buildUi();
  }
  
  /* UI.
   ***********************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    console.log(`ModecfgModalFm.buildUi`, { model: this.model });
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => { e.preventDefault(); e.stopPropagation(); }});
    
    // Left half of the form is our four envelopes, with time scales synced.
    const leftSide = this.dom.spawn(form, "DIV", ["leftSide"]);
    const levelenv = this.dom.spawnController(leftSide, EnvUi);
    levelenv.setup(this.model.levelenv, "levelenv", v => this.onEnvChange("levelenv", v));
    const mixenv = this.dom.spawnController(leftSide, EnvUi);
    mixenv.setup(this.model.mixenv, "mixenv", v => this.onEnvChange("mixenv", v));
    const rangeenv = this.dom.spawnController(leftSide, EnvUi);
    rangeenv.setup(this.model.rangeenv, "rangeenv", v => this.onEnvChange("rangeenv", v));
    const pitchenv = this.dom.spawnController(leftSide, EnvUi);
    pitchenv.setup(this.model.pitchenv, "pitchenv", v => this.onEnvChange("pitchenv", v));
    levelenv.onTimeChange = mixenv.onTimeChange = rangeenv.onTimeChange = pitchenv.onTimeChange = (p, c) => {
      levelenv.setTime(p, c);
      mixenv.setTime(p, c);
      rangeenv.setTime(p, c);
      pitchenv.setTime(p, c);
    };
    
    // Everything else goes on the right.
    const rightSide = this.dom.spawn(form, "DIV", ["rightSide"]);
    const wavesRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    this.dom.spawnController(wavesRow, WaveUi).setup(this.model.wavea, v => this.onWaveChange("wavea", v));
    this.dom.spawnController(wavesRow, WaveUi).setup(this.model.waveb, v => this.onWaveChange("waveb", v));
    
    const modRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const modTable = this.dom.spawn(modRow, "TABLE", ["modTable"]);
    this.spawnRow(modTable, "modrate", 0, 65535, 1);//TODO this needs to account for abs vs rel
    this.spawnRow(modTable, "modrange", 0, 65535, 1);
    this.dom.spawnController(modRow, WaveUi).setup(this.model.modulator, v => this.onWaveChange("modulator", v));
    
    const rangeRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const rangeTable = this.dom.spawn(rangeRow, "TABLE", ["rangeTable"]);
    this.spawnRow(rangeTable, "rangelforate", 0, 256, 1/256);
    this.spawnRow(rangeTable, "rangelfodepth", 0, 255, 1);
    this.dom.spawnController(rangeRow, WaveUi).setup(this.model.rangelfowave, v => this.onWaveChange("rangelfowave", v));
    
    const mixRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const mixTable = this.dom.spawn(mixRow, "TABLE", ["mixTable"]);
    this.spawnRow(mixTable, "mixlforate", 0, 256, 1/256);
    this.spawnRow(mixTable, "mixlfodepth", 0, 255, 1);
    this.dom.spawnController(mixRow, WaveUi).setup(this.model.mixlfowave, v => this.onWaveChange("mixlfowave", v));
    
    const miscTable = this.dom.spawn(rightSide, "TABLE", ["miscTable"]);
    this.spawnRow(miscTable, "wheelrange", 0, 65535, 1);
    
    this.dom.spawn(rightSide, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  spawnRow(table, name, min, max, step) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], name);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", min, max, step, name, value: this.model[name] })
    );
  }
  
  /* Events.
   *****************************************************************/
   
  onEnvChange(name, v) {
    this.model[name] = v;
  }
  
  onWaveChange(name, v) {
    this.model[name] = v;
  }
  
  onSubmit(event) {
    event.preventDefault();
    const model = {...this.model};
    for (const name of ["modrate", "modrange", "wheelrange", "rangelforate", "rangelfodepth", "mixlforate", "mixlfodepth"]) {
      model[name] = +this.element.querySelector(`input[name='${name}']`)?.value || 0;
    }
    console.log(`ModecfgModalFm delivering model`, model);
    this.resolve(encodeModecfg(model));
    this.element.remove();
  }
}
