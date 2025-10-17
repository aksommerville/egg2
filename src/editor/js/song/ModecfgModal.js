/* ModecfgModal.js
 * Edit modecfg, the mode-specific part of a channel's configuration.
 * We handle all modes.
 */
 
import { Dom } from "../Dom.js";
import { EauDecoder, encodeEnv, decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { Encoder } from "../Encoder.js";
import { SongService } from "./SongService.js";
import { GM_DRUM_NAMES } from "./MidiConstants.js";
import { HarmonicsUi } from "./HarmonicsUi.js";
import { EnvUi } from "./EnvUi.js";
import { SongEditor } from "./SongEditor.js";

export class ModecfgModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
    /* (model) is the live model derived from (modecfg) and encodable to the same.
     * See EauDecoder.js:decodeModecfg().
     */
    this.model = {};
    
    this.envUis = [];
    this.mode = 0;
    this.modecfg = []; // Read-only, as received initially.
    this.chid = -1;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(mode, modecfg, chid) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.model = decodeModecfg(mode, modecfg);
    this.buildUi();
  }
  
  /* UI.
   *************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    this.envUis = [];
    this.form = this.dom.spawn(this.element, "FORM", { method: "", action: "", "on-submit": e => this.onSubmitForm(e) });
    switch (this.model.mode) {
      case 0: this.buildUiRaw(); break;
      case 1: this.buildUiDrum(); break;
      case 2: this.buildUiFm(); break;
      case 3: this.buildUiHarsh(); break;
      case 4: this.buildUiHarm(); break;
      case 5: this.buildUiSub(); break;
    }
    const bottom = this.dom.spawn(this.form, "DIV", ["bottom"]);
    switch (this.model.mode) {
      case 1: this.buildBottomDrum(bottom); break;
    }
    this.dom.spawn(bottom, "DIV", ["spacer"]);
    this.dom.spawn(bottom, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  buildUiRaw() {
    this.spawnExtra(this.form);
  }
  
  buildUiDrum() {
    for (const drum of this.model.drums) {
      const row = this.dom.spawn(this.form, "DIV", ["drum"]);
      this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteDrum(drum) });
      this.dom.spawn(row, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlayDrum(drum) });
      this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 255, value: drum.noteid, name: "noteid", "on-change": e => this.onDrumChange(e, drum) });
      this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 255, value: drum.trimlo, name: "trimlo", "on-change": e => this.onDrumChange(e, drum) });
      this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 255, value: drum.trimhi, name: "trimhi", "on-change": e => this.onDrumChange(e, drum) });
      this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 255, value: drum.pan, name: "pan", "on-change": e => this.onDrumChange(e, drum) });
      this.dom.spawn(row, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditDrum(drum) });
      this.dom.spawn(row, "DIV", ["size"], drum.serial.length);
      this.dom.spawn(row, "DIV", ["name"], this.songService.song.getName(this.chid, drum.noteid) || GM_DRUM_NAMES[drum.noteid] || "");
    }
  }
  
  buildBottomDrum(bottom) {
    this.dom.spawn(bottom, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddDrum() });
    this.dom.spawn(bottom, "INPUT", { type: "button", value: "Sort", "on-click": () => this.onSortDrums() });
    this.dom.spawn(bottom, "INPUT", { type: "button", value: "Add Missing", "on-click": () => this.onAddMissingDrums() });
    this.dom.spawn(bottom, "INPUT", { type: "button", value: "Remove Unused", "on-click": () => this.onRemoveUnusedDrums() });
  }
  
  buildUiFm() {
    const bigrow = this.dom.spawn(this.form, "DIV", ["bigrow"]);
    const left = this.dom.spawn(bigrow, "DIV", ["left"]);
    const table = this.dom.spawn(left, "TABLE");
    this.spawnRowU88(table, "rate");
    this.spawnRowBoolean(table, "absrate", ["relative", "qnotes"]);
    this.spawnRowU88(table, "range");
    this.spawnRowU16(table, "wheelrange");
    this.spawnRowU88(table, "lforate");
    this.spawnRowU08(table, "lfodepth");
    this.spawnRowU08(table, "lfophase");
    this.spawnExtra(left);
    const right = this.dom.spawn(bigrow, "DIV", ["right"]);
    this.dom.spawn(right, "DIV", ["advice"], "Mouse wheel + ctl,shift to zoom and scroll.");
    this.spawnEnv(right, "levelenv");
    this.spawnEnv(right, "rangeenv");
    this.spawnEnv(right, "pitchenv");
  }
  
  buildUiHarsh() {
    const bigrow = this.dom.spawn(this.form, "DIV", ["bigrow"]);
    const left = this.dom.spawn(bigrow, "DIV", ["left"]);
    const table = this.dom.spawn(left, "TABLE");
    this.spawnRowU8(table, "shape", ["sine", "square", "saw", "triangle"]);
    this.spawnRowU16(table, "wheelrange");
    this.spawnExtra(left);
    const right = this.dom.spawn(bigrow, "DIV", ["right"]);
    this.dom.spawn(right, "DIV", ["advice"], "Mouse wheel + ctl,shift to zoom and scroll.");
    this.spawnEnv(right, "levelenv");
    this.spawnEnv(right, "pitchenv");
  }
  
  buildUiHarm() {
    const bigrow = this.dom.spawn(this.form, "DIV", ["bigrow"]);
    const left = this.dom.spawn(bigrow, "DIV", ["left"]);
    const table = this.dom.spawn(left, "TABLE");
    this.spawnRowU16(table, "wheelrange");
    this.spawnHarmonics(left);
    this.spawnExtra(left);
    const right = this.dom.spawn(bigrow, "DIV", ["right"]);
    this.dom.spawn(right, "DIV", ["advice"], "Mouse wheel + ctl,shift to zoom and scroll.");
    this.spawnEnv(right, "levelenv");
    this.spawnEnv(right, "pitchenv");
  }
  
  buildUiSub() {
    const bigrow = this.dom.spawn(this.form, "DIV", ["bigrow"]);
    const left = this.dom.spawn(bigrow, "DIV", ["left"]);
    const table = this.dom.spawn(left, "TABLE");
    this.spawnRowU16(table, "widthlo");
    this.spawnRowU16(table, "widthhi");
    this.spawnRowU8(table, "stagec");
    this.spawnRowU88(table, "gain");
    const right = this.dom.spawn(bigrow, "DIV", ["right"]);
    this.dom.spawn(right, "DIV", ["advice"], "Mouse wheel + ctl,shift to zoom and scroll.");
    this.spawnEnv(right, "levelenv");
  }
  
  spawnRowU88(table, k) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], k);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    const input = this.dom.spawn(tdv, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, name: k, value: this.model[k], "on-input": e => this.onScalarInput(e) });
  }
  
  spawnRowU08(table, k) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], k);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    const input = this.dom.spawn(tdv, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, name: k, value: this.model[k], "on-input": e => this.onScalarInput(e) });
  }
  
  spawnRowU16(table, k) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], k);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    const input = this.dom.spawn(tdv, "INPUT", { type: "number", min: 0, max: 0xffff, name: k, value: this.model[k], "on-input": e => this.onScalarInput(e) });
  }
  
  spawnRowU8(table, k, enumOptions) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], k);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    if (enumOptions) {
      const select = this.dom.spawn(tdv, "SELECT", { name: k, "on-input": e => this.onScalarInput(e) });
      for (let i=0; i<enumOptions.length; i++) {
        this.dom.spawn(select, "OPTION", { value: i }, enumOptions[i]);
      }
      for (let i=enumOptions.length; i<256; i++) {
        this.dom.spawn(select, "OPTION", { value: i }, i);
      }
      select.value = this.model[k];
    } else {
      const input = this.dom.spawn(tdv, "INPUT", { type: "number", min: 0, max: 256, name: k, value: this.model[k], "on-input": e => this.onScalarInput(e) });
    }
  }
  
  spawnRowBoolean(table, k, enumOptions) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], k);
    const tdv = this.dom.spawn(tr, "TD", ["v"]);
    const pfx = `ModecfgModal-${k}`;
    const inputFalse = this.dom.spawn(tdv, "INPUT", { type: "radio", name: k, id: pfx + "-false", value: "false", "on-change": e => this.onRadioChange(e) });
    this.dom.spawn(tdv, "LABEL", { for: pfx + "-false" }, enumOptions?.[0] || "false");
    const inputTrue = this.dom.spawn(tdv, "INPUT", { type: "radio", name: k, id: pfx + "-true", value: "true", "on-change": e => this.onRadioChange(e) });
    this.dom.spawn(tdv, "LABEL", { for: pfx + "-true" }, enumOptions?.[1] || "true");
    if (this.model[k]) inputTrue.checked = true;
    else inputFalse.checked = true;
  }
  
  spawnHarmonics(parent) {
    const controller = this.dom.spawnController(parent, HarmonicsUi);
    controller.setup(this.model.harmonics, v => {
      this.model.harmonics = v;
    });
  }
  
  spawnEnv(parent, k) {
    const controller = this.dom.spawnController(parent, EnvUi);
    controller.onTimeChange = (p, c) => {
      for (const env of this.envUis) env.setTime(p, c);
    };
    controller.setup(this.model[k], k, v => {
      this.model[k] = v;
    });
    this.envUis.push(controller);
  }
  
  spawnExtra(parent) {
    const textarea = this.dom.spawn(parent, "TEXTAREA", { name: "extra", "on-input": e => this.onExtraChange(e) });
    textarea.value = this.hexdump(this.model.extra);
  }
  
  hexdump(src) {
    if (!src) return "";
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += "0123456789abcdef"[src[i] >> 4];
      dst += "0123456789abcdef"[src[i] & 15];
      dst += " ";
    }
    return dst;
  }
  
  unhexdump(src) {
    src = src.replace(/[^0-9a-fA-F]+/g, "");
    const dst = new Uint8Array(src.length >> 1);
    for (let i=0; i<src.length; i+=2) {
      dst[i >> 1] = parseInt(src.substring(i, i + 2), 16);
    }
    return dst;
  }
  
  /* Events.
   **************************************************************************************/
   
  onScalarInput(event) {
    this.model[event.target.name] = +event.target.value;
  }
  
  onRadioChange(event) {
    this.model[event.target.name] = event.target.value === "true";
  }
   
  onExtraChange(event) {
    this.model.extra = this.unhexdump(event.target.value);
  }
  
  onAddDrum() {
    let noteid = 35;
    if (this.model.drums.length && (this.model.drums[this.model.drums.length - 1].noteid < 0x7f)) {
      noteid = this.model.drums[this.model.drums.length - 1].noteid + 1;
    }
    this.model.drums.push({
      noteid,
      trimlo: 0x80,
      trimhi: 0xff,
      pan: 0x80,
      serial: [],
    });
    this.buildUi();
  }
  
  onDeleteDrum(drum) {
    // Debatable: Could show a confirmation modal. I think it's ok without; if you mess up you can cancel the whole modecfg edit.
    const p = this.model.drums.indexOf(drum);
    if (p < 0) return;
    this.model.drums.splice(p, 1);
    this.buildUi();
  }
  
  onPlayDrum(drum) {
    this.songService.playSong(drum.serial, 0);
  }
  
  onEditDrum(drum) {
    // DO NOT override SongService here; we explicitly want a new one:
    const modal = this.dom.spawnModal(SongEditor);
    modal.setupSerial(drum.serial, (song) => {
      drum.serial = song.encode();
      this.buildUi(); // Overkill. Can we defer until the modal dismisses?
    });
  }
  
  // (note,trimlo,trimhi,pan), the scalar fields per drum
  onDrumChange(event, drum) {
    const k = event.target.name;
    drum[k] = +event.target.value;
  }
  
  onSortDrums() {
    this.model.drums.sort((a, b) => a.noteid - b.noteid);
    this.buildUi();
  }
  
  onAddMissingDrums() {
    if ((typeof(this.chid) !== "number") || (this.chid < 0)) return;
    const needed = new Set();
    for (const event of this.songService.song.events) {
      if (event.chid !== this.chid) continue;
      if (event.type !== "note") continue;
      needed.add(event.noteid);
    }
    for (const note of this.model.drums) {
      needed.delete(note.noteid);
    }
    const noteidv = Array.from(needed);
    if (noteidv.length < 1) return;
    for (const noteid of noteidv) {
      this.model.drums.push({
        noteid,
        trimlo: 0x80,
        trimhi: 0xff,
        pan: 0x80,
        serial: [],
      });
    }
    this.buildUi();
  }
  
  onRemoveUnusedDrums() {
    if ((typeof(this.chid) !== "number") || (this.chid < 0)) return;
    const needed = new Set();
    for (const event of this.songService.song.events) {
      if (event.chid !== this.chid) continue;
      if (event.type !== "note") continue;
      needed.add(event.noteid);
    }
    const noteidv = [];
    for (const note of this.model.drums) {
      if (!needed.has(note.noteid)) noteidv.push(note.noteid);
    }
    if (noteidv.length < 1) return;
    this.dom.modalPickOne(`Delete ${noteidv.length} drums? ${JSON.stringify(noteidv)}`, ["Delete", "Cancel"]).then(rsp => {
      if (rsp !== "Delete") return;
      // Iterate (model.drums) instead of (noteidv) in case a noteid appears more than once.
      for (let i=this.model.drums.length; i-->0; ) {
        const noteid = this.model.drums[i].noteid;
        if (noteidv.indexOf(noteid) < 0) continue;
        this.model.drums.splice(i, 1);
      }
      this.buildUi();
    });
  }
  
  onSubmitForm(event) {
    event.stopPropagation();
    event.preventDefault();
  }
   
  onSubmit(event) {
    event.stopPropagation();
    event.preventDefault();
    this.resolve(encodeModecfg(this.model));
    this.element.remove();
  }
}
