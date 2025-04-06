/* SongChartUi.js
 */
 
import { SongService } from "./SongService.js";
import { Dom } from "../Dom.js";

export class SongChartUi {
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
    this.element.innerText = "TODO SongChartUi";
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
    }
  }
}
