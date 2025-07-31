/* SongChannelsUi.js
 * Left side of SongEditor, shows all the channels.
 * SongEditor must wait until everything's loaded before instantiating me.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";
import { ModecfgModal } from "./ModecfgModal.js";
import { PostModal } from "./PostModal.js";

export class SongChannelsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
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
    this.dom.spawn(top, "DIV", ["name"], `Channel ${channel.chid}`);
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
    
    const modeSelect = this.dom.spawn(card, "SELECT", { name: "mode", "on-change": e => this.onModeChange(e, channel) });
    this.dom.spawn(modeSelect, "OPTION", { value: 0 }, "0: NOOP");
    this.dom.spawn(modeSelect, "OPTION", { value: 1 }, "1: DRUM");
    this.dom.spawn(modeSelect, "OPTION", { value: 2 }, "2: FM");
    this.dom.spawn(modeSelect, "OPTION", { value: 3 }, "3: HARSH");
    this.dom.spawn(modeSelect, "OPTION", { value: 4 }, "4: HARM");
    for (let i=5; i<256; i++) this.dom.spawn(modeSelect, "OPTION", { value: i }, i);
    modeSelect.value = channel.mode;
    
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": e => this.onEditModecfg(e, channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.modecfg.length} bytes modecfg`)
    );
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": e => this.onEditPost(e, channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.post.length} bytes post`)
    );
    this.dom.spawn(card, "DIV", ["advice"], "ctl-click to edit raw");
  }
  
  /* Events.
   ***************************************************************************************/
  
  onStore(channel) {
    console.log(`onStore`, channel);//TODO present modal with the SDK's instrument set
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
    channel.mode = mode;
    if (channel.stash[mode]) {
      channel.modecfg = channel.stash[mode];
      //TODO Should we try to transfer common things like level envelope?
    } else {
      channel.modecfg = [];
      //TODO Default config per mode. Empty is always legal but I think we can do better.
    }
    this.songService.broadcast("dirty");
  }
  
  onEditModecfg(event, channel) {
    const modal = this.dom.spawnModal(ModecfgModal);
    modal.setup(event.ctrlKey ? 0 : channel.mode, channel.modecfg, channel.chid); // mode zero forces raw presentation
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.modecfg = rsp;
      this.populateCard(this.element.querySelector(`.channel.chid-${channel.chid}`), channel);
      this.songService.broadcast("dirty");
    });
  }
  
  onEditPost(event, channel) {
    const modal = this.dom.spawnModal(PostModal);
    modal.setup(channel.post, event.ctrlKey);
    modal.result.then(rsp => {
      if (!rsp) return;
      channel.post = rsp;
      this.populateCard(this.element.querySelector(`.channel.chid-${channel.chid}`), channel);
      this.songService.broadcast("dirty");
    });
  }
  
  onSongServiceEvent(event) {
    switch (event) {
      case "channelSetChanged": this.buildUi(); break;
    }
  }
}
