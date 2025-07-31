/* SongEventsUi.js
 * Right side of SongEditor, shows all the events.
 * SongEditor must wait until everything's loaded before instantiating me.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";

// If there's so many events to display, require a click first.
// Songs can be arbitrarily complex, there's no general event limit.
const HUGE_LIST_THRESHOLD = 1000;

export class SongEventsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
    this.hugeListConfirmed = false;
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const events = this.filterEvents(this.songService.song, this.songService.visChid);
    if (!events.length) return;
    if ((events.length >= HUGE_LIST_THRESHOLD) && !this.hugeListConfirmed) {
      this.dom.spawn(this.element, "INPUT", {
        type: "button",
        value: `Click to show ${events.length} events`,
        "on-click": () => this.onHugeListConfirm(),
      });
    } else {
      for (const event of events) {
        const row = this.dom.spawn(this.element, "DIV", ["event"], { "data-id": event.id, "on-click": e => this.onEdit(e, event.id) });
        this.populateEventRow(row, event);
      }
    }
  }
  
  filterEvents(song, visChid) {
    if (!song) return [];
    if (visChid === null) return song.events;
    return song.events.filter(e => e.chid === visChid);
  }
  
  populateEventRow(row, event) {
    row.innerHTML = "";
    
    // Action buttons. We don't need "move" buttons because the timestamp controls that, and I don't think breaking ties matters.
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": e => this.onDelete(e, event.id) });
    
    this.dom.spawn(row, "DIV", ["time"], ~~event.time);
    this.dom.spawn(row, "DIV", ["chid", `chid-${event.chid}`], event.chid);
    this.dom.spawn(row, "DIV", ["type", event.type], event.type);
    
    this.dom.spawn(row, "DIV", ["desc"], this.describeEvent(event));
  }
  
  /* (chid,time,type) are already displayed generically.
   * For "noop", "loop", or unknown type, we've nothing more to say.
   */
  describeEvent(event) {
    switch (event.type) {
      case "note": return `0x${event.noteid.toString(16).padStart('0', 2)}(${this.reprNote(event.noteid)}) 0x${event.velocity.toString(16).padStart(2, '0')} ${event.durms}ms`;
      case "wheel": return `${event.wheel}(${this.reprWheel(event.wheel)})`;
    }
    return "";
  }
  
  reprNote(noteid) {
    if ((noteid < 0) || (noteid > 0x7f)) return "!!!";
    // The first octave is -1, and the first note in each octave is C. Not A.
    const octave = Math.floor(noteid / 12) - 1;
    const name = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][noteid % 12];
    return name + octave;
  }
  
  reprWheel(wheel) {
    return ((wheel - 512) / 513).toFixed(3); // -0.999 .. 0.999 or so.
  }
  
  /* Events.
   ***********************************************************************************/
   
  onHugeListConfirm() {
    this.hugeListConfirmed = true;
    this.buildUi();
  }
  
  onEdit(domEvent, id) {
    const songEvent = this.songService.song.events.find(e => e.id === id);
    if (!songEvent) return;
    console.log(`onEdit ${id}`, songEvent);//TODO event modal
  }
  
  onDelete(domEvent, id) {
    domEvent.stopPropagation();
    const p = this.songService.song.events.findIndex(e => e.id === id);
    if (p < 0) return;
    // Debatable: I say delete immediately, but it would be reasonable to ask confirmation I guess.
    this.songService.song.events.splice(p, 1);
    this.songService.broadcast("dirty");
    // Don't broadcast "eventsChanged". We're the only one listening for it, and we don't need to rebuild the whole UI.
    this.element.querySelector(`.event[data-id='${id}']`)?.remove();
  }
  
  onVisibilityChange() {
    this.hugeListConfirmed = false;
    this.buildUi();
  }
  
  onEventsChanged() {
    // Keep hugeListConfirmed if set.
    this.buildUi();
  }
  
  onSongServiceEvent(event) {
    switch (event) {
      case "visibility": this.onVisibilityChange(); break;
      case "eventsChanged": this.onEventsChanged(); break;
    }
  }
}
