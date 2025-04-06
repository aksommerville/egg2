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
    //for (let i=0; i<200; i++) this.dom.spawn(this.element, "DIV", `Long list of events: ${i}`);
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
    }
  }
}
