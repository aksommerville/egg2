/* WaveUi.js
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { Encoder } from "../Encoder.js";
//TODO import { eauEncodeWave } from "./eauSong.js";

export const SHAPE_NAMES = [ // indexed by shape value
  "sine",
  "square",
  "saw",
  "triangle",
  "fixedfm",
];

export class WaveUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, Comm];
  }
  constructor(element, dom, window, comm) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.comm = comm;
    
    this.ondirty = v => {};
    
    /* Arbitrarily limit harmonics to 16.
     * Technically we can go up to 255 but by that point they're not very useful.
     */
    this.harmcolc = 16;
    
    this.wave = null; // READONLY, model as provided.
    this.harmonics = null; // Replacement harmonics. (shape,qual) transient values are stored in the DOM.
    this.harmonicsRenderTimeout = null;
    this.previewRenderTimeout = null;
    this.trackHarmonic = -1; // 0..harmcolc-1 if tracking
  }
  
  onRemoveFromDom() {
    if (this.harmonicsRenderTimeout) {
      this.window.clearTimeout(this.harmonicsRenderTimeout);
      this.harmonicsRenderTimeout = null;
    }
    if (this.previewRenderTimeout) {
      this.window.clearTimeout(this.previewRenderTimeout);
      this.previewRenderTimeout = null;
    }
  }
  
  setup(wave, name) {
    this.wave = wave;
    this.harmonics = [...wave.harmonics];
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["row"]);
    const shapeSelect = this.dom.spawn(topRow, "SELECT", { name: "shape", "on-change": () => {
      this.renderSoon();
      this.ondirty(this.rebuildModel());
    }});
    for (let i=0; i<256; i++) {
      this.dom.spawn(shapeSelect, "OPTION", { value: i }, SHAPE_NAMES[i] || i.toString());
    }
    shapeSelect.value = wave.shape;
    
    // text rather than number, since for fixedfm i prefer to express it in hexadecimal
    this.dom.spawn(topRow, "INPUT", { name: "qual", type: "text", value: this.wave.qual, "on-input": () => this.onQualInput("qual", "qualSlider") });
    
    // And also a range slider for qual, since for square and saw it's continuous.
    this.dom.spawn(topRow, "INPUT", { name: "qualSlider", type: "range", min: 0, max: 0xff, value: this.wave.qual, "on-input": () => this.onQualInput("qualSlider", "qual") });
    
    const bottomRow = this.dom.spawn(this.element, "DIV", ["row"]);
    this.dom.spawn(bottomRow, "CANVAS", ["harmonics"], {
      "on-pointerdown": e => this.onHarmonicsPointerDown(e),
      "on-pointerup": e => this.onHarmonicsPointerUp(e),
      "on-pointermove": e => this.onHarmonicsPointerMove(e),
    });
    
    this.dom.spawn(bottomRow, "CANVAS", ["preview"]);
    
    this.renderSoon();
  }
  
  rebuildModel() {
    const harmonics = [...this.harmonics];
    while ((harmonics.length > 0) && !harmonics[harmonics.length - 1]) harmonics.splice(harmonics.length - 1, 1);
    return {
      shape: +this.element.querySelector("select[name='shape']").value || 0,
      qual: +this.element.querySelector("input[name='qual']").value || 0,
      harmonics,
    };
  }
  
  /* Render.
   **********************************************************************************/
   
  renderSoon() {
    if (!this.harmonicsRenderTimeout) {
      this.harmonicsRenderTimeout = this.window.setTimeout(() => {
        this.harmonicsRenderTimeout = null;
        this.renderHarmonicsNow();
      }, 50);
    }
    if (!this.previewRenderTimeout) {
      this.previewRenderTimeout = this.window.setTimeout(() => {
        this.previewRenderTimeout = null;
        this.generateAndRenderPreview();
      }, 500);
    }
  }
  
  renderHarmonicsNow() {
    const canvas = this.element.querySelector("canvas.harmonics");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#888";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    // Guide lines between columns, and within each at 1/n.
    ctx.beginPath();
    for (let col=0, x=0.5; col<this.harmcolc; col++) {
      const nextX = Math.floor(((col + 1) * bounds.width) / this.harmcolc) + 0.5;
      if (col) {
        ctx.moveTo(x, 0);
        ctx.lineTo(x, bounds.height);
        const y = Math.floor(bounds.height - bounds.height / (col + 1)) + 0.5;
        ctx.moveTo(x, y);
        ctx.lineTo(nextX, y);
      }
      x = nextX;
    }
    ctx.strokeStyle = "#666";
    ctx.stroke();
    
    // Orange boxes for the nonzero harmonics.
    ctx.beginPath();
    for (let col=0, x=0.5; col<this.harmcolc; col++) {
      const nextX = Math.floor(((col + 1) * bounds.width) / this.harmcolc) + 0.5;
      const v = this.harmonics[col];
      if (v) {
        const h = Math.floor((v * bounds.height) / 0xffff);
        ctx.rect(x, bounds.height - h + 0.5, nextX - x, h);
      }
      x = nextX;
    }
    ctx.strokeStyle = "#000";
    ctx.fillStyle = "#f80";
    ctx.fill();
    ctx.stroke();
  }
  
  generateAndRenderPreview() {
    //TODO Once the web runtime's synthesizer is written, we could use that instead and not need to make an HTTP call.
    // There may also be some value in allowing user to toggle sources, especially during development of the web runtime.
    // At any rate, renderPreviewNow() is ready to accept Float32Array in -1..1 instead of Int16Array.
    const encoder = new Encoder();
    eauEncodeWave(encoder, this.rebuildModel());
    const req = encoder.finish();
    this.comm.httpBinary("POST", "/api/synthwave", null, null, req).then(rsp => {
      rsp = new Int16Array(rsp);
      this.renderPreviewNow(rsp);
    }).catch(e => {
      console.error(e);
      this.renderPreviewNow(null);
    });
  }
  
  // (pcm) is Float32Array or Int16Array.
  renderPreviewNow(pcm) {
    const canvas = this.element.querySelector("canvas.preview");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    if (!pcm) return;
    
    // Generate high and low value for each column, first in pcm scale, then convert to pixels.
    const hiv=[], lov=[];
    for (let x=0, lop=0; x<bounds.width; x++) {
      const hip = Math.max(lop+1, Math.floor(((x + 1) * pcm.length) / bounds.width));
      let hi=pcm[lop], lo=pcm[lop];
      for (let p=lop+1; p<hip; p++) {
        if (pcm[p] > hi) hi = pcm[p];
        else if (pcm[p] < lo) lo = pcm[p];
      }
      hiv.push(hi);
      lov.push(lo);
      lop = hip;
    }
    if (pcm instanceof Float32Array) {
      const yscale = bounds.height / 2;
      for (let x=bounds.width; x-->0; ) {
        hiv[x] = (1 - hiv[x]) * yscale;
        lov[x] = (1 - lov[x]) * yscale;
      }
    } else if (pcm instanceof Int16Array) {
      const yscale = bounds.height / 65535;
      for (let x=bounds.width; x-->0; ) {
        hiv[x] = (32768 - hiv[x]) * yscale;
        lov[x] = (32768 - lov[x]) * yscale;
      }
    }
    
    ctx.beginPath();
    ctx.moveTo(0, hiv[0]);
    for (let x=0; x<bounds.width; x++) ctx.lineTo(x, hiv[x]);
    for (let x=bounds.width; x-->0; ) ctx.lineTo(x, lov[x]);
    ctx.fillStyle = "#ffc";
    ctx.fill();
    ctx.strokeStyle = "#ff0";
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.lineWidth = 1;
  }
  
  /* Events.
   **********************************************************************************/
   
  onHarmonicsPointerDown(event) {
    const canvas = this.element.querySelector("canvas.harmonics");
    const bounds = canvas.getBoundingClientRect();
    const col = Math.floor(((event.x - bounds.x) * this.harmcolc) / bounds.width);
    if ((col < 0) || (col >= this.harmcolc)) return;
    while (this.harmonics.length <= col) this.harmonics.push(0);
    this.trackHarmonic = col;
    canvas.setPointerCapture(event.pointerId);
    this.onHarmonicsPointerMove(event);
  }
  
  onHarmonicsPointerUp(event) {
    if (this.trackHarmonic < 0) return;
    this.trackHarmonic = -1;
    this.renderSoon();
    this.ondirty(this.rebuildModel());
  }
  
  onHarmonicsPointerMove(event) {
    if (this.trackHarmonic < 0) return;
    const canvas = this.element.querySelector("canvas.harmonics");
    const bounds = canvas.getBoundingClientRect();
    const v = Math.min(0xffff, Math.max(0, Math.floor(0xffff - ((event.y - bounds.y) * 0xffff) / bounds.height)));
    if (v === this.harmonics[this.trackHarmonic]) return;
    this.harmonics[this.trackHarmonic] = v;
    this.renderSoon();
  }
  
  onQualInput(active, passive) {
    active = this.element.querySelector(`input[name='${active}']`);
    passive = this.element.querySelector(`input[name='${passive}']`);
    const v = Math.max(0, Math.min(0xff, Math.floor(+active.value) || 0));
    passive.value = v;
    this.renderSoon();
    this.ondirty(this.rebuildModel());
  }
}
