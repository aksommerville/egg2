/* ModecfgModalDrum.js
 * Drum channels are very unlike the other modes.
 */
 
import { Dom } from "../Dom.js";
import { encodeModecfg, decodeModecfg } from "./EauDecoder.js";
import { GM_DRUM_NAMES } from "./MidiConstants.js";
import { SongService } from "./SongService.js";
import { SongEditor } from "./SongEditor.js";

export class ModecfgModalDrum {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
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
   *******************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"]);
    for (const drum of this.model.drums) {
      const row = this.dom.spawn(scroller, "DIV", ["row"]);
      this.populateRow(row, drum);
    }
    const footer = this.dom.spawn(this.element, "DIV", ["row"]);
    this.dom.spawn(footer, "INPUT", { type: "button", value: "Add", "on-click": () => this.onAdd() });
    this.dom.spawn(footer, "INPUT", { type: "button", value: "Import", "on-click": () => this.onImport() });
    this.dom.spawn(footer, "INPUT", { type: "button", value: "Sort", "on-click": () => this.onSort() });
    this.dom.spawn(footer, "INPUT", { type: "button", value: "From Events", "on-click": () => this.onFromEvents() });
    this.dom.spawn(footer, "INPUT", { type: "button", value: "Remove Unsed", "on-click": () => this.onRemoveUnused() });
    this.dom.spawn(footer, "DIV", ["spacer"]);
    this.dom.spawn(footer, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() });
  }
  
  populateRow(row, drum) {
    row.innerHTML = "";
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": () => this.onDelete(drum.noteid) });
    this.dom.spawn(row, "INPUT", { type: "number", name: "noteid", min: 0, max: 256, value: drum.noteid, "on-input": e => {
      drum.noteid = +e.target.value;
      row.querySelector("input[name='name']").value = this.drumNameForNoteid(drum.noteid);
    }});
    this.dom.spawn(row, "INPUT", { type: "number", name: "trimlo", min: 0, max: 256, value: drum.trimlo, "on-input": e => drum.trimlo = +e.target.value });
    this.dom.spawn(row, "INPUT", { type: "number", name: "trimhi", min: 0, max: 256, value: drum.trimhi, "on-input": e => drum.trimhi = +e.target.value });
    this.dom.spawn(row, "INPUT", { type: "number", name: "pan", min: 0, max: 256, value: drum.pan, "on-input": e => drum.pan = +e.target.value });
    this.dom.spawn(row, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay(drum) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "...", "on-click": () => this.onEdit(drum) });
    this.dom.spawn(row, "INPUT", { type: "text", name: "name", value: this.drumNameForNoteid(drum.noteid) });
  }
  
  drumNameForNoteid(noteid) {
    return this.songService.song.getName(this.chid, noteid) || GM_DRUM_NAMES[noteid] || "";
  }
  
  /* Events.
   ********************************************************************/
   
  onDelete(noteid) {
    const p = this.model.drums.findIndex(d => d.noteid === noteid);
    if (p >= 0) {
      this.model.drums.splice(p, 1);
    }
    const input = this.element.querySelector(`input[name='noteid'][value='${noteid}']`);
    let row = input;
    while (row && !row.classList.contains("row")) row = row.parentNode;
    if (row) row.remove();
  }
  
  onPlay(drum) {
    this.songService.playSong(drum.serial, 0);
  }
  
  onEdit(drum) {
    // DO NOT override SongService here; we explicitly want a new one:
    const modal = this.dom.spawnModal(SongEditor);
    modal.setupSerial(drum.serial, (song) => {
      drum.serial = song.encode();
      // No need to update UI; we don't report the serial length or anything.
    });
  }
   
  onAdd() {
    let noteid;
    if (!this.model.drums.length) {
      noteid = 35;
    } else {
      const noteidInUse = new Set();
      let lowest=256, highest=0;
      for (const drum of this.model.drums) {
        noteidInUse.add(drum.noteid);
        if (drum.noteid < lowest) lowest = drum.noteid;
        else if (drum.noteid > highest) highest = drum.noteid;
      }
      if (highest < 127) {
        noteid = highest + 1;
      } else {
        for (let q=35; q<=81; q++) {
          if (!noteidInUse.has(q)) {
            noteid = q;
            break;
          }
        }
        if (!noteid) {
          if (lowest > 0) noteid = lowest - 1;
          else if (highest < 255) noteid = highest + 1;
          else return this.dom.modalError(`No available noteid.`);
        }
      }
    }
    const drum = { noteid, trimlo: 0x40, trimhi: 0xff, pan: 0x80, serial: new Uint8Array(0) };
    this.model.drums.push(drum);
    const scroller = this.element.querySelector(".scroller");
    const row = this.dom.spawn(scroller, "DIV", ["row"]);
    this.populateRow(row, drum);
  }
  
  onImport() {
    console.log(`TODO ModecfgModalDrum.onImport`);
    //TODO Present a modal allowing user to pick sound resources and drums from other songs, esp from the SDK, to copy in at a noteid they provide.
  }
  
  onSort() {
    this.model.drums.sort((a, b) => a.noteid - b.noteid);
    const scroller = this.element.querySelector(".scroller");
    scroller.innerHTML = "";
    for (const drum of this.model.drums) {
      const row = this.dom.spawn(scroller, "DIV", ["row"]);
      this.populateRow(row, drum);
    }
  }
  
  onFromEvents() {
    const scroller = this.element.querySelector(".scroller");
    const haveNoteid = new Set(this.model.drums.map(d => d.noteid));
    for (const event of this.songService.song.events) {
      if (event.type !== "note") continue;
      if (event.chid !== this.chid) continue;
      if (haveNoteid.has(event.noteid)) continue;
      haveNoteid.add(event.noteid);
      //TODO Pull serial from the SDK's default kit.
      const drum = { noteid: event.noteid, trimlo: 0x40, trimhi: 0xff, pan: 0x80, serial: new Uint8Array(0) };
      this.model.drums.push(drum);
      const row = this.dom.spawn(scroller, "DIV", ["row"]);
      this.populateRow(row, drum);
    }
  }
  
  onRemoveUnused() {
    const noteidInUse = new Set();
    for (const event of this.songService.song.events) {
      if (event.type !== "note") continue;
      if (event.chid !== this.chid) continue;
      noteidInUse.add(event.noteid);
    }
    for (let i=this.model.drums.length; i-->0; ) {
      const drum = this.model.drums[i];
      if (noteidInUse.has(drum.noteid)) continue;
      this.model.drums.splice(i, 1);
      const input = this.element.querySelector(`input[name='noteid'][value='${drum.noteid}']`);
      let row = input;
      while (row && !row.classList.contains("row")) row = row.parentNode;
      if (row) row.remove();
    }
  }
  
  onSubmit() {
    // Model stays fresh in real time, for the most part.
    // Do need to send each name to the song; those aren't actually recorded in modecfg.
    for (const row of this.element.querySelectorAll(".scroller > .row")) {
      const noteid = +row.querySelector("input[name='noteid']").value;
      const name = row.querySelector("input[name='name']").value;
      if (!isNaN(noteid) && (noteid >= 0) && (noteid <= 0xff)) {
        this.songService.song.setName(this.chid, noteid, name);
      }
    }
    this.resolve(encodeModecfg(this.model));
    this.element.remove();
  }
}
