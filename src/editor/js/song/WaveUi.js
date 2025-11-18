/* WaveUi.js
 */

import { Audio } from "../Audio.js";
import { Dom } from "../Dom.js";
import { Encoder } from "../Encoder.js";
import { encodeWave } from "./EauDecoder.js";

export class WaveUi {
  static getDependencies() {
    return [HTMLCanvasElement, Dom, Window, Audio];
  }
  constructor(element, dom, window, audio) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.audio = audio;
    
    this.wave = [];
    this.name = "";
    this.cb = v => {};
    this.renderTimeout = null;
    
    this.element.addEventListener("click", () => this.onClick());
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(wave, name, cb) {
    this.wave = wave;
    this.name = name;
    this.cb = cb;
    this.renderSoon();
  }
  
  onClick() {
    const modal = this.dom.spawnModal(WaveModal);
    modal.setup(this.wave, this.name, v => {
      this.wave = v;
      this.renderSoon();
      this.cb(v);
    });
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNowish();
    }, 100);
  }
  
  renderNowish() {
    const encoder = new Encoder();
    encodeWave(encoder, this.wave, null);
    const serial = encoder.finish();
    this.audio.printWave(serial).then(pcm => {
      this.renderNow(pcm);
    });
  }
  
  renderNow(pcm) {
    const bounds = this.element.getBoundingClientRect();
    this.element.width = bounds.width;
    this.element.height = bounds.height;
    const ctx = this.element.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    /* Don't overthink it, we don't have to be efficient (anyway it's a bit late for efficiency :P)
     * Trace a line through every sample.
     */
    const margin = 2;
    const vert = v => bounds.height - margin - ((v + 1.0) * (bounds.height - margin * 2)) / 2.0;
    ctx.beginPath();
    ctx.moveTo(0, vert(pcm[0]));
    for (let i=1; i<pcm.length; i++) {
      const x = (i * bounds.width) / pcm.length;
      ctx.lineTo(x, vert(pcm[i]));
    }
    ctx.strokeStyle = "#fff";
    ctx.stroke();
  }
}

/* WaveModal: Appears when you click on the thumbnail.
 *****************************************************************************/

