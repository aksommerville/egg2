/* ModecfgModalTrivial.js
 */
 
import { Dom } from "../Dom.js";
import { decodeModecfg, encodeModecfg } from "./EauDecoder.js";
import { Encoder } from "../Encoder.js";
import { MidiService } from "./MidiService.js";
import { Audio } from "../Audio.js";

export class ModecfgModalTrivial {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MidiService, Audio];
  }
  constructor(element, dom, midiService, audio) {
    this.element = element;
    this.dom = dom;
    this.midiService = midiService;
    this.audio = audio;
    
    this.unitForField = {
      maxlevel: "/ 65535",
      minlevel: "/ 65535",
      minhold: "ms",
      rlstime: "ms",
      wheelrange: "cents",
    };
    
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
    this.dom.spawn(this.element, "H2", `Trivial voice for channel ${this.chid}:`);
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => { e.preventDefault(); e.stopPropagation(); },
      "on-input": e => { this.playbackDirty = true; },
    });
    const table = this.dom.spawn(form, "TABLE");
    // Every field in our model is u16, a happy coincidence.
    for (const name of ["maxlevel", "minlevel", "minhold", "rlstime", "wheelrange"]) {
      const tr = this.dom.spawn(table, "TR");
      this.dom.spawn(tr, "TD", ["k"], name);
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name, min: 0, max: 0xffff, value: this.model[name] })
      );
      this.dom.spawn(tr, "TD", ["unit"], this.unitForField[name] || '');
    }
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    const firstInput = this.element.querySelector("input");
    firstInput.focus();
    firstInput.select();
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
   
  /* Events.
   *****************************************************************************/
   
  encodeModel() {
    const model = { ...this.model };
    for (const input of this.element.querySelectorAll("input[type='number']")) {
      model[input.name] = +input.value;
    }
    return encodeModecfg(model);
  }
   
  onSubmit(event) {
    this.resolve(this.encodeModel());
    this.element.remove();
    event.preventDefault();
  }
}
