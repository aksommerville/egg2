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
import { SharedSymbols } from "../SharedSymbols.js";
 
export class SongService {
  static getDependencies() {
    return [Dom, Data, Window, SharedSymbols];
  }
  constructor(dom, data, window, sharedSymbols) {
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
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
  
  /* If (song) contains any incomplete channels, fetch the default instruments and apply them.
   * Resolves with (song) again after completion, never rejects.
   * Modifies (song) in place.
   * Defaulting deliberately does not mark the song dirty.
   * If the user modifies anything after, it will save with the chosen instruments baked in.
   */
  defaultInstruments(song) {
    if (song.format !== "mid") return Promise.resolve(song); // Only relevant when sourced from MIDI.
    const incompleteChannels = song.channels.filter(c => !c.explicitChdr);
    if (!incompleteChannels.length) return Promise.resolve(song); // All channels voiced properly, nothing for us to do.
    return this.sharedSymbols.getInstruments().then(instruments => {
      for (const channel of incompleteChannels) {
        let src = instruments.channelsByChid[channel.pid]; // Song's pid is instruments' chid.
        if (!src) {
          // No defaulting. It's OK to leave a channel unconfigured. Better than introducing exotic fallback rules.
          console.warn(`No default instrument for pid ${channel.pid}.`);
          continue;
        }
        // (chid,trim,pan) are preserved from the song's channel. Replace (mode,payload,post). If (name) unset, take it from the instrument.
        channel.mode = src.mode;
        channel.payload = src.payload;
        channel.post = src.post;
        if (src.name && !channel.name) channel.name = src.name
      }
      return song;
    });
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
