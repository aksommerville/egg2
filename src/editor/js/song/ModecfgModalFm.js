/* ModecfgModalFm.js
 * "FM" covers pretty much every tuned instrument. They don't necessarily involve actual FM.
 * So this is going to be more involved than most modecfg modals.
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { EnvUi } from "./EnvUi.js";
import { WaveUi } from "./WaveUi.js";
import { MidiService } from "./MidiService.js";
import { Encoder } from "../Encoder.js";
import { Audio } from "../Audio.js";

export class ModecfgModalFm {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MidiService, Audio];
  }
  constructor(element, dom, midiService, audio) {
    this.element = element;
    this.dom = dom;
    this.midiService = midiService;
    this.audio = audio;
    
    // All "Modecfg" modals must implement, and resolve with Uint8Array or null.
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
  
  /* All "Modecfg" modals must implement.
   */
  setup(channel) {
    this.mode = channel.mode;
    this.modecfg = channel.modecfg;
    this.chid = channel.chid;
    this.channel = channel;
    this.model = decodeModecfg(this.mode, this.modecfg);
    this.buildUi();
  }
  
  /* UI.
   ***********************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => { e.preventDefault(); e.stopPropagation(); },
      "on-input": e => { this.playbackDirty = true; },
    });
    
    // Left half of the form is our four envelopes, with time scales synced.
    const leftSide = this.dom.spawn(form, "DIV", ["leftSide"]);
    const levelenv = this.dom.spawnController(leftSide, EnvUi);
    levelenv.setup(this.model.levelenv, "levelenv", v => this.onEnvChange("levelenv", v));
    const mixenv = this.dom.spawnController(leftSide, EnvUi);
    mixenv.setup(this.model.mixenv, "mixenv", v => this.onEnvChange("mixenv", v));
    const rangeenv = this.dom.spawnController(leftSide, EnvUi);
    rangeenv.setup(this.model.rangeenv, "rangeenv", v => this.onEnvChange("rangeenv", v));
    const pitchenv = this.dom.spawnController(leftSide, EnvUi);
    pitchenv.setup(this.model.pitchenv, "pitchenv", v => this.onEnvChange("pitchenv", v));
    levelenv.onTimeChange = mixenv.onTimeChange = rangeenv.onTimeChange = pitchenv.onTimeChange = (p, c) => {
      levelenv.setTime(p, c);
      mixenv.setTime(p, c);
      rangeenv.setTime(p, c);
      pitchenv.setTime(p, c);
    };
    
    // Everything else goes on the right.
    const rightSide = this.dom.spawn(form, "DIV", ["rightSide"]);
    const wavesRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    this.dom.spawnController(wavesRow, WaveUi).setup(this.model.wavea, "wavea", v => this.onWaveChange("wavea", v));
    this.dom.spawnController(wavesRow, WaveUi).setup(this.model.waveb, "waveb", v => this.onWaveChange("waveb", v));
    
    const modRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const modTable = this.dom.spawn(modRow, "TABLE", ["modTable"]);
    this.spawnBooleanRow(modTable, "modabs", this.model.modrate & 0x8000);
    this.spawnRow(modTable, "modrate", 0, 127, 1/256, "u7.8");
    this.spawnRow(modTable, "modrange", 0, 256, 1/256, "u8.8");
    this.dom.spawnController(modRow, WaveUi).setup(this.model.modulator, "modulator", v => this.onWaveChange("modulator", v));
    
    const rangeRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const rangeTable = this.dom.spawn(rangeRow, "TABLE", ["rangeTable"]);
    this.spawnRow(rangeTable, "rangelforate", 0, 256, 1/256);
    this.spawnRow(rangeTable, "rangelfodepth", 0, 255, 1);
    this.dom.spawnController(rangeRow, WaveUi).setup(this.model.rangelfowave, "rangelfo", v => this.onWaveChange("rangelfowave", v));
    
    const mixRow = this.dom.spawn(rightSide, "DIV", ["row"]);
    const mixTable = this.dom.spawn(mixRow, "TABLE", ["mixTable"]);
    this.spawnRow(mixTable, "mixlforate", 0, 256, 1/256);
    this.spawnRow(mixTable, "mixlfodepth", 0, 255, 1);
    this.dom.spawnController(mixRow, WaveUi).setup(this.model.mixlfowave, "mixlfo", v => this.onWaveChange("mixlfowave", v));
    
    const miscTable = this.dom.spawn(rightSide, "TABLE", ["miscTable"]);
    this.spawnRow(miscTable, "wheelrange", 0, 65535, 1);
    
    this.dom.spawn(rightSide, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  spawnBooleanRow(table, name, checked) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], name);
    let checkbox;
    this.dom.spawn(tr, "TD",
      checkbox = this.dom.spawn(null, "INPUT", { type: "checkbox", name })
    );
    if (checked) checkbox.checked = true;
  }
  
  spawnRow(table, name, min, max, step, adjust) {
    let value = this.model[name];
    switch (adjust) {
      case "u7.8": value = (value & 0x7fff) / 256; break;
      case "u8.8": value /= 256; break;
    }
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], name);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", min, max, step, name, value })
    );
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
      const modecfg = this.encodeModel();
      const encoder = new Encoder();
      encoder.raw("\0EAU");
      encoder.u16be(500); // tempo, whatever
      encoder.u32be(modecfg.length + 8 + (this.channel.post?.length || 0)); // Channel Headers length
      encoder.u8(0); // Channel zero.
      encoder.u8(this.channel.trim);
      encoder.u8(this.channel.pan);
      encoder.u8(this.mode);
      encoder.u16be(modecfg.length);
      encoder.raw(modecfg);
      if (this.channel.post) {
        encoder.u16be(this.channel.post.length);
        encoder.raw(this.channel.post);
      } else {
        encoder.u16be(0);
      }
      encoder.u32be(1); // Events length
      encoder.u8(0x7f); // Long delay
      this.audio.playEauSong(encoder.finish(), true);
    }
  }
  
  /* UI Events.
   *****************************************************************/
   
  encodeModel() {
    const model = {...this.model};
    for (const name of ["modrate", "modrange"]) { // Stored u8.8, presented as float.
      let v = +this.element.querySelector(`input[name='${name}']`)?.value || 0;
      v = Math.round(v * 256);
      if (v < 0) v = 0;
      else if (v > 0xffff) v = 0xffff;
      model[name] = v;
    }
    if (this.element.querySelector("input[name='modabs']")?.checked) {
      model.modrate |= 0x8000;
    }
    for (const name of ["wheelrange", "rangelforate", "rangelfodepth", "mixlforate", "mixlfodepth"]) { // Stored as presented.
      model[name] = +this.element.querySelector(`input[name='${name}']`)?.value || 0;
    }
    return encodeModecfg(model);
  }
   
  onEnvChange(name, v) {
    this.model[name] = v;
    this.playbackDirty = true;
  }
  
  onWaveChange(name, v) {
    this.model[name] = v;
    this.playbackDirty = true;
  }
  
  onSubmit(event) {
    event.preventDefault();
    this.resolve(this.encodeModel());
    this.element.remove();
  }
}
