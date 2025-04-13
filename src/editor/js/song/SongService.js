/* SongService.js
 * Shared context for a SongEditor and its children.
 * NB We are not a singleton.
 */
 
import { Song, SongChannel, SongEvent } from "./Song.js";
import { SongChartUi } from "./SongChartUi.js";
import { SongListUi } from "./SongListUi.js";
import { SongEventModal } from "./SongEventModal.js";
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
    
    this.detailEditorIdentifier = "list";
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
   *   { type:"channelAdded" }
   *   { type:"eventsRemoved" }
   *   { type:"eventsChanged" }
   *   { type:"eventAdded" }
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
      { value: "addChannel", label: "Add Channel" },
      { value: "addEvent", label: "Add Event..." },
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
  
  action_addChannel() {
    if (!this.song) return;
    const chid = this.song.unusedChid();
    if (chid < 0) return this.dom.spawnModal("All channels in use.");
    const channel = new SongChannel(chid);
    this.song.channels[chid] = channel;
    this.broadcast({ type: "channelAdded" });
  }
  
  action_addEvent() {
    if (!this.song) return;
    const modal = this.dom.spawnModal(SongEventModal);
    modal.setup({
      time: 0,
      type: "n",
      chid: 0, // TODO Maybe change if there's a visibility filter, or use the lowest chid known to exist...
      noteid: 0x40,
      velocity: 7,
      durms: 0,
    }, this.song);
    modal.result.then(rsp => {
      if (!rsp) return;
      console.log(`event to add`, rsp);
      if ((rsp.chid >= 0) && !this.song.channels[rsp.chid]) {
        // Implicitly add the channel, since we don't need any more detail to do so.
        this.song.channels[rsp.chid] = new SongChannel(rsp.chid);
        this.broadcast({ type: "channelAdded" });
      }
      this.song.events.push(rsp);
      this.song.sortEvents();
      this.broadcast({ type: "eventAdded" });
    });
  }
}

SongService.singleton = false;
