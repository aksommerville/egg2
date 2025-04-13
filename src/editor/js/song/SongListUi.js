/* SongListUi.js
 */
 
import { SongService } from "./SongService.js";
import { getChannelColor, reprPayload, reprMetaType, reprNoteid } from "./songDisplayBits.js";
import { SongEventModal } from "./SongEventModal.js";
import { Dom } from "../Dom.js";

export class SongListUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
    this.visibleEvents = []; // Contents are owned by this.songService.song.events.
    this.showTooMany = false; // True if user asked us to show events, when there's too many.
    
    this.songServiceListener = this.songService.listen(e => this.onSongServiceEvent(e));
  }
  
  onRemoveFromDom() {
    this.songService.unlisten(this.songServiceListener);
  }
  
  setup() {
    this.element.innerHTML = "";
    
    let trackId=-1, chid=-1;
    if (this.songService.visibilityFilter.track) trackId = +this.songService.visibilityFilter.track;
    if (this.songService.visibilityFilter.channel) chid = +this.songService.visibilityFilter.channel;
    this.visibleEvents = this.songService.song.events.filter(event => {
      if ((trackId >= 0) && (event.trackId !== trackId)) return false;
      if ((chid >= 0) && (event.chid !== chid)) return false;
      switch (this.songService.visibilityFilter.event) {
        case "eauOnly": if ((event.type !== "n") && (event.type !== "w")) return false; break;
        case "midiOnly": if (event.type !== "m") return false; break;
        case "metaOnly": if ((event.type !== "m") || (event.opcode !== 0xff)) return false; break;
        case "noteOnly": if (event.type !== "n") return false; break;
      }
      return true;
    });
    
    /* A song may contain thousands of events, there's really no upper limit.
     * And our default is to show everything.
     * So if we get a crazy high count, require a click from the user before we fill your PC's memory with event-list widgets...
     * This cutoff can be set anywhere, no technical reason for one number or another.
     */
    const TOO_MANY = 500;
    if ((this.visibleEvents.length > TOO_MANY) && !this.showTooMany) {
      this.dom.spawn(this.element, "INPUT", { type: "button", value: `Show ${this.visibleEvents.length} events`, "on-click": () => this.onShowAll() });
      return;
    }
    
    for (const event of this.visibleEvents) {
      const row = this.dom.spawn(this.element, "DIV", ["event"], { "data-eventId": event.id });
      this.populateEventRow(row, event);
    }
  }
  
  populateEventRow(row, event) {
    row.innerHTML = "";
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteEvent(event.id) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "...", "on-click": () => this.onEditEvent(event.id) });
    this.dom.spawn(row, "DIV", ["time"], Math.floor(event.time));
    const chidElement = this.dom.spawn(row, "DIV", ["chid"], event.chid);
    chidElement.style.backgroundColor = getChannelColor(event.chid);
    switch (event.type) {
      case "n": {
          this.dom.spawn(row, "DIV", ["noteid"], reprNoteid(event.noteid));
          this.dom.spawn(row, "DIV", ["velocity"], event.velocity);
          this.dom.spawn(row, "DIV", ["durms"], Math.floor(event.durms));
        } break;
      case "w": {
          this.dom.spawn(row, "DIV", ["wheel"], event.v);
        } break;
      case "m": switch (event.opcode) {
          case 0xff: {
              this.dom.spawn(row, "DIV", ["meta"], reprMetaType(event.a));
              const [str, mode] = reprPayload(event.v);
              switch (mode) {
                case "text": this.dom.spawn(row, "DIV", ["payloadText"], str); break;
                case "hexdump": this.dom.spawn(row, "DIV", ["payload"], str); break;
                case "placeholder": this.dom.spawn(row, "DIV", ["payloadPlaceholder"], str); break;
              }
            } break;
          case 0xf7: case 0xf0: {
              const [str, mode] = reprPayload(event.v);
              switch (mode) {
                case "text": this.dom.spawn(row, "DIV", ["payloadText"], str); break;
                case "hexdump": this.dom.spawn(row, "DIV", ["payload"], str); break;
                case "placeholder": this.dom.spawn(row, "DIV", ["payloadPlaceholder"], str); break;
              }
            } break;
          default: {
              this.dom.spawn(row, "DIV", ["opcode"], "0x" + event.opcode.toString(16).padStart(2, '0'));
              if (event.hasOwnProperty("b")) {
                this.dom.spawn(row, "DIV", ["payload"], `0x${event.a.toString(16).padStart(2, '0')} 0x${event.b.toString(16).padStart(2, '0')}`);
              } else {
                this.dom.spawn(row, "DIV", ["payload"], `0x${event.a.toString(16).padStart(2, '0')}`);
              }
            } break;
        } break;
    }
  }
  
  onSongServiceEvent(event) {
    switch (event.type) {
      case "setup": this.setup(); break;
      case "visibilityFilter": this.showTooMany = false; this.setup(); break;
      case "eventAdded": this.setup(); break; // heavy-handed but meh
    }
  }
  
  onShowAll() {
    if (this.showTooMany) return;
    this.showTooMany = true;
    this.setup();
  }
  
  onDeleteEvent(id) {
    const p = this.songService.song.events.findIndex(e => e.id === id);
    if (p < 0) return;
    this.songService.song.events.splice(p, 1);
    const row = this.element.querySelector(`.event[data-eventId='${id}']`);
    if (row) row.remove();
    this.songService.broadcast({ type: "dirty" });
  }
  
  onEditEvent(id) {
    const event = this.songService.song.events.find(e => e.id === id);
    if (!event) return;
    const modal = this.dom.spawnModal(SongEventModal);
    modal.setup(event, this.songService.song);
    modal.result.then(rsp => {
      if (rsp === null) return;
      rsp.id = event.id;
      const p = this.songService.song.events.findIndex(e => e.id === event.id);
      if (p < 0) return;
      this.songService.song.events[p] = rsp;
      if (rsp.time === event.time) { // Time unchanged, we can do it cheap.
        const row = this.element.querySelector(`.event[data-eventId='${event.id}']`);
        if (row) {
          row.innerHTML = "";
          this.populateEventRow(row, rsp);
        }
      } else { // Time changed. Sort events and rebuild from scratch.
        this.songService.song.sortEvents();
        this.setup();
      }
      this.songService.broadcast({ type: "dirty" });
    });
  }
}
