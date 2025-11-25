/* ModecfgModalSub.js
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { EnvUi } from "./EnvUi.js";
import { Encoder } from "../Encoder.js";
import { MidiService } from "./MidiService.js";
import { Audio } from "../Audio.js";

export class ModecfgModalSub {
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
  setup(mode, modecfg, chid) {
    this.mode = mode;
    this.modecfg = modecfg;
    this.chid = chid;
    this.model = decodeModecfg(mode, modecfg);
    this.buildUi();
  }
  
  /* UI.
   ***************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", `Sub voice for channel ${this.chid}:`);
    
    const levelenvController = this.dom.spawnController(this.element, EnvUi);
    levelenvController.setup(this.model.levelenv, "levelenv", v => this.onLevelenvChange(v));
    
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => { e.preventDefault(); e.stopPropagation(); },
      "on-input": e => { this.playbackDirty = true; },
    });
    const table = this.dom.spawn(form, "TABLE");
    this.spawnRow(table, "widthlo", 0, 65535, 1, "hz");
    this.spawnRow(table, "widthhi", 0, 65535, 1, "hz");
    this.spawnRow(table, "stagec", 0, 255, 1, "");
    this.spawnRow(table, "gain", 0, 256, 1/256, "x");
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    
    const firstInput = this.element.querySelector("input[name='widthlo']");
    firstInput.focus();
    firstInput.select();
  }
  
  spawnRow(table, name, min, max, step, unit) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TD", ["k"], name);
    this.dom.spawn(tr, "TD",
      this.dom.spawn(null, "INPUT", { type: "number", name, min, max, step, value: this.model[name] })
    );
    if (unit) this.dom.spawn(tr, "TD", ["unit"], unit);
  }
  
  /* Communication with MIDI bus and synthesizer.
   ********************************************************************/
  
  onMidiEvent(event) {
    switch (event.opcode) {
      // No need to forward wheel events; sub doesn't use them.
      case 0x80:
      case 0x90: {
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
      encoder.u32be(modecfg.length + 8); // Channel Headers length
      encoder.u8(0); // Channel zero.
      encoder.u8(0x80); // Trim.
      encoder.u8(0x80); // Pan.
      encoder.u8(this.mode); // Should always be 3==SUB, but we got it generically so use it.
      encoder.u16be(modecfg.length);
      encoder.raw(modecfg);
      encoder.u16be(0); // Post length
      encoder.u32be(1); // Events length
      encoder.u8(0x7f); // Long delay
      this.audio.playEauSong(encoder.finish(), true);
    }
  }
  
  /* UI Events.
   ***********************************************************/
   
  encodeModel() {
    const model = {...this.model};
    for (const input of this.element.querySelectorAll("input[type='number']")) {
      model[input.name] = +input.value;
    }
    return encodeModecfg(model);
  }
   
  onLevelenvChange(v) {
    this.model.levelenv = v;
    this.playbackDirty = true;
  }
  
  onSubmit(event) {
    this.resolve(this.encodeModel());
    this.element.remove();
    event.preventDefault();
  }
}
