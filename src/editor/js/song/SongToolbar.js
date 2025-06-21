/* SongToolbar.js
 */
 
import { SongService } from "./SongService.js";
import { SongChartUi } from "./SongChartUi.js";
import { SongListUi } from "./SongListUi.js";
import { eauSongEncode } from "./eauSong.js";
import { Dom } from "../Dom.js";
import { Audio } from "../Audio.js"; // rt

export class SongToolbar {
  static getDependencies() {
    return [HTMLElement, Dom, SongService, "nonce", Audio];
  }
  constructor(element, dom, songService, nonce, audio) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.nonce = nonce;
    this.audio = audio;
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
  }
  
  /* UI setup.
   ***********************************************************************************/
  
  setup() {
    this.element.innerHTML = "";
    
    /* Playback controls.
     */
    this.dom.spawn(this.element, "DIV", ["playhead"]);//TODO playhead indicator
    this.dom.spawn(this.element, "INPUT", { type: "button", value: ">", name: "play", "on-click": () => this.onPlay() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "|<", "on-click": () => this.onPlayheadZero() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "!!!", "on-click": () => this.onStop() });
    
    /* Editor class selection.
     */
    const detailClassContainer = this.dom.spawn(this.element, "DIV", ["detailClass"], { "on-change": () => this.onEditorClassChanged() });
    for (const option of this.songService.getDetailEditorIdentifiers()) {
      const id = `SongToolbar-${this.nonce}-detailClass-${option}`;
      this.dom.spawn(detailClassContainer, "INPUT", ["toggle"], { type: "radio", name: `detailClass-${this.nonce}`, value: option, id });
      this.dom.spawn(detailClassContainer, "LABEL", ["toggle"], { for: id }, option);
    }
    const detailClass = this.songService.getDetailEditorIdentifier();
    detailClassContainer.querySelector(`input[value='${detailClass}']`).checked = true;
    
    /* Visibility filters.
     */
    const trackFilter = this.dom.spawn(this.element, "SELECT", ["trackFilter"], { "on-change": e => this.onFilterChanged(e) },
      this.dom.spawn(null, "OPTION", { value: "", selected: "selected" }, "All tracks")
    );
    for (const trackId of this.songService.song.getTrackIds()) {
      this.dom.spawn(trackFilter, "OPTION", { value: trackId }, `Track ${trackId}`);
    }
    trackFilter.value = this.songService.visibilityFilter.track;
    const channelFilter = this.dom.spawn(this.element, "SELECT", ["channelFilter"], { "on-change": e => this.onFilterChanged(e) },
      this.dom.spawn(null, "OPTION", { value: "", selected: "selected" }, "All channels")
    );
    for (const chid of this.songService.song.getChids()) {
      this.dom.spawn(channelFilter, "OPTION", { value: chid }, `Channel ${chid}`);
    }
    channelFilter.value = this.songService.visibilityFilter.channel;
    const eventFilter = this.dom.spawn(this.element, "SELECT", ["eventFilter"], { "on-change": e => this.onFilterChanged(e) },
      this.dom.spawn(null, "OPTION", { value: "", selected: "selected" }, "All events"),
      this.dom.spawn(null, "OPTION", { value: "eauOnly" }, "EAU only"),
      this.dom.spawn(null, "OPTION", { value: "midiOnly" }, "MIDI only"),
      this.dom.spawn(null, "OPTION", { value: "metaOnly" }, "Meta only"),
      this.dom.spawn(null, "OPTION", { value: "noteOnly" }, "Note only"),
    );
    eventFilter.value = this.songService.visibilityFilter.event;
    
    /* Actions.
     */
    const actionsMenu = this.dom.spawn(this.element, "SELECT", { "on-change": e => this.onAction(e) },
      this.dom.spawn(null, "OPTION", { value: "", disabled: "disabled", selected: "selected" }, "Actions...")
    );
    for (const action of this.songService.listActions()) {
      this.dom.spawn(actionsMenu, "OPTION", { value: action.value }, action.label);
    }
  }
  
  /* Events.
   **********************************************************************************/
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
      // Not listening for "visibilityFilter" because we're the only ones that change it.
    }
  }
  
  onPlay() {
    const serial = eauSongEncode(this.songService.song);
    this.audio.start();
    this.audio.playEauSong(serial, this.songService.res.rid);
  }
  
  onPlayheadZero() {
    this.audio.egg_song_set_playhead(0);
  }
  
  onStop() {
    this.audio.stop();
  }
  
  onEditorClassChanged() {
    const name = this.element.querySelector(".detailClass input:checked")?.value;
    if (!name) return;
    this.songService.setDetailEditorIdentifier(name);
  }
  
  onFilterChanged(event) {
    const trackFilter = this.element.querySelector(".trackFilter")?.value || "";
    const channelFilter = this.element.querySelector(".channelFilter")?.value || "";
    const eventFilter = this.element.querySelector(".eventFilter")?.value || "";
    this.songService.setVisibilityFilter(trackFilter, channelFilter, eventFilter);
  }
  
  onAction(event) {
    const name = event.target.value;
    event.target.value = "";
    this.songService.performAction(name);
  }
}
