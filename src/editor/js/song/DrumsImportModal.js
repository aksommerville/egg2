/* DrumsImportModal.js
 * Let the user select a song or the built-in instruments, then one drum channel within that.
 * Pick drums a la carte from that source channel for importing to the existing drum channel.
 * We allow changing noteid during the import, any other tweaks must be done after.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { Song, SongChannel } from "./Song.js";
//TODO import { eauModecfgDecodeDrum, eauModecfgEncodeDrum } from "./eauSong.js";
import { reprGmDrum } from "./songDisplayBits.js";

export class DrumsImportModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window, SharedSymbols, Data];
  }
  constructor(element, dom, window, sharedSymbols, data) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    this.data = data;
    
    this.instruments = null; // Song, from SharedSymbols (read-only)
    this.channel = null; // SongChannel (read-only)
    this.song = null; // Song, the one being edited (read-only)
    this.model = null; // {notes:{noteid,trimLo,trimHi,pan,serial}[]}
    this.source = null; // Song, the one we're reading from. (read-only)
    this.sourceChannel = null; // SongChannel, read-only, from (source).
    this.sourceModel = null; // Same shape as (this.model), from (this.sourceChannel.payload).
    this.targetNotesUsed = new Set();
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* We resolve with an array in the shape of (model.notes) ie {noteid,trimLo,trimHi,pan,serial},
   * for notes to add to the channel.
   */
  setup(channel, song, model) {
    this.sharedSymbols.getInstruments().then(instruments => this.setupSync(instruments, channel, song, model));
  }
  
  /* UI
   *************************************************************************************************/
  
  setupSync(instruments, channel, song, model) {
    this.instruments = instruments;
    this.channel = channel;
    this.song = song;
    this.model = model;
    this.acquireTargetNotesUsed();
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => {
        e.preventDefault();
        e.stopPropagation();
      },
    });
    const table = this.dom.spawn(form, "TABLE");
    let tr;
    
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Source");
      const tdv = this.dom.spawn(tr, "TD");
      const select = this.dom.spawn(tdv, "SELECT", { name: "source", "on-change": e => this.onSourceChanged(e) });
      this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, "Pick one...");
      this.dom.spawn(select, "OPTION", { value: "builtin" }, "Built-in instruments");
      for (const { type, rid, serial, name, path } of this.data.resv) {
        if (type === "sound") {
          this.dom.spawn(select, "OPTION", { value: path }, `sound:${rid} ${name}`);
        } else if (type === "song") {
          this.dom.spawn(select, "OPTION", { value: path }, `song:${rid} ${name}`);
        }
      }
      select.value = "";
    }
    
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Channel");
      const tdv = this.dom.spawn(tr, "TD");
      const select = this.dom.spawn(tdv, "SELECT", { name: "channel", "on-change": e => this.onChannelChanged(e) });
      this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, "Pick source first");
      select.disabled = true;
      select.value = "";
    }
    
    this.dom.spawn(form, "DIV", ["subui"]);
    
    this.dom.spawn(form, "DIV", ["bottomRow"],
      this.dom.spawn(null, "INPUT", { type: "Submit", value: "OK", "on-click": e => this.onSubmit(e) }),
      this.dom.spawn(null, "DIV", ["advice", "submitAdvice"]),
    );
    this.refreshSubmitAdvice();
  }
  
  rebuildSubUi() {
    const container = this.element.querySelector(".subui");
    container.innerHTML = "";
    if (!this.sourceModel) return;
    for (const note of this.sourceModel.notes) {
      const row = this.dom.spawn(container, "DIV", ["row", "sourceNote"], { "data-sourceNoteid": note.noteid });
      const existing = this.model.notes.find(n => n.noteid === note.noteid);
      if (existing && !existing.serial.length) row.classList.add("recommend");
      else if (!existing && this.targetNotesUsed.has(note.noteid)) row.classList.add("recommend");
      this.dom.spawn(row, "DIV", ["sourceNoteId"], `${note.noteid} ${reprGmDrum(note.noteid)}`);
      this.dom.spawn(row, "INPUT", { type: "button", value: "=", "on-click": e => this.onTargetIdentity(e, note.noteid) });
      this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": e => this.onTargetClear(e, note.noteid) });
      this.dom.spawn(row, "INPUT", { name: `target-${note.noteid}`, type: "number", min: -1, max: 127, value: -1, "on-input": e => this.onTargetChanged(e, note.noteid) });
      this.dom.spawn(row, "DIV", ["advice", "targetAdvice"]);
    }
  }
  
  /* Model.
   *************************************************************************************/
  
  generateResponse() {
    const output = [];
    if (this.sourceModel) {
      for (const row of this.element.querySelectorAll(".row[data-sourceNoteid]")) {
        const dstNoteid = +row.querySelector("input[type='number']").value;
        if (isNaN(dstNoteid) || (dstNoteid < 0) || (dstNoteid > 0x7f)) continue;
        const srcNoteid = +row.getAttribute("data-sourceNoteid");
        const note = this.sourceModel.notes.find(n => n.noteid === srcNoteid);
        if (!note) continue;
        output.push({
          ...note,
          noteid: dstNoteid,
          serial: new Uint8Array(note.serial), // just to be on the safe side
        });
      }
    }
    return output;
  }
  
  acquireTargetNotesUsed() {
    this.targetNotesUsed.clear();
    if (this.song?.events && this.channel) {
      for (const event of this.song.events) {
        if (event.chid !== this.channel.chid) continue;
        if (event.type !== "n") continue;
        this.targetNotesUsed.add(event.noteid);
      }
    }
  }
  
  refreshSubmitAdvice() {
    const element = this.element.querySelector(".submitAdvice");
    if (this.sourceModel && this.model) {
      const dstNoteidAlready = new Set();
      let addc=0, replacec=0, replemptyc=0;
      for (const row of this.element.querySelectorAll(".row[data-sourceNoteid]")) {
        const dstNoteid = +row.querySelector("input[type='number']").value;
        if (isNaN(dstNoteid) || (dstNoteid < 0) || (dstNoteid > 0x7f)) continue;
        if (dstNoteidAlready.has(dstNoteid)) {
          element.innerText = `!!! Conflict !!! Multiple source for destination note ${dstNoteid}.`;
          return;
        }
        dstNoteidAlready.add(dstNoteid);
        const oldNote = this.model.notes.find(n => n.noteid === dstNoteid);
        if (!oldNote) addc++;
        else if (!oldNote.serial.length) replemptyc++;
        else replacec++;
      }
      if (addc || replacec || replemptyc) {
        if (replacec) { // This is the dangerous case that we want to warn about.
          element.innerText = `! Will replace ${replacec} drums ! (and ${replemptyc} empty replaces, and ${addc} adds)`;
        } else {
          element.innerText = `Will add ${addc} and replace ${replemptyc} empties.`;
        }
        return;
      }
    }
    element.innerText = "No changes.";
  }
  
  loadSource(sourceid) {
    if (!sourceid) {
      this.source = null;
    } else if (sourceid === "builtin") {
      this.source = this.instruments;
    } else {
      const res = this.data.resv.find(r => r.path === sourceid);
      if (res) {
        this.source = new Song(res.serial);
      } else {
        this.source = null;
      }
    }
  }
  
  /* Call after channel select changes.
   * Sets or clears (this.sourceChannel) and (this.sourceModel).
   */
  reacquireSourceChannel() {
    this.sourceChannel = null;
    this.sourceModel = null;
    if (!this.source) return;
    const chid = +this.element.querySelector("select[name='channel']").value;
    if (!this.source.channels[chid]) return;
    this.sourceChannel = this.source.channels[chid];
    try {
      this.sourceModel = eauModecfgDecodeDrum(this.sourceChannel.payload);
    } catch (e) {
      this.sourceChannel = null;
      this.sourceModel = null;
      this.dom.modalError(e);
    }
  }
  
  /* Events.
   **************************************************************************************/
  
  onSourceChanged(event) {
    const sourceid = event.target.value;
    try {
      this.loadSource(sourceid);
      const channelsMenu = this.element.querySelector("select[name='channel']");
      channelsMenu.innerHTML = "";
      channelsMenu.disabled = false;
      let chanc = 0;
      for (const channel of this.source?.channels || []) {
        if (channel?.mode !== 1) continue; // Channels are sparse, and also don't show the non-drum ones.
        this.dom.spawn(channelsMenu, "OPTION", { value: channel.chid }, channel.getDisplayName());
        chanc++;
      }
      if (!chanc) {
        this.dom.spawn(channelsMenu, "OPTION", { value: "", disabled: "disabled" }, "No drum channels");
        channelsMenu.value = "";
        channelsMenu.disabled = true;
      }
      this.reacquireSourceChannel();
      this.rebuildSubUi();
    } catch (e) {
      this.dom.modalError(e);
    }
  }
  
  onChannelChanged(event) {
    this.reacquireSourceChannel();
    this.rebuildSubUi();
  }
  
  onTargetChanged(event, noteid) {
    if (!this.model) return;
    const row = this.element.querySelector(`.row[data-sourceNoteid='${noteid}']`);
    if (!row) return;
    const advice = row.querySelector(".targetAdvice");
    if (advice) {
      const dstNoteid = +event.target.value;
      if (isNaN(dstNoteid) || (dstNoteid < 0) || (dstNoteid > 0x7f)) {
        advice.innerText = "";
      } else {
        const oldNote = this.model.notes.find(n => n.noteid === dstNoteid);
        if (oldNote?.serial.length) advice.innerText = `Overwrites existing drum with ${oldNote.serial.length} bytes payload.`;
        else if (oldNote) advice.innerText = "Overwrites existing empty drum.";
        else if (this.targetNotesUsed.has(dstNoteid)) advice.innerText = "New drum, events exist.";
        else advice.innerText = "New drum, no events.";
      }
    }
    this.refreshSubmitAdvice();
  }
  
  onTargetIdentity(event, noteid) {
    if (!this.model) return;
    const input = this.element.querySelector(`.row[data-sourceNoteid='${noteid}'] input[type='number']`);
    if (!input) return;
    input.value = noteid;
    this.onTargetChanged({ target: input }, noteid);
  }
  
  onTargetClear(event, noteid) {
    if (!this.model) return;
    const input = this.element.querySelector(`.row[data-sourceNoteid='${noteid}'] input[type='number']`);
    if (!input) return;
    input.value = -1;
    this.onTargetChanged({ target: input }, noteid);
  }
  
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    this.resolve(this.generateResponse());
    this.element.remove();
  }
}
