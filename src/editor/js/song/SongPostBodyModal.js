/* SongPostBodyModal.js
 * Edit the body of a post stage, for all stageid.
 */
 
import { Dom } from "../Dom.js";
//TODO import { EAU_POST_STAGE_NAMES } from "./eauSong.js";
import { reprHexdump, evalHexdump } from "./songDisplayBits.js";
import { Encoder } from "../Encoder.js";

export class SongPostBodyModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.stageid = 0;
    this.body = [];
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(stageid, body) {
    this.stageid = stageid;
    this.body = body;
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["title"], `Post stage ${stageid}: ${EAU_POST_STAGE_NAMES[stageid] || ""}`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    
    let autofocus = null;
    switch (this.stageid) {
      case 1: autofocus = this.spawnUiGain(form); break;
      case 2: autofocus = this.spawnUiDelay(form); break;
      case 3: autofocus = this.spawnUiFilter(form, false); break;
      case 4: autofocus = this.spawnUiFilter(form, false); break;
      case 5: autofocus = this.spawnUiFilter(form, true); break;
      case 6: autofocus = this.spawnUiFilter(form, true); break;
      case 7: autofocus = this.spawnUiWaveshaper(form); break;
      case 8: autofocus = this.spawnUiTremolo(form); break;
      default: this.spawnUiGeneric(form);
    }
    
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => {
      e.preventDefault();
      e.stopPropagation();
      this.onSubmit();
    }});
    
    if (autofocus) {
      this.window.setTimeout(() => {
        autofocus.focus();
        autofocus.select();
      }, 50);
    }
  }
  
  onSubmit() {
    let body = null;
    switch (this.stageid) {
      case 1: body = this.readUiGain(); break;
      case 2: body = this.readUiDelay(); break;
      case 3: body = this.readUiFilter(false); break;
      case 4: body = this.readUiFilter(false); break;
      case 5: body = this.readUiFilter(true); break;
      case 6: body = this.readUiFilter(true); break;
      case 7: body = this.readUiWaveshaper(); break;
      case 8: body = this.readUiTremolo(); break;
      default: body = this.readUiGeneric();
    }
    this.resolve(body);
    this.element.remove();
  }
  
  /* Generic stage: Hex dump.
   ********************************************************************************/
   
  spawnUiGeneric(form) {
    return this.dom.spawn(form, "TEXTAREA", reprHexdump(this.body));
  }
  
  readUiGeneric() {
    return evalHexdump(this.element.querySelector("textarea")?.value);
  }
  
  /* Gain: (u8.8 gain, u0.8 clip)
   *********************************************************************************/
   
  spawnUiGain(form) {
    let autofocus = null;
    let gain = 1, clip = 1;
    if (this.body.length >= 2) {
      gain = ((this.body[0] << 8) | this.body[1]) / 256;
      if (this.body.length >= 3) {
        clip = this.body[2] / 255;
      }
    }
    this.dom.spawn(form, "TABLE", 
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Gain"),
        this.dom.spawn(null, "TD",
          autofocus = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, value: gain, name: "gain" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Clip"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: clip, name: "clip" })
        )
      )
    );
    return autofocus;
  }
  
  readUiGain() {
    let gain = +this.element.querySelector("input[name='gain']")?.value || 0;
    let clip = +this.element.querySelector("input[name='clip']")?.value || 0;
    const encoder = new Encoder();
    encoder.u8_8(+this.element.querySelector("input[name='gain']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='clip']")?.value || 0);
    return encoder.finish();
  }
  
  /* Delay: (u8.8 period, u0.8 dry, u0.8 wet, u0.8 store, u0.8 feedback)
   *********************************************************************************/
   
  spawnUiDelay(form) {
    let autofocus = null;
    let period=1, dry=0.5, wet=0.5, sto=0.5, fbk=0.5;
    if (this.body.length >= 6) {
      period = ((this.body[0] << 8) | this.body[1]) / 256;
      dry = this.body[2] / 255;
      wet = this.body[3] / 255;
      sto = this.body[4] / 255;
      fbk = this.body[5] / 255;
    }
    this.dom.spawn(form, "TABLE",
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Period (qnotes)"),
        this.dom.spawn(null, "TD",
          autofocus = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, value: period, name: "period" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Dry"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: dry, name: "dry" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Wet"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: wet, name: "wet" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Store"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: sto, name: "sto" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Feedback"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: fbk, name: "fbk" })
        )
      ),
    );
    return autofocus;
  }
  
  readUiDelay() {
    const encoder = new Encoder();
    encoder.u8_8(+this.element.querySelector("input[name='period']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='dry']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='wet']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='sto']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='fbk']")?.value || 0);
    return encoder.finish();
  }
  
  /* Lopass, hipass, bandpass, and notch: (u16 hz[, u16 width(hz)])
   *******************************************************************************/
   
  spawnUiFilter(form, withWidth) {
    let autofocus = null;
    const table = this.dom.spawn(form, "TABLE");
    let mid=512, wid=256;
    if (this.body.length >= 2) {
      mid = (this.body[0] << 8) | this.body[1];
      if (withWidth && (this.body.length >= 4)) {
        wid = (this.body[2] << 8) | this.body[3];
      }
    }
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], withWidth ? "Center (hz)" : "Cutoff (hz)"),
      this.dom.spawn(null, "TD",
        autofocus = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 0xffff, value: mid, name: "mid" })
      )
    );
    if (withWidth) {
      this.dom.spawn(table, "TR",
        this.dom.spawn(null, "TD", ["key"], "Width (hz)"),
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 0xffff, value: wid, name: "wid" })
      )
    }
    return autofocus;
  }
  
  readUiFilter(withWidth) {
    const encoder = new Encoder();
    encoder.u16be(+this.element.querySelector("input[name='mid']")?.value || 0);
    if (withWidth) {
      encoder.u16be(+this.element.querySelector("input[name='wid']")?.value || 0);
    }
    return encoder.finish();
  }
  
  /* Waveshaper: (u0.16 level...)
   ******************************************************************************/
   
  spawnUiWaveshaper(form) {
    let autofocus = null;
    const table = this.dom.spawn(form, "TABLE");
    const levelc = Math.max((this.body.length >> 1) + 1, 5);
    for (let i=0, p=0; i<levelc; i++, p+=2) {
      const level = (((this.body[p] << 8) | this.body[p+1]) / 65536) || 0;
      let el;
      this.dom.spawn(table, "TR", ["level"],
        this.dom.spawn(null, "TD", ["key"], i),
        this.dom.spawn(null, "TD",
          el = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/0x10000, name: i, value: level })
        )
      );
      if (!autofocus) autofocus = el;
    }
    this.dom.spawn(form, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddWaveshaperLevel() });
    return autofocus;
  }
  
  readUiWaveshaper() {
    const levels = Array.from(this.element.querySelectorAll("tr.level input")).map(e => +e.value || 0);
    while (levels.length && !levels[levels.length - 1]) levels.splice(levels.length - 1, 1);
    const encoder = new Encoder();
    for (const level of levels) encoder.u0_16(level);
    return encoder.finish();
  }
  
  onAddWaveshaperLevel() {
    const table = this.element.querySelector("form > table");
    if (!table) return;
    const levelc = Array.from(table.querySelectorAll("tr")).length;
    if (levelc >= 0x80) return;
    let autofocus = null;
    this.dom.spawn(table, "TR", ["level"],
      this.dom.spawn(null, "TD", ["key"], levelc),
      this.dom.spawn(null, "TD",
        autofocus = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/0x10000, name: levelc, value: 0 })
      )
    );
    autofocus.focus();
    autofocus.select();
  }
  
  /* Tremolo: (u8.8 period, u0.8 depth, u0.8 phase)
   *******************************************************************************/
   
  spawnUiTremolo(form) {
    let autofocus = null;
    let period=1, depth=0.5, phase=0;
    if (this.body.length >= 2) {
      period = ((this.body[0] << 8) | this.body[1]) / 256;
      if (this.body.length >= 3) {
        depth = this.body[2] / 255;
        if (this.body.length >= 4) {
          phase = this.body[3] / 256;
        }
      }
    }
    this.dom.spawn(form, "TABLE",
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Period (qnotes)"),
        this.dom.spawn(null, "TD",
          autofocus = this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 256, step: 1/256, value: period, name: "period" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Depth"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: depth, name: "depth" })
        )
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", ["key"], "Phase"),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 1, step: 1/256, value: phase, name: "phase" })
        )
      ),
    );
    return autofocus;
  }
  
  readUiTremolo() {
    const encoder = new Encoder();
    encoder.u8_8(+this.element.querySelector("input[name='period']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='depth']")?.value || 0);
    encoder.u0_8(+this.element.querySelector("input[name='phase']")?.value || 0);
    return encoder.finish();
  }
}
