/* InstrumentsModal.js
 * Shows the built-in instruments and prompts user to pick one.
 */
 
import { Dom } from "../Dom.js";
import { SdkInstrumentsService } from "./SdkInstrumentsService.js";
import { Data } from "../Data.js";
import { Song } from "./Song.js";

export class InstrumentsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SdkInstrumentsService, Data];
  }
  constructor(element, dom, sdkInstrumentsService, data) {
    this.element = element;
    this.dom = dom;
    this.sdkInstrumentsService = sdkInstrumentsService;
    this.data = data;
    
    this.instruments = null;
    this.localSources = []; // {name,instruments:{name,instrument}[]}
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.acquireLocalSources();
    this.sdkInstrumentsService.getInstruments().then(instruments => {
      this.instruments = instruments;
      this.buildUi();
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  acquireLocalSources() {
    this.localSources = [];
    this.unid = 1;
    for (const res of this.data.resv) {
      try {
        if (res.type === "song") {
          const name = res.path.replace(/^.*\/([^\/]*)$/, "$1");
          let song = null;
          if (name.endsWith(".mid")) song = Song.fromMidiWithoutEvents(res.serial);
          else song = Song.withoutEvents(res.serial);
          if (!song) continue;
          const source = { name, instruments: [] };
          this.localSources.push(source);
          for (const channel of song.channels) {
            const chname = song.getNameForce(channel.chid, 0xff);
            source.instruments.push({
              unid: this.unid++,
              name: chname,
              instrument: channel,
            });
          }
        }
      } catch (e) {
        console.warn(e);
      }
    }
    this.localSources = this.localSources.filter(s => s.instruments.length).sort((a, b) => {
      const aid = parseInt(a.name);
      const bid = parseInt(b.name);
      return aid - bid;
    });
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const left = this.dom.spawn(this.element, "DIV");
    const right = this.dom.spawn(this.element, "DIV");
    
    // SDK Instruments on the left, if we have any.
    if (this.instruments) {
    
      /* Split the instruments into 32 buckets.
       * The first 16 are GM, and the last 16 are proposed by Egg.
       * Instruments above bank one mirror into these.
       */
      const buckets = [];
      for (let i=0; i<32; i++) buckets.push([]);
      for (const instrument of this.instruments.channels) {
        const bid = (instrument.chid >> 3) & 0x1f;
        buckets[bid].push(instrument);
      }
    
      /* Add a select for each non-empty bucket.
       * I feel this is more helpful than a single select with 200 instruments in it.
       */
      this.dom.spawn(left, "DIV", ["columnHeader"], "SDK Instruments");
      for (let i=0; i<32; i++) {
        const bucket = buckets[i];
        if (bucket.length < 1) continue;
        const select = this.dom.spawn(left, "SELECT", { "on-input": e => this.onSelect(e) });
        this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, InstrumentsModal.BUCKET_NAMES[i]);
        for (const instrument of bucket) {
          this.dom.spawn(select, "OPTION", { value: instrument.chid }, this.instruments.getNameForce(instrument.chid, 0xff));
        }
        select.value = "";
      }
    }
    
    // Songs in current project on the right.
    this.dom.spawn(right, "DIV", ["columnHeader"], "This Project");
    for (const source of this.localSources) {
      const select = this.dom.spawn(right, "SELECT", { "on-input": e => this.onSelectLocal(e, source) });
      this.dom.spawn(select, "OPTION", { value: "", disabled: "disabled" }, source.name);
      for (const instrument of source.instruments) {
        this.dom.spawn(select, "OPTION", { value: instrument.unid }, instrument.name);
      }
      select.value = "";
    }
  }
  
  onSelect(event) {
    const chid = +event.target.value;
    if (isNaN(chid)) return;
    const instrument = this.instruments.channels.find(i => i.chid === chid);
    if (instrument) {
      this.resolve(instrument);
      this.element.remove();
    }
  }
  
  onSelectLocal(event, source) {
    const unid = +event.target.value;
    const instrument = source.instruments.find(i => i.unid === unid);
    if (!instrument) return;
    this.resolve(instrument.instrument);
    this.element.remove();
  }
  
  getInstrumentName(instrument) {
    if (!instrument || !this.instruments) return "";
    return this.instruments.getName(instrument.chid, 0xff);
  }
}

InstrumentsModal.BUCKET_NAMES = [
  // The first 16 come from General MIDI. Don't change.
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
  // The second half of buckets are ours to define.
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