export class WaveModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.wave = [];
    this.name = "";
    this.cb = v => {};
  }
  
  setup(wave, name, cb) {
    this.wave = wave;
    this.name = name;
    this.cb = cb;
    this.buildUi();
  }
  
  /* UI.
   ********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["row"]);
    this.dom.spawn(topRow, "DIV", ["name"], this.name);
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddCommand() });
    
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"], { "on-input": e => this.onInput(e) });
    this.rebuildList(scroller);
    
    this.dom.spawn(this.element, "DIV", ["message"]);
  }
  
  rebuildList(scroller) {
    if (!scroller) scroller = this.element.querySelector(".scroller");
    scroller.innerHTML = "";
    if (this.wave instanceof Array) {
      for (let ix=0; ix<this.wave.length; ix++) {
        const command = this.wave[ix];
        const row = this.dom.spawn(scroller, "DIV", ["command"]);
        this.rebuildRow(row, command);
      }
    }
  }
  
  rebuildRow(row, command) {
    row.innerHTML = "";
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteCommand(command, row) });
    const opcodeSelect = this.dom.spawn(row, "SELECT");
    for (let opcode=0; opcode<256; opcode++) {
      const opcodek = opcode;
      this.dom.spawn(opcodeSelect, "OPTION", { value: opcodek }, opcodek + " " + (COMMANDS[opcodek]?.name || ""));
    }
    opcodeSelect.value = command.opcode;
    this.dom.spawn(row, "INPUT", { type: "text", name: "param", value: this.reprHex(command.param) });
  }
  
  /* Model.
   *************************************************************/
  
  reprHex(src) {
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += src[i].toString(16).padStart(2, '0') + " ";
    }
    return dst;
  }
  
  evalHex(src) {
    let hi = -1;
    let dst = []; // Assemble temporarily into a full array.
    for (let srcp=0; srcp<src.length; srcp++) {
      let ch = src.charCodeAt(srcp);
      if (ch <= 0x20) continue;
           if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30;
      else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x41 + 10;
      else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x61 + 10;
      else throw new Error(`Unexpected character ${JSON.stringify(ch)} in hex dump`);
      if (hi < 0) hi = ch; else {
        dst.push((hi << 4) | ch);
        hi = -1;
      }
    }
    if (hi >= 0) throw new Error(`Uneven hex dump length.`);
    return new Uint8Array(dst);
  }
  
  /* Returns a new wave from our UI if valid, or a string describing the error if not.
   */
  validate() {
    const wave = [];
    for (const row of this.element.querySelectorAll(".scroller > .command")) {
      const opcode = +row.querySelector("select")?.value;
      if (isNaN(opcode) || (opcode < 0) || (opcode > 0xff)) return "Invalid opcode.";
      let param;
      try {
        param = this.evalHex(row.querySelector("input[name='param']").value);
      } catch (e) {
        return e?.message || "Invalid hex dump";
      }
      const meta = COMMANDS[opcode];
      if (!meta) {
        // Unknown opcode. User is on their own to keep it valid.
      } else {
        if (typeof(meta.paramlen) === "number") {
          if (meta.paramlen !== param.length) {
            return `Command ${JSON.stringify(meta.name)} takes exactly ${meta.paramlen} bytes param.`;
          }
        } else switch (meta.paramlen) {
          case "b0x2": {
              if (param.length < 1) return `Length byte required for ${JSON.stringify(meta.name)} command.`;
              if (param.length !== 1 + param[0] * 2) return `Incorrect length byte for ${JSON.stringify(meta.name)} command.`;
            } break;
          default: return `Unknown param mode ${JSON.stringify(meta.paramlen)}`; // my fault, not yours
        }
      }
      wave.push({ opcode, param });
    }
    
    // Empty is ok, and don't append an EOF.
    if (!wave.length) return wave;

    // If it doesn't end with EOF, insert one.
    if (wave[wave.length - 1].opcode !== 0x00) {
      wave.push({ opcode: 0, param: new Uint8Array(0) });
    }
    
    // If it contains an EOF before the final position, fail.
    for (let i=wave.length-1; i-->0; ) {
      if (wave[i].opcode === 0x00) return `EOF can only be the final command.`;
    }
    
    // When we have more than one command (one for EOF), the first must be 'initial' and other must not.
    if (wave.length > 1) {
      let meta = COMMANDS[wave[0].opcode];
      if (meta && !meta.initial) return `${JSON.stringify(meta.name)} command should not be in the leading position.`;
      for (let i=1; i<wave.length; i++) {
        meta = COMMANDS[wave[i].opcode];
        if (meta && meta.initial) return `${JSON.stringify(meta.name)} command only makes sense in the leading position.`;
      }
    }
    
    return wave;
  }
  
  /* Events.
   ***********************************************************************/
   
  onAddCommand() {
    const command = { opcode: 0, param: new Uint8Array(0) };
    this.wave.push(command);
    const scroller = this.element.querySelector(".scroller");
    const row = this.dom.spawn(scroller, "DIV", ["command"]);
    this.rebuildRow(row, command);
    // Don't trigger (cb). Appending an EOF like we've just done doesn't change the wave.
  }
  
  onDeleteCommand(command, row) {
    const p = this.wave.indexOf(command);
    if (p < 0) return;
    this.wave.splice(p, 1);
    row.remove();
    this.onInput(null);
  }
  
  onInput(event) {
    const result = this.validate();
    const messageElement = this.element.querySelector(".message");
    if (result instanceof Array) { // Valid.
      this.wave = result;
      messageElement.innerText = "";
      this.cb(this.wave);
    } else { // Invalid, should be an error message.
      messageElement.innerText = result || "invalid";
    }
  }
}

/* Indexed by opcode.
 */
const COMMANDS = [
  /*00*/ { name: "EOF",       paramlen: 0,      initial: false },
  /*01*/ { name: "SINE",      paramlen: 0,      initial: true },
  /*02*/ { name: "SQUARE",    paramlen: 1,      initial: true },
  /*03*/ { name: "SAW",       paramlen: 1,      initial: true },
  /*04*/ { name: "TRIANGLE",  paramlen: 1,      initial: true },
  /*05*/ { name: "NOISE",     paramlen: 0,      initial: true },
  /*06*/ { name: "ROTATE",    paramlen: 1,      initial: false },
  /*07*/ { name: "GAIN",      paramlen: 2,      initial: false },
  /*08*/ { name: "CLIP",      paramlen: 1,      initial: false },
  /*09*/ { name: "NORM",      paramlen: 1,      initial: false },
  /*0a*/ { name: "HARMONICS", paramlen: "b0x2", initial: false },
  /*0b*/ { name: "HARMFM",    paramlen: 1,      initial: false },
  /*0c*/ { name: "MAVG",      paramlen: 1,      initial: false },
];
