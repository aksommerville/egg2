/* SongEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SongService } from "./SongService.js";
import { SongToolbarUi } from "./SongToolbarUi.js";
import { SongChannelsUi } from "./SongChannelsUi.js";
import { SongEventsUi } from "./SongEventsUi.js";
import { Song } from "./Song.js";

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
    if (res.type === "sound") return 2;
    return 0;
  }
  
  setup(res) {
    this.songService.getSong(res.path).then(song => {
      this.songService.reset(song, res.rid);
      this.res = res;
      this.song = song;
      this.buildUi();
    });
  }
  
  /* For modals.
   * We won't dirty the resource, because it isn't one.
   * Instead, we call (cbDirty(song)) on every dirty.
   */
  setupSerial(serial, cbDirty) {
    this.element.classList.add("shorter");
    this.cbDirty = cbDirty;
    const song = new Song(serial);
    this.songService.reset(song, 0);
    this.res = null;
    this.song = song;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    this.toolbar = this.dom.spawnController(this.element, SongToolbarUi, [this.songService]);
    const bottom = this.dom.spawn(this.element, "DIV", ["bottom"]);
    this.channels = this.dom.spawnController(bottom, SongChannelsUi, [this.songService]);
    this.events = this.dom.spawnController(bottom, SongEventsUi, [this.songService]);
  }
  
  onSongServiceEvent(e) {
    //console.log(`SongEditor.onSongServiceEvent: ${JSON.stringify(e)}`);
    switch (e) {
      case "dirty": {
          if (this.res) { // Normal top-level case.
            this.songService.dirtySong(this.res.path, this.song);
          } else if (this.cbDirty) { // Instantiated with serial, modal case.
            this.cbDirty(this.song);
          }
        } break;
    }
  }
}
