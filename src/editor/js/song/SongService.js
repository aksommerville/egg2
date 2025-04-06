/* SongService.js
 * Shared context for a SongEditor and its children.
 * NB We are not a singleton.
 */
 
import { Song, SongChannel, SongEvent } from "./Song.js";
import { SongChartUi } from "./SongChartUi.js";
import { SongListUi } from "./SongListUi.js";
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
 
export class SongService {
  static getDependencies() {
    return [Dom, Data, Window];
  }
  constructor(dom, data, window) {
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.songEditor = null; // Owner assigns after construction.
    
    this.nextListenerId = 1;
    this.listeners = [];
    
    this.detailEditorIdentifier = "chart";
    this.visibilityFilter = {
      track: "", // Empty string or trackId as string.
      channel: "", // Empty string or chid as string.
      event: "", // "", "eauOnly", "midiOnly", "metaOnly", "noteOnly"
    };
    //TODO load settings from localStorage
  }
  
  setup(res, song) {
    this.res = res;
    this.song = song;
    this.broadcast({ type: "setup" });
  }
  
  /* Detail editor class.
   * Clients shouldn't need to know about SongChartUi and SongListUi, only about the common interface they implement.
   **************************************************************************************/
  
  getDetailEditorClass() {
    switch (this.detailEditorIdentifier) {
      case "chart": return SongChartUi;
      case "list": return SongListUi;
    }
    return SongListUi; // Must return something valid.
  }
  
  getDetailEditorIdentifiers() {
    return ["chart", "list"];
  }
  
  getDetailEditorIdentifier() {
    return this.detailEditorIdentifier;
  }
  
  setDetailEditorIdentifier(name) {
    if (name === this.detailEditorIdentifier) return;
    if (!this.getDetailEditorIdentifiers().includes(name)) return;
    this.detailEditorIdentifier = name;
    this.broadcast({ type: "detailEditor" });
  }
  
  /* Visibility filter.
   ******************************************************************************/
   
  setVisibilityFilter(track, channel, event) {
    if (
      (track === this.visibilityFilter.track) &&
      (channel === this.visibilityFilter.channel) &&
      (event === this.visibilityFilter.event)
    ) return;
    this.visibilityFilter = { track, channel, event };
    this.broadcast({ type: "visibilityFilter" });
  }
  
  /* Event dispatch.
   *   { type:"setup" }
   *   { type:"detailEditor" }
   *   { type:"visibilityFilter" }
   *   { type:"dirty" }
   *   { type:"channelsRemoved" }
   *   { type:"eventsRemoved" }
   *   { type:"channelChanged", chid }
   *************************************************************************************/
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  broadcast(e) {
    for (const { cb } of this.listeners) cb(e);
  }
  
  /* Generic global actions.
   ********************************************************************************/
   
  listActions() {
    return [
      { value: "tempo", label: "Tempo..." },
      { value: "autoStartTime", label: "Auto start time" },
      { value: "autoEndTime", label: "Auto end time" },
      { value: "removeUnusedEvents", label: "Remove unused events" },
      { value: "reassignChannels", label: "Reassign channels..." },
    ];
  }
  
  performAction(value) {
    const fnname = `action_${value}`;
    this[fnname]?.();
  }
  
  action_tempo() {
    console.log(`TODO SongService.action_tempo`);
  }
  
  action_autoStartTime() {
    console.log(`TODO SongService.action_autoStartTime`);
  }
  
  action_autoEndTime() {
    console.log(`TODO SongService.action_autoEndTime`);
  }
  
  action_removeUnusedEvents() {
    console.log(`TODO SongService.action_removeUnusedEvents`);
  }
  
  action_reassignChannels() {
    console.log(`TODO SongService.action_reassignChannels`);
  }
}

SongService.singleton = false;
