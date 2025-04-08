/* SongEventModal.js
 */
 
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { reprNoteid, reprMetaType, reprControlKey } from "./songDisplayBits.js";
import { SongEvent } from "./Song.js";

export class SongEventModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window, SharedSymbols];
  }
  constructor(element, dom, window, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    
    this.event = null; // SongEvent, immutable.
    this.song = null; // Song, optional, for advice.
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* We'll resolve with a new SongEvent, with its own id.
   * Recommend, if you're keeping it, replace id on the new one with the old one's id, then replace it in (song.events).
   * The objects you provide here are strictly read-only to us.
   * (song) is optional. We only use it to look up channel names.
   */
  setup(event, song) {
    this.event = event;
    this.song = song;
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    const table = this.dom.spawn(form, "TABLE");
    
    const typeSelect = this.spawnSelect(table, "Type", "type", [
      ["n", "Note"],
      ["w", "Wheel"],
      ["m", "MIDI"],
      ["", "Other"],
    ]);
    typeSelect.addEventListener("change", () => this.onTypeChanged());
    
    this.spawnInteger(table, "Time", "time", 0, 999999999);
    
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    this.onTypeChanged();
  }
  
  /* Spawn generic table rows.
   ************************************************************************/
  
  spawnSelect(table, label, key, options, describe) {
    const tr = this.dom.spawn(table, "TR", { "data-key": key });
    this.dom.spawn(tr, "TD", ["key"], label);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    const select = this.dom.spawn(tdv, "SELECT", { name: key });
    for (const [value, display] of options) {
      this.dom.spawn(select, "OPTION", { value }, display);
    }
    select.value = this.event[key];
    if (describe) {
      const tdd = this.dom.spawn(tr, "TD", ["advice"], describe(this.event[key]));
      select.addEventListener("change", () => tdd.innerText = describe(+select.value));
    }
    return select;
  }
  
  spawnInteger(table, label, key, min, max, describe) {
    const tr = this.dom.spawn(table, "TR", { "data-key": key });
    this.dom.spawn(tr, "TD", ["key"], label);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    const input = this.dom.spawn(tdv, "INPUT", { type: "number", name: key, min, max, value: this.event[key] });
    if (describe) {
      const tdd = this.dom.spawn(tr, "TD", ["advice"], describe(this.event[key]));
      input.addEventListener("input", () => tdd.innerText = describe(+input.value));
    }
    return input;
  }
  
  spawnHexdump(table, label, key) {
    const tr = this.dom.spawn(table, "TR", { "data-key": key });
    this.dom.spawn(tr, "TD", ["key"], label);
    const tdv = this.dom.spawn(tr, "TD", ["value"]);
    const input = this.dom.spawn(tdv, "INPUT", ["hexdump"], { type: "text", name: key, value: this.reprHexDump(this.event[key]) });
    return input;
  }
  
  /* Spawn specific table content.
   *****************************************************************************************/
  
  spawnNoteUi(table) {
    this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
    this.spawnInteger(table, "Note", "noteid", 0, 127, v => this.describeNote(v));
    this.spawnInteger(table, "Velocity", "velocity", 0, 15, null);
    this.spawnInteger(table, "Duration", "durms", 0, 32768, null);
  }
  
  spawnWheelUi(table) {
    this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
    this.spawnInteger(table, "Wheel", "v", 0, 255, v => this.describeWheel(v));
  }
  
  spawnMidiUi(table) {
    const opcodeSelect = this.spawnSelect(table, "Opcode", "opcode", [
      [0x80, "Note Off"],
      [0x90, "Note On"],
      [0xa0, "Note Adjust"],
      [0xb0, "Control Change"],
      [0xc0, "Program Change"],
      [0xd0, "Channel Pressure"],
      [0xe0, "Wheel"],
      [0xf0, "Sysex F0"],
      [0xf7, "Sysex F7"],
      [0xff, "Meta"],
    ], v => this.describeOpcode(v));
    opcodeSelect.value = this.event.opcode || 0xff; // I reckon Meta is the most useful non-EAU event.
    opcodeSelect.addEventListener("change", () => this.onMidiOpcodeChanged());
    this.spawnMidiSubOpcodeUi(table);
  }
  
  spawnMidiSubOpcodeUi(table) {
    const opcode = +this.element.querySelector("select[name='opcode']").value || 0xff;
    const alreadyRows = Array.from(table.querySelectorAll("tr"));
    this.spawnInteger(table, "Track", "trackId", 0, 999, () => "usually meaningless");
    switch (opcode) {
      case 0x80:
      case 0x90:
      case 0xa0: { // Note Off, Note On, Note Adjust all get the same ui.
          this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
          this.spawnInteger(table, "Note", "a", 0, 127, v => this.describeNote(v));
          this.spawnInteger(table, "Velocity", "b", 0, 127, null); // NB Velocity for MIDI events is 7-bit, not EAU's 4.
        } break;
      case 0xb0: {
          this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
          this.spawnInteger(table, "Key", "a", 0, 127, v => this.describeControl(v));
          this.spawnInteger(table, "Value", "b", 0, 127, null);
        } break;
      case 0xc0: {
          this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
          this.spawnInteger(table, "Program", "a", 0, 127, v => this.describeProgram(v));
        } break;
      case 0xd0: {
          this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
          this.spawnInteger(table, "Pressure", "a", 0, 127, null);
        } break;
      case 0xe0: {
          this.spawnInteger(table, "Channel", "chid", 0, 15, v => this.describeChannel(v));
          // No sense combining the words for better presentation: Real wheel events should not be expressed as type "m".
          this.spawnInteger(table, "LSB", "a", 0, 127, null);
          this.spawnInteger(table, "MSB", "b", 0, 127, null);
        } break;
      case 0xf0:
      case 0xf7: {
          this.spawnHexdump(table, "Payload", "v");
        } break;
      case 0xff: {
          this.spawnInteger(table, "Meta Type", "a", 0, 127, v => this.describeMeta(v));
          this.spawnHexdump(table, "Payload", "v");
        } break;
    }
    for (const row of table.querySelectorAll("tr")) {
      if (alreadyRows.indexOf(row) >= 0) continue;
      row.classList.add("midiSubOpcode");
    }
  }
  
  spawnOtherUi(table) {
    //TODO Not sure what to do about these. Should we fill a <pre> with JSON? I dunno, I don't expect to use "other".
  }
  
  /* Advice (third column).
   *********************************************************************************/
   
  describeNote(v) {
    return reprNoteid(v);
  }
   
  describeChannel(v) {
    if (!this.song) return "";
    if (typeof(v) !== "number") return "";
    const channel = this.song.channels[v];
    if (!channel) return "";
    return channel.getDisplayName();
  }
   
  describeWheel(v) {
    if ((typeof(v) !== "number") || (v < 0) || (v > 255)) return "";
    const norm = Math.max(-1, (v - 0x80) / 0x7f);
    return norm.toString();
  }
  
  describeOpcode(v) {
    switch (v) {
      case 0x80: return "Don't use.";
      case 0x90: return "Use Type=Note instead.";
      case 0xe0: return "Use Type=Wheel instead.";
    }
    return "";
  }
   
  describeControl(v) {
    return reprControlKey(v);
  }
   
  describeProgram(v) {
    // Being very lazy about this. Ask SharedSymbols to load if it's null, but don't take any action when that completes.
    // It may be possible that we fail to describe a Program Change event the first time it comes up, but we'll catch it on the first change.
    if (typeof(v) !== "number") return "";
    if (!this.sharedSymbols.instruments) {
      this.sharedSymbols.getInstruments();
      return "";
    }
    const channel = this.sharedSymbols.instruments.channels[v];
    if (!channel) return "";
    return channel.getDisplayName();
  }
   
  describeMeta(v) {
    return reprMetaType(v);
  }
  
  reprHexDump(v) {
    if (!v?.length || (!(v instanceof Uint8Array))) return "";
    let dst = "";
    for (let i=0; i<v.length; i++) {
      dst += v[i].toString(16).padStart(2, '0');
    }
    return dst;
  }
  
  evalHexDump(v) {
    if (typeof(v) !== "string") throw "Expected string.";
    v = v.replace(/\s/g, "");
    if (v.length & 1) throw "Inappropriate length.";
    if (v.match(/[^0-9a-fA-F]/)) throw "Unexpected character.";
    const dstc = v.length >> 1;
    const dst = new Uint8Array(dstc);
    for (let dstp=0, srcp=0; dstp<dstc; dstp++, srcp+=2) {
      dst[dstp] = parseInt(v.substring(srcp, srcp+2), 16);
    }
    return dst;
  }
  
  /* Read event off UI.
   ********************************************************************************/
   
  eventFromUi() {
    const event = new SongEvent();
    const reviewAfter = {}; // Fields that depend on other context.
    for (const element of this.element.querySelectorAll("*[name]")) {
      const k = element.getAttribute("name");
      const v = element.value;
      if (!k) continue;
      // Pick off the known "other" fields, and everything else is an integer.
      switch (k) {
        
        case "type": { // string
            event.type = v;
          } break;
        
        case "v": {
            // "v" is complicated. What was I thinking...
            // For Wheel events, it's the integer wheel value in 0..255.
            // For Meta and Sysex, it's a hex dump that must be stored as Uint8Array.
            reviewAfter.v = v;
          } break;
        
        default: event[k] = +v || 0;
      }
    }
    if (reviewAfter.v || (typeof(reviewAfter.v) === "number") || (typeof(reviewAfter.v) === "string")) {
      if (event.type === "w") {
        event.v = +reviewAfter.v;
      } else {
        event.v = this.evalHexDump(reviewAfter.v);
      }
    }
    return event;
  }
  
  /* Events.
   **********************************************************************************/
  
  onTypeChanged() {
    const table = this.element.querySelector("table");
    for (const tr of table.querySelectorAll("tr")) {
      const key = tr.getAttribute("data-key");
      if (key === "type") continue;
      if (key === "time") continue;
      tr.remove();
    }
    const type = this.element.querySelector("select[name='type']").value;
    switch (type) {
      case "n": this.spawnNoteUi(table); break;
      case "w": this.spawnWheelUi(table); break;
      case "m": this.spawnMidiUi(table); break;
      case "": this.spawnOtherUi(table); break;
    }
  }
  
  onMidiOpcodeChanged() {
    const table = this.element.querySelector("table");
    for (const tr of table.querySelectorAll("tr.midiSubOpcode")) tr.remove();
    this.spawnMidiSubOpcodeUi(table);
  }
  
  onSubmit(event) {
    event.preventDefault();
    const songEvent = this.eventFromUi();
    this.resolve(songEvent);
    this.element.remove();
  }
}
