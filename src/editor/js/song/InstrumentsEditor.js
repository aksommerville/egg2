/* InstrumentsEditor.js
 * Edits an EAU file like SongEditor, but assumes that there are no events, and than chid == fqpid.
 * For editing the SDK instruments.
 */
 
import { Dom } from "../Dom.js";
import { Song, SongChannel } from "./Song.js";
import { mergeModecfg, decodeDrumModecfg } from "./EauDecoder.js";
import { InstrumentsModal } from "./InstrumentsModal.js";
import { PostModal } from "./PostModal.js";
import { SongChannelsUi } from "./SongChannelsUi.js"; // To borrow some modecfg modal class selection logic.
import { SongService } from "./SongService.js";

export class InstrumentsEditor {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService; // We don't use, really, but some modecfg modals expect it.
    
    this.song = null;
    this.cbDirty = song => {};
  }
  
  setup(song, cbDirty) {
    this.song = song;
    song.dropNoopChannels = true;
    if (cbDirty) this.cbDirty = cbDirty;
    this.songService.reset(this.song, 0);
    this.buildUi();
    this.populateUi();
  }
  
  /* UI.
   *****************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = null;
    this.addTable(0x00);
    this.dom.spawn(this.element, "DIV", ["midline"]);
    this.addTable(0x80);
  }
  
  addTable(fqpid) {
    // I was picturing an actual table with 8 columns but there just isn't room. Do a vertical list instead.
    const container = this.dom.spawn(this.element, "UL", ["bank"]);
    for (let i=128; i-->0; fqpid++) {
      const kfqpid = fqpid;
      
      const row = this.dom.spawn(container, "LI", ["instrument"], { "data-fqpid": fqpid });
      this.dom.spawn(row, "INPUT", { type: "button", value: "=>", "on-click": () => this.onCopyToInstrument(kfqpid) });
      
      const modeSelect = this.dom.spawn(row, "SELECT", { name: "mode", "on-change": () => this.onModeChanged(kfqpid) });
      this.dom.spawn(modeSelect, "OPTION", { value: 0 }, "NOOP");
      this.dom.spawn(modeSelect, "OPTION", { value: 1 }, "TRIVIAL");
      this.dom.spawn(modeSelect, "OPTION", { value: 2 }, "FM");
      this.dom.spawn(modeSelect, "OPTION", { value: 3 }, "SUB");
      this.dom.spawn(modeSelect, "OPTION", { value: 4 }, "DRUM");
      for (let value=5; value<0x100; value++) {
        this.dom.spawn(modeSelect, "OPTION", { value }, value);
      }
      modeSelect.value = 0;
      
      this.dom.spawn(row, "INPUT", { type: "button", value: "Modecfg", "on-click": event => this.onEditModecfg(kfqpid, event) });
      this.dom.spawn(row, "INPUT", { type: "button", value: "Post", "on-click": event => this.onEditPost(kfqpid, event) });
      this.dom.spawn(row, "DIV", ["name"], { "on-click": () => this.onEditName(kfqpid) }, kfqpid);
    }
  }
  
  reprInstrument(fqpid) {
    if (this.song) {
      const name = this.song.getName(fqpid, 0xff);
      if (name) return `${fqpid}: ${name}`;
    }
    return fqpid;
  }
  
  populateUi() {
    if (!this.song) return; // Correct thing would be to blank all the fields, but that case will never arise.
    for (const channel of this.song.channels) this.populateChannel(channel);
  }
  
  populateChannel(channel) {
    const row = this.element.querySelector(`.instrument[data-fqpid='${channel.chid}']`);
    if (!row) return;
    row.querySelector("select[name='mode']").value = channel.mode;
    row.querySelector(".name").innerText = this.reprInstrument(channel.chid);
  }
  
  /* Model.
   **********************************************************************************/
   
  requireChannel(fqpid) {
    if (isNaN(fqpid) || (fqpid < 0) || (fqpid > 0xff)) return null;
    if (!this.song) {
      this.song = new Song();
      this.song.dropNoopChannels = true;
    }
    if ((fqpid < 16) && this.song.channelsByChid[fqpid]) return this.song.channelsByChid[fqpid];
    let channel = this.song.channels.find(c => c.chid === fqpid);
    if (channel) return channel;
    channel = new SongChannel(fqpid);
    this.song.channels.push(channel);
    if (fqpid < 16) this.song.channelsByChid[fqpid] = channel;
    this.cbDirty(this.song);
    return channel;
  }
  
  applyNewDrumNames(channel, fromChid) {
    const drums = decodeDrumModecfg(channel.modecfg);
    if (!drums.length) return;
    this.sdkInstrumentsService.getInstruments().then(instruments => {
      for (const drum of drums) {
        const name = instruments.getName(fromChid, drum.noteid);
        if (!name) continue;
        this.songService.song.setName(channel.chid, drum.noteid, name);
      }
      this.songService.broadcast("dirty");
    });
  }
  
  /* Events.
   * We're largely duplicating SongChannelsUi here, with some different wiring.
   ************************************************************************************/
   
  onModeChanged(fqpid) {
    const channel = this.requireChannel(fqpid);
    if (!channel) return;
    const select = this.element.querySelector(`*[data-fqpid='${fqpid}'] select[name='mode']`);
    if (!select) return;
    const nv = +select.value;
    if (isNaN(nv) || (nv < 0) || (nv > 0xff)) return;
    if (nv === channel.mode) return;
    channel.stash[channel.mode] = channel.modecfg;
    channel.modecfg = mergeModecfg(nv, channel.stash[nv], channel.mode, channel.modecfg);
    channel.mode = nv;
    this.cbDirty(this.song);
  }
   
  onCopyToInstrument(fqpid) {
    // "InstrumentsEditor" and "InstrumentsModal", sorry, I could have thought that through better.
    const channel = this.requireChannel(fqpid);
    if (!channel) return;
    const modal = this.dom.spawnModal(InstrumentsModal);
    modal.result.then(rsp => {
      if (!rsp) return;
      const name = modal.getInstrumentName(rsp);
      channel.overwrite(rsp);
      this.song.removeNoteNames(channel.chid);
      this.song.setName(channel.chid, 0xff, name);
      if (rsp.mode === 4) this.applyNewDrumNames(channel, rsp.chid);
      this.populateChannel(channel);
      this.cbDirty(this.song);
    });
  }
  
  onEditModecfg(fqpid, event) {
    const channel = this.requireChannel(fqpid);
    if (!channel) return;
    const modal = this.dom.spawnModal(SongChannelsUi.getModecfgModalClass(event, channel), [this.songService]);
    modal.setup(channel);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.modecfg = rsp;
      // No need to update ui; we don't report the length or anything.
      this.cbDirty(this.song);
    });
  }
  
  onEditPost(fqpid, event) {
    const channel = this.requireChannel(fqpid);
    if (!channel) return;
    const modal = this.dom.spawnModal(PostModal);
    modal.setup(channel, event.ctrlKey);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.post = rsp;
      // No need to update ui; we don't report the length or anything.
      this.cbDirty(this.song);
    });
  }
  
  onEditName(fqpid) {
    const channel = this.requireChannel(fqpid);
    if (!channel) return;
    this.dom.modalText(`Name for instrument ${channel.chid}:`, this.song.getName(channel.chid, 0xff)).then(rsp => {
      if (typeof(rsp) !== "string") return;
      this.song.setName(channel.chid, 0xff, rsp);
      this.populateChannel(channel);
      this.cbDirty(this.song);
    });
  }
}
