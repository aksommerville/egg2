/* SongListUi.js
 */
 
import { SongService } from "./SongService.js";
import { Dom } from "../Dom.js";

export class SongListUi {
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
    this.dom.spawn(this.element, "DIV", "TODO SongListUi");
    for (const event of this.songService.song.events) {
      if (event.chid !== 1) continue;
      if (event.type !== "n") continue;
      this.dom.spawn(this.element, "DIV", `${event.time} ${event.chid} ${event.type} ${event.noteid}`);
    }
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
    }
  }
}
