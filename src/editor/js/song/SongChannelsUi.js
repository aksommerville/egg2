/* SongChannelsUi.js
 * Left side of SongEditor, shows all the channels.
 * SongEditor must wait until everything's loaded before instantiating me.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";
import { PostModal } from "./PostModal.js";
import { SdkInstrumentsService } from "./SdkInstrumentsService.js";
import { InstrumentsModal } from "./InstrumentsModal.js";
import { decodeDrumModecfg, mergeModecfg } from "./EauDecoder.js";
import { ModecfgModalRaw } from "./ModecfgModalRaw.js";
import { ModecfgModalTrivial } from "./ModecfgModalTrivial.js";
import { ModecfgModalFm } from "./ModecfgModalFm.js";
import { ModecfgModalSub } from "./ModecfgModalSub.js";
import { ModecfgModalDrum } from "./ModecfgModalDrum.js";

export class SongChannelsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService, SdkInstrumentsService, "nonce"];
  }
  constructor(element, dom, songService, sdkInstrumentsService, nonce) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.sdkInstrumentsService = sdkInstrumentsService;
    this.nonce = nonce;
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
  }
  
  /* UI.
   ***************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const song = this.songService.song;
    if (!song) return;
    for (const channel of song.channels) {
      const card = this.dom.spawn(this.element, "DIV", ["channel", `chid-${channel.chid}`], { "data-chid": channel.chid });
      this.populateCard(card, channel);
    }
  }
  
  populateCard(card, channel) {
    card.innerHTML = "";
    
    const top = this.dom.spawn(card, "DIV", ["row"]);
    this.dom.spawn(top, "DIV", ["name"], { "on-click": () => this.onEditName(channel) }, this.songService.song.getNameForce(channel.chid, 0xff));
    this.dom.spawn(top, "DIV", ["spacer"]);
    this.dom.spawn(top, "INPUT", { type: "button", value: "Store...", "on-click": () => this.onStore(channel) });
    this.dom.spawn(top, "INPUT", { type: "button", value: "X", "on-click": () => this.onDelete(channel) });
    
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "DIV", ["tattle"], channel.trim),
      this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 255, value: channel.trim, name: "trim", "on-input": e => this.onTrimChange(e, channel) })
    );
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "DIV", ["tattle"], channel.pan),
      this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: 255, value: channel.pan, name: "pan", "on-input": e => this.onPanChange(e, channel) })
    );
    
    const bottom = this.dom.spawn(card, "DIV", ["row"]);
    
    const playback = this.dom.spawn(bottom, "DIV", ["col", "playback"], { "on-change": e => this.onPlaybackControlsChanged(e) });
    const muteInput = this.dom.spawn(playback, "INPUT", ["toggle"], { id: `SongChannelUi-${this.nonce}-${channel.chid}-mute`, type: "checkbox" });
    this.dom.spawn(playback, "LABEL", ["toggle"], { for: `SongChannelUi-${this.nonce}-${channel.chid}-mute` }, "M");
    const soloInput = this.dom.spawn(playback, "INPUT", ["toggle"], { id: `SongChannelUi-${this.nonce}-${channel.chid}-solo`, type: "checkbox" });
    this.dom.spawn(playback, "LABEL", ["toggle"], { for: `SongChannelUi-${this.nonce}-${channel.chid}-solo` }, "S");
    const postInput = this.dom.spawn(playback, "INPUT", ["toggle"], { id: `SongChannelUi-${this.nonce}-${channel.chid}-post`, type: "checkbox" });
    this.dom.spawn(playback, "LABEL", ["toggle"], { for: postInput.id }, "P");
    muteInput.checked = this.songService.getPlaybackControl("mute", channel.chid);
    soloInput.checked = this.songService.getPlaybackControl("solo", channel.chid);
    postInput.checked = this.songService.getPlaybackControl("post", channel.chid);
    
    const voicing = this.dom.spawn(bottom, "DIV", ["col"]);
    
    const modeSelect = this.dom.spawn(voicing, "SELECT", { name: "mode", "on-change": e => this.onModeChange(e, channel) });
    this.dom.spawn(modeSelect, "OPTION", { value: 0 }, "0: NOOP");
    this.dom.spawn(modeSelect, "OPTION", { value: 1 }, "1: TRIVIAL");
    this.dom.spawn(modeSelect, "OPTION", { value: 2 }, "2: FM");
    this.dom.spawn(modeSelect, "OPTION", { value: 3 }, "3: SUB");
    this.dom.spawn(modeSelect, "OPTION", { value: 4 }, "4: DRUM");
    for (let i=6; i<256; i++) this.dom.spawn(modeSelect, "OPTION", { value: i }, i);
    modeSelect.value = channel.mode;
    
    this.dom.spawn(voicing, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": e => this.onEditModecfg(e, channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.modecfg.length} bytes modecfg`)
    );
    this.dom.spawn(voicing, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": e => this.onEditPost(e, channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.post.length} bytes post`)
    );
    this.dom.spawn(voicing, "DIV", ["advice"], "ctl-click to edit raw");
  }
  
  /* Events.
   ***************************************************************************************/
   
  onEditName(channel) {
    this.dom.modalText(`Name for channel ${channel.chid}:`, this.songService.song.getName(channel.chid, 0xff)).then(rsp => {
      if (typeof(rsp) !== "string") return;
      this.songService.song.setName(channel.chid, 0xff, rsp);
      const element = this.element.querySelector(`.channel[data-chid='${channel.chid}'] .name`);
      if (element) element.innerText = this.songService.song.getNameForce(channel.chid, 0xff); // Don't use (rsp); there's some defaulting and decoration here.
      this.songService.broadcast("dirty");
    });
  }
  
  onStore(channel) {
    const modal = this.dom.spawnModal(InstrumentsModal);
    modal.result.then(rsp => {
      if (!rsp) return;
      const name = modal.getInstrumentName(rsp);
      channel.overwrite(rsp);
      this.songService.song.removeNoteNames(channel.chid);
      this.songService.song.setName(channel.chid, 0xff, name);
      if (rsp.mode === 4) this.applyNewDrumNames(channel, rsp.chid);
      this.songService.broadcast("dirty");
      this.songService.broadcast("channelSetChanged");
    }).catch(e => this.dom.modalErrror(e));
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
  
  onDelete(channel) {
    const eventc = this.songService.song.countEventsForChid(channel.chid);
    this.dom.modalPickOne(`Delete channel ${channel.chid} and ${eventc} events?`, ["Delete", "Cancel"]).then(choice => {
      if (choice !== "Delete") return;
      const p = this.songService.song.channels.indexOf(channel);
      if (p >= 0) this.songService.song.channels.splice(p, 1);
      this.songService.song.channelsByChid[channel.chid] = null;
      for (let i=this.songService.song.events.length; i-->0; ) {
        const event = this.songService.song.events[i];
        if (event.chid === channel.chid) this.songService.song.events.splice(i, 1);
      }
      this.songService.broadcast("dirty");
      this.songService.broadcast("channelSetChanged");
      this.songService.broadcast("eventsChanged");
    });
  }
  
  onTrimChange(event, channel) {
    const trim = Math.max(0, Math.min(255, ~~event.target.value));
    if (trim === channel.trim) return;
    event.target.parentNode.querySelector(".tattle").innerText = event.target.value;
    channel.trim = trim;
    this.songService.broadcast("dirty");
  }
  
  onPanChange(event, channel) {
    const pan = Math.max(0, Math.min(255, ~~event.target.value));
    if (pan === channel.pan) return;
    event.target.parentNode.querySelector(".tattle").innerText = event.target.value;
    channel.pan = pan;
    this.songService.broadcast("dirty");
  }
  
  onModeChange(event, channel) {
    const mode = +event.target.value;
    if ((typeof(mode) !== "number") || (mode < 0) || (mode > 0xff)) return;
    if (mode === channel.mode) return;
    channel.stash[channel.mode] = channel.modecfg;
    channel.modecfg = mergeModecfg(mode, channel.stash[mode], channel.mode, channel.modecfg);
    channel.mode = mode;
    this.populateCard(this.element.querySelector(`.channel.chid-${channel.chid}`), channel);
    this.songService.broadcast("dirty");
  }
  
  onEditModecfg(event, channel) {
    const modal = this.dom.spawnModal(SongChannelsUi.getModecfgModalClass(event, channel), [this.songService]);
    modal.setup(channel);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.modecfg = rsp;
      this.populateCard(this.element.querySelector(`.channel.chid-${channel.chid}`), channel);
      this.songService.broadcast("dirty");
    });
  }
  
  // static so InstrumentsEditor can share it.
  static getModecfgModalClass(event, channel) {
    if (event.ctrlKey) return ModecfgModalRaw;
    switch (channel.mode) {
      case 0: return ModecfgModalRaw; // NOOP
      case 1: return ModecfgModalTrivial;
      case 2: return ModecfgModalFm;
      case 3: return ModecfgModalSub;
      case 4: return ModecfgModalDrum;
    }
    return ModecfgModalRaw;
  }
  
  onEditPost(event, channel) {
    const modal = this.dom.spawnModal(PostModal);
    modal.setup(channel, event.ctrlKey);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.post = rsp;
      this.populateCard(this.element.querySelector(`.channel.chid-${channel.chid}`), channel);
      this.songService.broadcast("dirty");
    });
  }
  
  onPlaybackControlsChanged(event) {
    const input = event.target;
    const splits = (input?.id || "").split('-');
    if (splits.length < 4) return;
    const chid = +splits[2];
    const name = splits[3];
    this.songService.setPlaybackControl(name, chid, input.checked);
  }
  
  onSongServiceEvent(event) {
    switch (event) {
      case "channelSetChanged": this.buildUi(); break;
    }
  }
}
