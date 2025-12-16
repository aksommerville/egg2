/* ModecfgModalDrum.js
 * Drum channels are very unlike the other modes.
 */
 
import { Dom } from "../Dom.js";
import { encodeModecfg, decodeModecfg } from "./EauDecoder.js";
import { GM_DRUM_NAMES } from "./MidiConstants.js";
import { SongService } from "./SongService.js";
import { SongEditor } from "./SongEditor.js";
import { PickSoundModal } from "./PickSoundModal.js";
import { MidiService } from "./MidiService.js";
import { Audio } from "../Audio.js";
import { Encoder } from "../Encoder.js";

export class ModecfgModalDrum {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SongService, MidiService, Audio];
  }
  constructor(element, dom, songService, midiService, audio) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.midiService = midiService;
    this.audio = audio;
    
    // All "Modecfg" modals must implement, and resolve with Uint8Array or null.
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.cbSave = null; // (Uint8Array) => void ; Set manually to enable save-without-submit.
    
    this.midiServiceListener = this.midiService.listen(e => this.onMidiEvent(e));
    this.playbackDirty = true;
  }
  
  onRemoveFromDom() {
    this.resolve(null);
    this.midiService.unlisten(this.midiServiceListener);
  }
  
  /* All "Modecfg" modals must implement.
   * If you can allow saving before submit, set cbSave *before* calling this.
   */
  setup(channel) {
    this.mode = channel.mode;
    this.modecfg = channel.modecfg;
    this.chid = channel.chid;
    this.channel = channel;
    this.model = decodeModecfg(this.mode, this.modecfg);
    this.buildUi();
  }
  
  /* UI.
   *******************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    this.element.oninput = () => { this.playbackDirty = true; };
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
    if (this.cbSave) {
      this.dom.spawn(footer, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
    }
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
    this.dom.spawn(row, "DIV", ["length"], drum.serial.length);
  }
  
  drumNameForNoteid(noteid) {
    return this.songService.song.getName(this.chid, noteid) || GM_DRUM_NAMES[noteid] || "";
  }
  
  rowForNoteid(noteid) {
    const input = this.element.querySelector(`input[name='noteid'][value='${noteid}']`);
    let row = input;
    while (row && !row.classList.contains("row")) row = row.parentNode;
    return row;
  }
  
  /* Communication with MIDI bus and synthesizer.
   ********************************************************************/
  
  onMidiEvent(event) {
    switch (event.opcode) {
      case 0x80:
      case 0x90:
      case 0xe0: {
          this.requirePlaybackSerial();
          event.chid = 0; // Don't care about input channel, we've registered on channel zero.
          this.audio.sendEvent(event);
        } break;
    }
  }
  
  requirePlaybackSerial() {
    if (this.playbackDirty) {
      this.playbackDirty = false;
      const modecfg = this.encode();
      const encoder = new Encoder();
      encoder.raw("\0EAU");
      encoder.u16be(500); // tempo, whatever
      encoder.u32be(modecfg.length + 8 + (this.channel.post?.length || 0)); // Channel Headers length
      encoder.u8(0); // Channel zero.
      encoder.u8(0xff); // Trim, full.
      encoder.u8(0x80); // Pan, center.
      encoder.u8(this.mode);
      encoder.u16be(modecfg.length);
      encoder.raw(modecfg);
      if (this.channel.post) {
        encoder.u16be(this.channel.post.length);
        encoder.raw(this.channel.post);
      } else {
        encoder.u16be(0);
      }
      encoder.u32be(1); // Events length
      encoder.u8(0x7f); // Long delay
      this.audio.playEauSong(encoder.finish(), true);
    }
  }
  
  /* Events.
   ********************************************************************/
   
  onDelete(noteid) {
    const p = this.model.drums.findIndex(d => d.noteid === noteid);
    if (p >= 0) {
      this.model.drums.splice(p, 1);
    }
    const row = this.rowForNoteid(noteid);
    if (row) row.remove();
  }
  
  onPlay(drum) {
    this.songService.playSong(drum.serial, 0, drum.trimhi / 255, (drum.pan - 0x80) / 0x80);
  }
  
  onEdit(drum) {
    // DO NOT override SongService here; we explicitly want a new one:
    const modal = this.dom.spawnModal(SongEditor);
    modal.setupSerial(drum.serial, (song) => {
      drum.serial = song.encode();
      const row = this.rowForNoteid(drum.noteid);
      const tattle = row?.querySelector(".length");
      if (tattle) tattle.innerText = drum.serial.length;
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
        if (drum.noteid > highest) highest = drum.noteid;
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
    const modal = this.dom.spawnModal(PickSoundModal);
    modal.setupForDrums(this.model);
    modal.result.then(serial => {
      if (!serial) return;
      // Add or update.
      let row = null;
      let drum = this.model.drums.find(d => d.noteid === modal.noteid);
      if (drum) {
        drum.serial = serial;
        row = this.rowForNoteid(modal.noteid);
      } else {
        drum = { noteid: modal.noteid, trimlo: 0x40, trimhi: 0xff, pan: 0x80, serial };
        this.model.drums.push(drum);
        const scroller = this.element.querySelector(".scroller");
        row = this.dom.spawn(scroller, "DIV", ["row"]);
      }
      // If the modal provides a name, and we don't already have one, use it.
      // But if we already have a name, keep it.
      if (modal.name) {
        const existing = this.songService.song.getName(this.chid, drum.noteid);
        if (!existing) {
          this.songService.song.setName(this.chid, drum.noteid, modal.name);
        }
      }
      if (row) this.populateRow(row, drum);
    }).catch(e => {
      this.dom.modalError(e);
    });
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
  
  encode() {
    // Model stays fresh in real time, for the most part.
    // Do need to send each name to the song; those aren't actually recorded in modecfg.
    for (const row of this.element.querySelectorAll(".scroller > .row")) {
      const noteid = +row.querySelector("input[name='noteid']").value;
      const name = row.querySelector("input[name='name']").value;
      if (!isNaN(noteid) && (noteid >= 0) && (noteid <= 0xff)) {
        this.songService.song.setName(this.chid, noteid, name);
      }
    }
    return encodeModecfg(this.model);
  }
  
  onSubmit() {
    this.resolve(this.encode());
    this.element.remove();
  }
  
  onSave() {
    if (!this.cbSave) return this.dom.modalError("save-without-submit not supported here");
    this.cbSave(this.encode());
  }
}
