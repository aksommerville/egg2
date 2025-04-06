/* SongChannelsUi.js
 */
 
import { SongService } from "./SongService.js";
import { PidModal } from "./PidModal.js";
import { Dom } from "../Dom.js";

export class SongChannelsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
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
    const card = this.dom.spawn(this.element, "DIV", ["channel"]);
    const topRow = this.dom.spawn(card, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "DIV", ["title"], channel.getDisplayName());
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "Replace...", "on-click": () => this.onReplaceChannel(channel.chid) });
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
    }
  }
  
  onReplaceChannel(chid) {
    const channel = this.songService.song.channels[chid];
    if (!channel) return;
    const modal = this.dom.spawnModal(PidModal);
    modal.setup(channel);
    modal.result.then(pid => {
      if (pid === null) return;
      console.log(`...ok replace channel ${chid} with pid ${pid}`);
    });
  }
}
