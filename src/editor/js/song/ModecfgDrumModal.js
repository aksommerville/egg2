/* ModecfgDrumModal.js
 * Edit a channel's payload as a straight hex dump.
 */
 
import { Dom } from "../Dom.js";
import { eauModecfgEncodeDrum, eauModecfgDecodeDrum } from "./eauSong.js";
import { reprGmDrum } from "./songDisplayBits.js";
import { DrumsImportModal } from "./DrumsImportModal.js";

export class ModecfgDrumModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.channel = null;
    this.song = null;
    this.model = null;
    
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
    this.model = eauModecfgDecodeDrum(channel.payload);
    
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    
    const scroller = this.dom.spawn(form, "DIV", ["scroller"]);
    const table = this.dom.spawn(scroller, "TABLE", ["notes"]);
    this.rebuildNotesTable(table);
    
    const buttonsRow = this.dom.spawn(form, "DIV", ["row"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Add per events", "on-click": () => this.onAddNotes() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Remove per events", "on-click": () => this.onRemoveNotes() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Import...", "on-click": () => this.onImport() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Sort", "on-click": () => this.onSort() });
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Add", "on-click": () => this.onAddOne() });
    this.dom.spawn(buttonsRow, "DIV", ["spacer"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  rebuildNotesTable(table) {
    if (!table) table = this.element.querySelector(".notes");
    table.innerHTML = "";
    this.dom.spawn(table, "TR", ["tableHeader"],
      this.dom.spawn(null, "TH", ["gmName"], "GM Name"),
      this.dom.spawn(null, "TH", "Note ID"),
      this.dom.spawn(null, "TH", "Trim (min)"),
      this.dom.spawn(null, "TH", "Trim (max)"),
      this.dom.spawn(null, "TH", "Pan"),
      this.dom.spawn(null, "TH", "Actions"),
    );
    for (const note of this.model.notes) {
      this.appendNotesRow(table, note);
    }
  }
  
  appendNotesRow(table, note) {
    const tr = this.dom.spawn(table, "TR", { "on-input": e => this.onNoteInput(e, note) });
    this.dom.spawn(tr, "TD", this.dom.spawn(null, "DIV", ["advice", "name"], reprGmDrum(note.noteid)));
    this.dom.spawn(tr, "TD", this.dom.spawn(null, "INPUT", { type: "number", name: "noteid", value: note.noteid, min: 0, max: 127 }));
    this.dom.spawn(tr, "TD", this.dom.spawn(null, "INPUT", { type: "number", name: "trimLo", value: note.trimLo, min: 0, max: 255 }));
    this.dom.spawn(tr, "TD", this.dom.spawn(null, "INPUT", { type: "number", name: "trimHi", value: note.trimHi, min: 0, max: 255 }));
    this.dom.spawn(tr, "TD", this.dom.spawn(null, "INPUT", { type: "number", name: "pan", value: note.pan, min: 0, max: 255 }));
    this.dom.spawn(tr, "TD", ["actions"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "Edit...", "on-click": () => this.onEditNote(note) }),
      this.dom.spawn(null, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlayNote(note) }),
      this.dom.spawn(null, "DIV", ["serialLength"], note.serial.length), // good to know, at least which ones are zero-length. Also separates "X" from the other buttons to avoid accidents.
      this.dom.spawn(null, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteNote(note) }),
    );
  }
  
  /* Events.
   ***************************************************************************/
   
  nearestAncestorOfType(element, tagName) {
    for (; element; element=element.parentNode) {
      if (element.tagName === tagName) return element;
    }
    return null;
  }
   
  onNoteInput(event, note) {
    if (!note.hasOwnProperty(event.target.name)) return;
    note[event.target.name] = +event.target.value;
    if (event.target.name === "noteid") {
      const tr = this.nearestAncestorOfType(event.target, "TR");
      const nameElement = tr?.querySelector(".name");
      if (nameElement) {
        nameElement.innerText = reprGmDrum(note.noteid);
      }
    }
  }
  
  // Examine the channel's events and create a note in the model for each event that doesn't have one yet.
  onAddNotes() {
    const table = this.element.querySelector("table.notes");
    for (const event of this.song.events) {
      if (event.chid !== this.channel.chid) continue;
      if (event.type !== "n") continue;
      if (this.model.notes.find(n => n.noteid === event.noteid)) continue;
      const note = {
        noteid: event.noteid,
        trimLo: 0x80,
        trimHi: 0xff,
        pan: 0x80,
        serial: new Uint8Array(0),
      };
      this.model.notes.push(note);
      this.appendNotesRow(table, note);
    }
  }
  
  // Remove notes from model if they aren't referenced by an event.
  onRemoveNotes() {
    const present = new Set();
    for (const event of this.song.events) {
      if (event.chid !== this.channel.chid) continue;
      if (event.type !== "n") continue;
      present.add(event.noteid);
    }
    let rmc = 0;
    for (let i=this.model.notes.length; i-->0; ) {
      const note = this.model.notes[i];
      if (!present.has(note.noteid)) {
        rmc++;
        this.model.notes.splice(i, 1);
      }
    }
    if (rmc) this.rebuildNotesTable();
  }
  
  // Prompt to select drums a la carte from the shared defaults or some other song.
  onImport() {
    const modal = this.dom.spawnModal(DrumsImportModal);
    modal.setup(this.channel, this.song, this.model);
    modal.result.then(rsp => {
      if (!rsp?.length) return;
      for (const note of rsp) {
        let p = this.model.notes.findIndex(n => n.noteid === note.noteid);
        if (p >= 0) {
          // Don't place it here directly, tho we could. I prefer for all the new ones to appear at the end.
          // This removal is for replacing existing notes. It also reduces duplicate noteids from the import to the last one.
          this.model.notes.splice(p, 1);
        }
        this.model.notes.push(note);
      }
      this.rebuildNotesTable();
    });
  }
  
  onSort() {
    this.model.notes.sort((a, b) => a.noteid - b.noteid);
    this.rebuildNotesTable();
  }
  
  onAddOne() {
    for (let noteid=0; noteid<128; noteid++) {
      if (this.model.notes.find(n => n.noteid === noteid)) continue;
      this.model.notes.push({
        noteid,
        trimLo: 0x80,
        trimHi: 0xff,
        pan: 0x80,
        serial: new Uint8Array(0),
      });
      this.rebuildNotesTable();
      return;
    }
    this.dom.modalError(`All 128 notes occupied.`);
  }
  
  onEditNote(note) {
    const p = this.model.notes.indexOf(note);
    if (p < 0) return;
    console.log(`onEditNote ${p}`, note);//TODO
  }
  
  onDeleteNote(note) {
    const p = this.model.notes.indexOf(note);
    if (p < 0) return;
    this.model.notes.splice(p, 1);
    this.rebuildNotesTable();
  }
  
  onPlayNote(note) {
    console.log(`onPlayNote`, note);//TODO
  }
   
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    // (this.model) stays fresh.
    const payload = eauModecfgEncodeDrum(this.model);
    this.resolve(payload);
    this.element.remove();
  }
}
