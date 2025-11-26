/* PostModal.js
 * Edit an entire post pipe.
 */
 
import { Dom } from "../Dom.js";
import { Encoder } from "../Encoder.js";
import { EauDecoder, encodePostStage, decodePostStage } from "./EauDecoder.js";
import { MidiService } from "./MidiService.js";
import { Audio } from "../Audio.js";

export class PostModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MidiService, Audio];
  }
  constructor(element, dom, midiService, audio) {
    this.element = element;
    this.dom = dom;
    this.midiService = midiService;
    this.audio = audio;
    
    this.post = null; // Post, see below.
    this.serial = null;
    this.raw = false;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.midiServiceListener = this.midiService.listen(e => this.onMidiEvent(e));
    this.playbackDirty = true;
  }
  
  onRemoveFromDom() {
    this.resolve(null);
    this.midiService.unlisten(this.midiServiceListener);
  }
  
  setup(serial, raw, mode, modecfg) {
    this.serial = serial;
    this.post = new Post(serial);
    this.raw = raw;
    this.mode = mode;
    this.modecfg = modecfg;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": event => {
        event.preventDefault();
        event.stopPropagation();
      },
      "on-input": () => { this.playbackDirty = true; },
    });
    if (this.raw) {
      const textarea = this.dom.spawn(form, "TEXTAREA", { name: "raw", "on-input": e => this.onRawChange(e) });
      textarea.value = this.hexdump(this.serial);
    } else {
      for (const stage of this.post.stages) {
        this.spawnStage(form, stage);
      }
      this.dom.spawn(form, "INPUT", { type: "button", value: "+", "on-click": e => this.onAddStage(e) });
    }
    this.dom.spawn(form, "DIV", ["spacer"]);
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  spawnStage(parent, stage) {
    const row = this.dom.spawn(parent, "DIV", ["stage"], { "data-stage-id": stage.id });
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": e => this.onDeleteStage(stage.id) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "^", "on-click": e => this.onMoveStage(stage.id, -1) });
    this.dom.spawn(row, "INPUT", { type: "button" , value: "v", "on-click": e => this.onMoveStage(stage.id, 1) });
    
    const stageidSelect = this.dom.spawn(row, "SELECT", { name: "stageid", "on-input": e => this.onStageIdChange(stage.id, +e.target.value) });
    this.dom.spawn(stageidSelect, "OPTION", { value: 0 }, "NOOP");
    this.dom.spawn(stageidSelect, "OPTION", { value: 1 }, "GAIN");
    this.dom.spawn(stageidSelect, "OPTION", { value: 2 }, "DELAY");
    this.dom.spawn(stageidSelect, "OPTION", { value: 3 }, "TREMOLO");
    this.dom.spawn(stageidSelect, "OPTION", { value: 4 }, "DETUNE");
    this.dom.spawn(stageidSelect, "OPTION", { value: 5 }, "WAVESHAPER");
    this.dom.spawn(stageidSelect, "OPTION", { value: 6 }, "LOPASS (X)");
    this.dom.spawn(stageidSelect, "OPTION", { value: 7 }, "HIPASS (X)");
    this.dom.spawn(stageidSelect, "OPTION", { value: 8 }, "BPASS (X)");
    this.dom.spawn(stageidSelect, "OPTION", { value: 9 }, "NOTCH (X)");
    for (let i=10; i<256; i++) {
      this.dom.spawn(stageidSelect, "OPTION", { value: i }, i);
    }
    stageidSelect.value = stage.stageid;
    
    const payload = this.dom.spawn(row, "DIV", ["payload"]);
    this.populateStageRow(stage);
  }
  
  populateStageRow(stage) {
    const element = this.element.querySelector(`.stage[data-stage-id='${stage.id}'] > .payload`);
    if (!element) {
      console.error(`PostModal: payload container for stage ${stage.id} not found`);
      return;
    }
    element.innerHTML = "";
    switch (stage.stageid) {
      case 0x00: break; // NOOP
      case 0x01: this.populateStageRowGain(element, stage); break; // GAIN
      case 0x02: this.populateStageDelay(element, stage); break; // DELAY
      case 0x03: case 0x04: this.populateStagePeriodic(element, stage); break; // TREMOLO,DETUNE
      case 0x05: this.populateStageRowWaveshaper(element, stage); break; // WAVESHAPER
      case 0x06: case 0x07: case 0x08: case 0x09: this.populateStageRowFilter(element, stage); break; // LOPASS,HIPASS,BPASS,NOTCH
      default: this.populateStageRowGeneric(element, stage); break;
    }
  }
  
  stageRowFieldNumber(element, id, name, value, params) {
    if (!params) params = {};
    return this.dom.spawn(element, "INPUT", { type: "number", name, title: name, value, "on-input": e => this.onPayloadNumberInput(id, name, e.target.value) });
  }
  
  populateStageRowGain(element, stage) {
    this.stageRowFieldNumber(element, stage.id, "gain", stage.gain, { min: 0, max: 256, step: 1/256 });
    this.stageRowFieldNumber(element, stage.id, "clip", stage.clip, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "gate", stage.gate, { min: 0, max: 255 });
  }
  
  populateStageRowWaveshaper(element, stage) {
    //TODO Would be cool to show a visual waveshaper config, drag bars up and down or something.
    // Or maybe "gain" and "curve" sliders, and you can watch the bars move.
    this.dom.spawn(element, "INPUT", { type: "text", name: "extra", title: "u16 coeffients", value: this.hexdump(stage.extra), "on-input": e => this.onPayloadHexInput(stage.id, "extra", e.target.value) });
  }
  
  populateStageDelay(element, stage) {
    this.stageRowFieldNumber(element, stage.id, "period", stage.period, { min: 0, max: 256, step: 1/256 });
    this.stageRowFieldNumber(element, stage.id, "dry", stage.dry, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "wet", stage.wet, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "store", stage.store, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "feedback", stage.feedback, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "sparkle", stage.sparkle, { min: 0, max: 255 });
  }
  
  // TREMOLO,DETUNE. Not that they're the same thing, just they have very similar parameters.
  populateStagePeriodic(element, stage) {
    this.stageRowFieldNumber(element, stage.id, "period", stage.period, { min: 0, max: 256, step: 1/256 });
    if (stage.hasOwnProperty("mix")) this.stageRowFieldNumber(element, stage.id, "mix", stage.mix, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "depth", stage.depth, { min: 0, max: 255 });
    this.stageRowFieldNumber(element, stage.id, "phase", stage.phase, { min: 0, max: 255 });
    if (stage.hasOwnProperty("sparkle")) this.stageRowFieldNumber(element, stage.id, "sparkle", stage.sparkle, { min: 0, max: 255 });
    if (stage.hasOwnProperty("rightphase")) this.stageRowFieldNumber(element, stage.id, "rightphase", stage.rightphase, { min: 0, max: 255 });
  }
  
  // LOPASS,HIPASS,BPASS,NOTCH
  populateStageRowFilter(element, stage) {
    this.stageRowFieldNumber(element, stage.id, "mid", stage.mid, { min: 0, max: 0xffff });
    if (stage.hasOwnProperty("width")) this.stageRowFieldNumber(element, stage.id, "width", stage.width, { min: 0, max: 0xffff });
  }
  
  /* Plain number inputs, or a hexdump for (extra).
   * Suitable for all stages, if we keep the key list up to date.
   * Ideal for none.
   */
  populateStageRowGeneric(element, stage) {
    const discardKeys = ["id", "stageid"];
    const keysOrder = [
      "period", "mix", "depth", "phase", "rightphase",
      "dry", "wet", "store", "feedback", "sparkle",
      "gain", "clip", "gate",
      "mid", "width",
      "extra",
    ];
    const keys = Object.keys(stage).filter(k => !discardKeys.includes(k));
    keys.sort((a, b) => {
      return keysOrder.indexOf(a) - keysOrder.indexOf(b);
    });
    for (let p=0; (p<keys.length) && !keysOrder.includes(keys[p]); p++) {
      console.error(`Please add key ${JSON.stringify(keys[p])} to keysOrder in PostModal.populateStageRow (stageid ${stage.stageid})`);
    }
    for (const k of keys) {
      // [title] produces a tooltip
      if (k === "extra") {
        const input = this.dom.spawn(element, "INPUT", { type: "text", name: k, title: k, value: this.hexdump(stage[k]), "on-input": e => this.onPayloadHexInput(stage.id, k, e.target.value) });
      } else {
        const input = this.dom.spawn(element, "INPUT", { type: "number", name: k, title: k, value: stage[k], "on-input": e => this.onPayloadNumberInput(stage.id, k, e.target.value) });
      }
    }
  }
  
  hexdump(src) {
    if (!src) return "";
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += "0123456789abcdef"[src[i] >> 4];
      dst += "0123456789abcdef"[src[i] & 15];
      dst += " ";
    }
    return dst;
  }
  
  unhexdump(src) {
    src = src.replace(/[^0-9a-fA-F]+/g, "");
    const dst = new Uint8Array(src.length >> 1);
    for (let i=0; i<src.length; i+=2) {
      dst[i >> 1] = parseInt(src.substring(i, i + 2), 16);
    }
    return dst;
  }
  
  onRawChange(event) {
    // Don't bother evaluating it until they submit.
  }
  
  onAddStage(event) {
    this.post.addStage();
    this.buildUi();
  }
  
  onDeleteStage(id) {
    const p = this.post.stages.findIndex(s => s.id === id);
    if (p < 0) return;
    this.post.stages.splice(p, 1);
    this.buildUi();
  }
  
  onMoveStage(id, d) {
    if (this.post.stages.length < 2) return;
    const p = this.post.stages.findIndex(s => s.id === id);
    if (p < 0) return;
    const stage = this.post.stages[p];
    this.post.stages.splice(p, 1);
    const np = Math.max(0, Math.min(this.post.stages.length, p + d));
    this.post.stages.splice(np, 0, stage);
    this.buildUi();
  }
  
  onStageIdChange(id, stageid) {
    const p = this.post.stages.findIndex(s => s.id === id);
    if (p < 0) return;
    const oldStage = this.post.stages[p];
    if (oldStage.stageid === stageid) return;
    // Lean on EauDecoder to produce the default model for this stageid.
    const newStage = decodePostStage(new EauDecoder(new Uint8Array([stageid])));
    newStage.id = oldStage.id;
    this.post.stages[p] = newStage;
    this.populateStageRow(newStage);
  }
  
  onPayloadHexInput(id, k, v) {
    v = this.unhexdump(v);
    const stage = this.post.stages.find(s => s.id === id);
    if (!stage) return;
    stage[k] = v;
  }
  
  onPayloadNumberInput(id, k, v) {
    v = +v;
    if (isNaN(v)) return;
    const stage = this.post.stages.find(s => s.id === id);
    if (!stage) return;
    stage[k] = v;
  }
  
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    this.resolve(this.serialFromUi());
    this.element.remove();
  }
  
  serialFromUi() {
    if (this.raw) {
      return this.unhexdump(this.element.querySelector("textarea[name='raw']").value);
    } else {
      return this.post.encode();
    }
  }
  
  /* Communication with MIDI bus and synthesizer.
   ********************************************************************/
  
  onMidiEvent(event) {
    switch (event.opcode) {
      case 0x80:
      case 0x90:
      case 0xe0: {
          this.requirePlaybackSerial();
          event.chid = 0; // Don't care about input channel, we've registered on channel zero.
          this.audio.sendEvent(event);
        } break;
    }
  }
  
  requirePlaybackSerial() {
    if (this.playbackDirty) {
      this.playbackDirty = false;
      const post = this.serialFromUi();
      const encoder = new Encoder();
      encoder.raw("\0EAU");
      encoder.u16be(500); // tempo, whatever
      encoder.u32be(this.modecfg.length + 8 + post.length); // Channel Headers length
      encoder.u8(0); // Channel zero.
      encoder.u8(0x80); // Trim.
      encoder.u8(0x80); // Pan.
      encoder.u8(this.mode);
      encoder.u16be(this.modecfg.length);
      encoder.raw(this.modecfg);
      encoder.u16be(post.length);
      encoder.raw(post);
      encoder.u32be(1); // Events length
      encoder.u8(0x7f); // Long delay
      this.audio.playEauSong(encoder.finish(), true);
    }
  }
}

export class Post {
  constructor(src) {
    this._init();
    if (!src) ;
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Unexpected input to Post`);
  }
  
  _init() {
    if (!Post.nextStageId) Post.nextStageId = 1;
    this.stages = []; // { id, stageid, extra, ...more fields per EauDecoder.js:decodePostStage... }
  }
  
  _decode(src) {
    const decoder = new EauDecoder(src);
    while (!decoder.finished()) {
      const stage = decodePostStage(decoder);
      stage.id = Post.nextStageId++;
      this.stages.push(stage);
    }
  }
  
  addStage() {
    const stage = {
      id: Post.nextStageId++,
      stageid: 0,
      extra: new Uint8Array(0),
    };
    this.stages.push(stage);
  }
  
  encode() {
    const dst = new Encoder();
    for (const stage of this.stages) encodePostStage(dst, stage);
    return dst.finish();
  }
}
