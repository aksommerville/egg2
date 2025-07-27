/* SongChannelsUi.js
 */
 
import { SongService } from "./SongService.js";
import { PidModal } from "./PidModal.js";
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { getChannelColor } from "./songDisplayBits.js";
//import { eauPostForEach, eauPostDecode, eauPostEncode, EAU_POST_STAGE_NAMES, eauPostDefaultBody } from "./eauSong.js";
import { ModecfgGenericModal } from "./ModecfgGenericModal.js";
import { ModecfgDrumModal } from "./ModecfgDrumModal.js";
import { ModecfgFmModal } from "./ModecfgFmModal.js";
import { ModecfgSubModal } from "./ModecfgSubModal.js";
import { SongPostTypeModal } from "./SongPostTypeModal.js";
import { SongPostBodyModal } from "./SongPostBodyModal.js";

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
    card.style.backgroundColor = getChannelColor(channel.chid);
    
    const topRow = this.dom.spawn(card, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "DIV", ["title"], { "on-click": () => this.onClickName(channel.chid) }, channel.getDisplayName());
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
    
    let modeSelect;
    this.dom.spawn(card, "DIV", ["modeRow"],
      modeSelect = this.dom.spawn(null, "SELECT", { name: "mode", "on-change": e => this.onModeChanged(channel.chid, e) },
        this.dom.spawn(null, "OPTION", { value: "0" }, "noop"),
        this.dom.spawn(null, "OPTION", { value: "1" }, "drum"),
        this.dom.spawn(null, "OPTION", { value: "2" }, "fm"),
        this.dom.spawn(null, "OPTION", { value: "3" }, "sub"),
      ),
      this.dom.spawn(null, "INPUT", { type: "button", value: "Edit config...", "on-click": () => this.onEditModeConfig(channel.chid) })
    );
    for (let i=4; i<256; i++) this.dom.spawn(modeSelect, "OPTION", { value: i }, i);
    modeSelect.value = channel.mode;
    
    const post = this.dom.spawn(card, "DIV", ["post"]);
    this.populatePostUi(post, channel);
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
    card.querySelector("select[name='mode']").value = channel.mode;
    const post = card.querySelector(".post");
    post.innerHTML = "";
    this.populatePostUi(post, channel);
  }
  
  populatePostUi(parent, channel) {
    /*TODO
    eauPostForEach(channel.post, (stageid, body, p) => {
      const pill = this.dom.spawn(parent, "DIV", ["stage"]);
      this.dom.spawn(pill, "INPUT", ["delete"], { type: "button", value: "X", "on-click": () => this.onDeletePostStage(channel.chid, p) });
      this.dom.spawn(pill, "DIV", ["title"], EAU_POST_STAGE_NAMES[stageid] || stageid.toString());
      this.dom.spawn(pill, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditPostStage(channel.chid, p) });
    });
    this.dom.spawn(parent, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddPostStage(channel.chid) });
    /**/
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
      case "channelsRemoved": this.setup(); break;
      case "channelAdded": this.setup(); break;
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
  
  onClickName(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    const card = this.element.querySelector(`.channel[data-chid='${chid}']`);
    if (!card) return;
    this.dom.modalText(`Name for channel ${chid}:`, channel.name || "").then(rsp => {
      if (rsp === null) return;
      channel.name = rsp;
      card.querySelector(".title").innerText = channel.getDisplayName();
      if (this.songService.song.replaceEventsForChannelName(chid)) {
        this.songService.broadcast({ type: "eventsChanged" });
      } else {
        this.songService.broadcast({ type: "dirty" });
      }
    });
  }
  
  onModeChanged(chid, event) {
    const v = +event?.target?.value;
    if (isNaN(v) || (v < 0) || (v > 0xff)) return;
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    if (channel.mode === v) return;
    channel.changeMode(v);
    this.songService.broadcast({ type: "dirty" });
  }
  
  onEditModeConfig(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    let modalcls;
    switch (channel.mode) {
      case 1: modalcls = ModecfgDrumModal; break;
      case 2: modalcls = ModecfgFmModal; break;
      case 3: modalcls = ModecfgSubModal; break;
      default: modalcls = ModecfgGenericModal; break;
    }
    const modal = this.dom.spawnModal(modalcls);
    modal.setup(channel, this.songService.song);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.payload = rsp;
      this.songService.broadcast({ type: "channelChanged", chid: channel.chid });
    });
  }
  
  onDeletePostStage(chid, p) {
    const channel = this.songService.song?.channels[chid];
    if (!channel) return;
    const stages = [];
    let got = false;
    /*TODO
    eauPostForEach(channel.post, (stageid, body, sp) => {
      if (sp === p) {
        got = true;
      } else {
        stages.push({ stageid, body });
      }
    });
    if (!got) return;
    channel.post = eauPostEncode(stages);
    this.songService.broadcast({ type: "channelChanged", chid });
    /**/
  }
  
  onEditPostStage(chid, p) {
    const channel = this.songService.song?.channels[chid];
    if (!channel) return;
    /*TODO
    const stages = eauPostDecode(channel.post);
    const stage = stages.find(s => (s.p === p));
    if (!stage) return;
    const modal = this.dom.spawnModal(SongPostBodyModal);
    modal.setup(stage.stageid, stage.body);
    modal.result.then(rsp => {
      if (!rsp) return;
      stage.body = rsp;
      delete stage.p;
      channel.post = eauPostEncode(stages);
      this.songService.broadcast({ type: "channelChanged", chid });
    });
    /**/
  }
  
  onAddPostStage(chid) {
    const channel = this.songService.song?.channels[chid];
    if (!channel) return;
    /*TODO
    const modal = this.dom.spawnModal(SongPostTypeModal);
    modal.result.then(stageid => {
      if (typeof(stageid) !== "number") return;
      const p = channel.post.length;
      const stages = eauPostDecode(channel.post);
      stages.push({ stageid, body: eauPostDefaultBody(stageid) });
      channel.post = eauPostEncode(stages);
      this.songService.broadcast({ type: "channelChanged", chid });
      this.onEditPostStage(chid, p);
    });
    /**/
  }
}
