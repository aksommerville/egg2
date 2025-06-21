/* SongEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Song } from "./Song.js";
import { SongService } from "./SongService.js";
import { SongToolbar } from "./SongToolbar.js";
import { SongChannelsUi } from "./SongChannelsUi.js";
import { Audio } from "../Audio.js"; // rt

export class SongEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, SongService, Audio, Window];
  }
  constructor(element, dom, data, songService, audio, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.songService = songService;
    this.songService.songEditor = this;
    this.audio = audio;
    this.window = window;
    
    this.res = null;
    this.song = null;
    this.songToolbar = null;
    this.songChannelsUi = null;
    this.detailEditor = null; // SongChartUi or SongListUi
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
    this.audioInterval = this.window.setInterval(() => this.audio.update(), 200);
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
    this.audio.stop();
    if (this.audioInterval) {
      this.window.clearInterval(this.audioInterval);
      this.audioInterval = null;
    }
  }
  
  static checkResource(res) {
    if (res.format === "wav") return 0;
    if (res.type === "song") return 2;
    if (res.type === "sound") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.song = new Song(res.serial);
    this.element.innerHTML = "";
    this.songToolbar = this.dom.spawnController(this.element, SongToolbar, [this.songService]);
    this.songChannelsUi = this.dom.spawnController(this.element, SongChannelsUi, [this.songService]);
    const detailContainer = this.dom.spawn(this.element, "DIV", ["detailContainer"]);
    this.detailEditor = this.dom.spawnController(detailContainer, this.songService.getDetailEditorClass(), [this.songService]);
    this.songService.setup(res, this.song);
  }
  
  replaceDetailEditor() {
    const detailContainer = this.element.querySelector(".detailContainer");
    detailContainer.innerHTML = "";
    this.detailEditor = this.dom.spawnController(detailContainer, this.songService.getDetailEditorClass(), [this.songService]);
    this.detailEditor.setup?.(); // They normally expect a "setup" event.
  }
  
  dirty() {
    if (!this.res || !this.song) return;
    this.data.dirty(this.res.path, () => this.song.encode());
  }
  
  onSongServiceEvent(event) {
    //console.log(`SongEditor.onSongServiceEvent: ${JSON.stringify(event)}`);
    switch (event.type) {
      case "detailEditor": this.replaceDetailEditor(); break;
      case "channelsRemoved":
      case "channelChanged":
      case "channelAdded":
      case "eventsRemoved":
      case "eventsChanged":
      case "eventAdded":
      case "dirty": this.dirty(); break;
    }
  }
}
