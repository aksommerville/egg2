/* HarmonicsUi.js
 */
 
import { Dom } from "../Dom.js";

export class HarmonicsUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.harmonics = []; // 0..0xffff
    this.cb = (v) => {};
    this.renderTimeout = null;
    this.colc = 16;
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(harmonics, cb) {
    this.harmonics = [...harmonics];
    this.cb = cb;
    this.colc = Math.min(255, Math.max(16, this.harmonics.length + 4));
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", {
      "on-pointerdown": e => this.onPointerDown(e),
      "on-pointerup": e => this.onPointerUp(e),
      "on-pointermove": e => this.onPointerMove(e),
      "on-contextmenu": e => e.preventDefault(),
    });
    this.renderSoon();
  }
  
  /* Render.
   **********************************************************************************/
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 20);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    // And a bar for each harmonic.
    const MINH = 5;
    ctx.fillStyle = "#f80";
    for (let col=0; col<this.harmonics.length; col++) {
      const v = this.harmonics[col];
      if (!v) continue;
      const h = MINH + Math.floor((v * (bounds.height - MINH)) / 0xffff);
      const xa = (col * bounds.width) / this.colc;
      const xz = ((col + 1) * bounds.width) / this.colc;
      ctx.fillRect(xa, bounds.height - h, xz - xa, h);
    }
    
    // Draw a guide at 1/n, and dividers between columns.
    ctx.beginPath();
    for (let col=0; col<this.colc; col++) {
      const xa = (col * bounds.width) / this.colc;
      const xz = ((col + 1) * bounds.width) / this.colc;
      const y = bounds.height - bounds.height / (col + 1);
      ctx.moveTo(xa, y);
      ctx.lineTo(xz, y);
      if (col) {
        ctx.moveTo(xa, 0);
        ctx.lineTo(xa, bounds.height);
      }
    }
    ctx.strokeStyle = "#088";
    ctx.stroke();
  }
  
  /* Events.
   **************************************************************************************/
  
  onPointerDown(event) {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const col = Math.floor((x * this.colc) / bounds.width);
    if ((col < 0) || (col >= this.colc)) return;
    if (event.button === 2) {
      event.stopPropagation();
      event.preventDefault();
      if (col >= this.harmonics.length) return;
      if (!this.harmonics[col]) return;
      this.harmonics[col] = 0;
    } else if (event.button === 0) {
      const v = Math.max(0, Math.min(0xffff, ~~(((bounds.height - y) * 0xffff) / bounds.height)));
      while (col >= this.harmonics.length) this.harmonics.push(0);
      if (this.harmonics[col] === v) return;
      this.harmonics[col] = v;
    } else return;
    this.renderSoon();
    this.cb(this.harmonics);
  }
  
  onPointerUp(event) {
  }
  
  onPointerMove(event) {
  }
}
