/* InstrumentsModal.js
 * Shows the built-in instruments and prompts user to pick one.
 */
 
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";

export class InstrumentsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SharedSymbols];
  }
  constructor(element, dom, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.sharedSymbols = sharedSymbols;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.sharedSymbols.getInstruments().then(instruments => {
      this.buildUi(instruments);
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  buildUi(instruments) {
    this.element.innerHTML = "";
    
    /* Split the instruments into 32 buckets.
     * The first 16 are GM, and the last 16 are proposed by Egg.
     * Instruments above bank one mirror into these.
     * Instruments without both name and pid are ignored.
     */
    const buckets = [];
    for (let i=0; i<32; i++) buckets.push([]);
    for (const instrument of instruments.instruments) {
      if (!instrument.name || !instrument.pid) continue;
      const bid = (instrument.pid >> 3) & 0x1f;
      buckets[bid].push(instrument);
    }
    
    /* Add a select for each non-empty bucket.
     * I feel this is more helpful than a single select with 200 instruments in it.
     */
    for (let i=0; i<32; i++) {
      const bucket = buckets[i];
      if (bucket.length < 1) continue;
      const select = this.dom.spawn(this.element, "SELECT", { "on-input": e => this.onSelect(e) });
      this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, InstrumentsModal.BUCKET_NAMES[i]);
      for (const instrument of bucket) {
        this.dom.spawn(select, "OPTION", { value: instrument.pid }, instrument.name || instrument.pid);
      }
      select.value = "";
    }
  }
  
  onSelect(event) {
    const pid = +event.target.value;
    if (isNaN(pid)) return;
    this.sharedSymbols.getInstruments().then(instruments => {
      const instrument = instruments.instruments.find(i => i.pid === pid);
      if (instrument) {
        this.resolve(instrument);
        this.element.remove();
      }
    });
  }
}

InstrumentsModal.BUCKET_NAMES = [
  "Piano", // 0...
  "Chromatic", // 8...
  "Organ", // 16...
  "Guitar", // 24...
  "Bass", // 32...
  "Solo String", // 40...
  "String Ensemble", // 48...
  "Brass", // 56...
  "Solo Reed", // 64...
  "Solo Flute", // 72...
  "Synth Lead", // 80...
  "Synth Pad", // 88...
  "Synth Effects", // 96...
  "World", // 104...
  "Percussion", // 112...
  "Filler", // 120...
  "Drum Kits", // 128...
  "Specialty Drum Kits", // 136...
  "Sound Effects 1", // 144...
  "Sound Effects 2", // 152...
  "Tuned Drums 1", // 160...
  "Tuned Drums 2", // 168...
  "Simple Synth", // 176...
  "Lead Guitar", // 184...
  "Rhythm Guitar", // 192...
  "Plucks", // 200...
  "Noise", // 208...
  "0xd8", // 216...
  "0xe0", // 224...
  "0xe8", // 232...
  "0xf0", // 240...
  "0xf8", // 248...
];
