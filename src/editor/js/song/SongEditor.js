/* SongEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SongService } from "./SongService.js";
import { SongToolbarUi } from "./SongToolbarUi.js";
import { SongChannelsUi } from "./SongChannelsUi.js";
import { SongEventsUi } from "./SongEventsUi.js";

export class SongEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, SongService];
  }
  constructor(element, dom, data, songService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.songService = songService;
    
    this.toolbar = null; // SongToolbarUi
    this.channels = null; // SongChannelsUi
    this.events = null; // SongEventsUi
    this.res = null;
    this.song = null;
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
  }
  
  onRemoveFromDom() {
    if (this.songService.song === this.song) this.songService.song = null;
    this.songService.unlisten(this.songServiceListener);
    this.songService.unload();
  }
  
  static checkResource(res) {
    if (res.type === "song") return 2;
    return 0;
  }
  
  setup(res) {
    this.songService.getSong(res.path).then(song => {
      console.log(`SongEditor got song`, { song, res });
      this.songService.reset(song, res.rid);
      this.res = res;
      this.song = song;
      this.buildUi();
    });
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.toolbar = this.dom.spawnController(this.element, SongToolbarUi);
    const bottom = this.dom.spawn(this.element, "DIV", ["bottom"]);
    this.channels = this.dom.spawnController(bottom, SongChannelsUi);
    this.events = this.dom.spawnController(bottom, SongEventsUi);
  }
  
  onSongServiceEvent(e) {
    console.log(`SongEditor.onSongServiceEvent: ${JSON.stringify(e)}`);
  }
}
