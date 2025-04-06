/* SongToolbar.js
 */
 
import { SongService } from "./SongService.js";
import { SongChartUi } from "./SongChartUi.js";
import { SongListUi } from "./SongListUi.js";
import { Dom } from "../Dom.js";

export class SongToolbar {
  static getDependencies() {
    return [HTMLElement, Dom, SongService, "nonce"];
  }
  constructor(element, dom, songService, nonce) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.nonce = nonce;
    
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
    this.dom.spawn(this.element, "INPUT", { type: "button", value: ">", name: "playPause", "on-click": () => this.onPlayPause() });
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
  
  onPlayPause() {
    console.log(`TODO SongToolbar.onPlayPause`);
  }
  
  onPlayheadZero() {
    console.log(`TODO SongToolbar.onPlayheadZero`);
  }
  
  onStop() {
    console.log(`TODO SongToolbar.onStop`);
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
