/* SongChannelsUi.js
 */
 
import { SongService } from "./SongService.js";
import { PidModal } from "./PidModal.js";
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";

export class SongChannelsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService, SharedSymbols];
  }
  constructor(element, dom, songService, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.sharedSymbols = sharedSymbols;
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
  }
  
  setup() {
    this.element.innerHTML = "";
    for (const channel of this.songService.song.channels) {
      if (!channel) continue;
      this.spawnChannelCard(channel);
    }
  }
  
  spawnChannelCard(channel) {
    const card = this.dom.spawn(this.element, "DIV", ["channel"], { "data-chid": channel.chid });
    
    const topRow = this.dom.spawn(card, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "DIV", ["title"], channel.getDisplayName());
    this.dom.spawn(topRow, "DIV", ["spacer"]);
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "Replace...", "on-click": () => this.onReplaceChannel(channel.chid) });
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteChannel(channel.chid) });
    
    this.dom.spawn(card, "DIV", ["slider"],
      this.dom.spawn(null, "SPAN", ["tattle", "trim"], channel.trim.toString().padStart(3)),
      this.dom.spawn(null, "INPUT", { type: "range", name: "trim", min: 0, max: 255, value: channel.trim, "on-input": () => this.onSliderChanged(channel.chid) })
    );
    this.dom.spawn(card, "DIV", ["slider"],
      this.dom.spawn(null, "SPAN", ["tattle", "pan"], channel.pan.toString().padStart(3)),
      this.dom.spawn(null, "INPUT", { type: "range", name: "pan", min: 0, max: 255, value: channel.pan, "on-input": () => this.onSliderChanged(channel.chid) })
    );
    
    //TODO payload
    //TODO post
  }
  
  rebuildChannelCardForChid(chid) {
    const card = this.element.querySelector(`.channel[data-chid='${chid}']`);
    if (!card) return;
    const channel = this.songService.song?.channels[chid];
    if (!channel) {
      card.remove();
      return;
    }
    card.querySelector(".title").innerText = channel.getDisplayName();
    card.querySelector("input[name='trim']").value = channel.trim;
    card.querySelector(".tattle.trim").innerText = channel.trim.toString().padStart(3);
    card.querySelector("input[name='pan']").value = channel.pan;
    card.querySelector(".tattle.pan").innerText = channel.pan.toString().padStart(3);
    //TODO payload
    //TODO post
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
      case "channelsRemoved": this.setup(); break;
      case "channelChanged": this.rebuildChannelCardForChid(event.chid); break;
    }
  }
  
  onReplaceChannel(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    const modal = this.dom.spawnModal(PidModal);
    modal.setup(channel);
    modal.result.then(pid => {
      if (pid === null) return;
      if (!this.sharedSymbols.instruments) return; // Don't poke or wait for it; the modal must have done that already.
      const src = this.sharedSymbols.instruments.channels[pid];
      if (!src) return this.dom.modalError(`pid ${pid} not found`);
      // Don't modify (chid) obviously, also don't touch (trim) or (pan) even though we could.
      // The trims and pans in the shared instrument set are dummies, one is expected to tweak those per channel.
      channel.mode = src.mode;
      channel.payload = new Uint8Array(src.payload);
      channel.post = new Uint8Array(src.post);
      channel.name = src.name; // Debatable.
      this.songService.broadcast({ type: "channelChanged", chid: channel.chid });
    });
  }
  
  onDeleteChannel(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    const events = this.songService.song.events.filter(e => e.chid === chid);
    this.dom.modalPickOne(`Delete ${channel.getDisplayName()} and ${events.length} events?`, ["Yes, delete"]).then(rsp => {
      if (!rsp) return;
      this.songService.song.channels[chid] = null;
      this.songService.song.events = this.songService.song.events.filter(v => v.chid !== chid);
      this.songService.broadcast({ type: "channelsRemoved" });
      this.songService.broadcast({ type: "eventsRemoved" });
    });
  }
  
  onSliderChanged(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    const card = this.element.querySelector(`.channel[data-chid='${chid}']`);
    if (!card) return;
    channel.trim = card.querySelector("input[name='trim']").value;
    card.querySelector(".tattle.trim").innerText = channel.trim.toString().padStart(3);
    channel.pan = card.querySelector("input[name='pan']").value;
    card.querySelector(".tattle.pan").innerText = channel.pan.toString().padStart(3);
    // Don't say "channelChanged" because we would redundantly update our UI. I don't think anyone else listens for it.
    this.songService.broadcast({ type: "dirty" });
  }
}
