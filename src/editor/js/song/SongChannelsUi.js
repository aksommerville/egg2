/* SongChannelsUi.js
 * Left side of SongEditor, shows all the channels.
 * SongEditor must wait until everything's loaded before instantiating me.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";

export class SongChannelsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
    this.buildUi();
  }
  
  buildUi() {
    this.innerHTML = "";
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
      this.dom.spawn(null, "INPUT", { type: "range", value: channel.trim, min: 0, max: 255, name: "trim", "on-input": e => this.onTrimChange(e, channel) })
    );
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "DIV", ["tattle"], channel.pan),
      this.dom.spawn(null, "INPUT", { type: "range", value: channel.pan, min: 0, max: 255, name: "pan", "on-input": e => this.onPanChange(e, channel) })
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
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditModecfg(channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.modecfg.length} bytes modecfg`)
    );
    this.dom.spawn(card, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditPost(channel) }),
      this.dom.spawn(null, "DIV", ["sublabel"], `${channel.post.length} bytes post`)
    );
  }
  
  onStore(channel) {
    console.log(`onStore`, channel);
  }
  
  onDelete(channel) {
    console.log(`onDelete`, channel);
  }
  
  onTrimChange(event, channel) {
    console.log(`onTrimChange ${channel.chid}=${event.target.value}`);
    event.target.parentNode.querySelector(".tattle").innerText = event.target.value;
  }
  
  onPanChange(event, channel) {
    console.log(`onPanChange ${channel.chid}=${event.target.value}`);
    event.target.parentNode.querySelector(".tattle").innerText = event.target.value;
  }
  
  onModeChange(event, channel) {
    console.log(`onModeChange ${channel.chid}=${event.target.value}`);
  }
  
  onEditModecfg(channel) {
    console.log(`onEditModecfg`, channel);
  }
  
  onEditPost(channel) {
    console.log(`onEditPost`, channel);
  }
}
