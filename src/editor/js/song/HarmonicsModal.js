/* HarmonicsModal.js
 * Visual bar-chart editor for a harmonics wave step.
 */
 
import { Dom } from "../Dom.js";

export class HarmonicsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.renderTimeout = null;
    
    this.result = new Promise((res, rej) => {
      this.resolve = res;
      this.reject = rej;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  /* (param) is a string, presumably a hex dump with a 1-byte count then 2-byte coefficients.
   * We'll eventually resolve with the same format, or null.
   */
  setup(param) {
    this.coefv = this.evalParam(param);
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "INPUT", { type: "number", min: 0, max: 255, step: 1, value: this.coefv.length, "on-input": e => this.onCoefcInput(e) });
    this.dom.spawn(this.element, "CANVAS", ["visual"], {
      "on-pointerdown": e => this.onPointerDown(e),
      "on-contextmenu": e => { e.preventDefault(); e.stopPropagation(); },
    });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "OK", "on-click": e => this.onSubmit(e) });
    this.renderSoon();
  }
  
  /* Model.
   *************************************************************************/
  
  // Returns an empty array if malformed, otherwise array of u16.
  evalParam(param) {
    const digits = param.replace(/\s/g, "");
    if ((digits.length < 2) || (digits.length & 1)) return [];
    const coefc = parseInt(digits.substring(0, 2), 16);
    if (coefc !== (digits.length - 2) / 4) return [];
    const coefv = [];
    for (let i=coefc, p=2; i-->0; p+=4) {
      coefv.push(parseInt(digits.substring(p, p+4), 16));
    }
    return coefv;
  }
  
  // Returns a valid hexdump. "00" if we can't do better.
  reprParam(coefv) {
    if (coefv.length < 1) return "00";
    const coefc = Math.min(0xff, coefv.length);
    let param = coefc.toString(16).padStart(2, '0') + " ";
    for (let i=0; i<coefc; i++) {
      param += coefv[i].toString(16).padStart(4, '0') + " ";
    }
    return param.trim();
  }
  
  /* Render.
   ***********************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas.visual");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext('2d');
    if (this.coefv.length < 1) {
      ctx.fillStyle = "#888";
      ctx.fillRect(0, 0, bounds.width, bounds.height);
      return;
    }
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    ctx.fillStyle = "#f80";
    for (let col=0; col<this.coefv.length; col++) {
      const v = this.coefv[col];
      const x = (col * bounds.width) / this.coefv.length;
      const xz = ((col + 1) * bounds.width) / this.coefv.length;
      const y = ((0xffff - v) * bounds.height) / 0xffff;
      ctx.fillRect(x, y, xz - x, bounds.height - y);
    }
    ctx.beginPath();
    for (let col=0; col<this.coefv.length; col++) {
      const x = (col * bounds.width) / this.coefv.length;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, bounds.height);
    }
    ctx.strokeStyle = "#888";
    ctx.stroke();
  }
  
  /* Events.
   ******************************************************************/
   
  onCoefcInput(event) {
    const coefc = +event?.target?.value;
    if (isNaN(coefc) || (coefc < 0) || (coefc > 0xff)) return;
    while (coefc > this.coefv.length) this.coefv.push(0);
    if (coefc < this.coefv.length) this.coefv.splice(coefc, this.coefv.length - coefc);
    this.renderSoon();
  }
   
  onPointerDown(event) {
    event.target.setPointerCapture(event.pointerId);
    event.target.onpointerup = e => this.onPointerUp(e);
    event.target.onpointermove = e => this.onPointerMove(e);
    this.onPointerMove(event);
  }
  
  onPointerUp(event) {
    event.target.onpointerup = null;
    event.target.onpointermove = null;
  }
  
  onPointerMove(event) {
    if (this.coefv.length < 1) return;
    const canvas = event.target;
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const p = Math.floor((x * this.coefv.length) / bounds.width);
    if ((p < 0) || (p >= this.coefv.length)) return;
    const v = Math.max(0, Math.min(0xffff, Math.round(((bounds.height - y) * 0xffff) / bounds.height)));
    if (this.coefv[p] === v) return;
    this.coefv[p] = v;
    this.renderSoon();
  }
  
  onSubmit(event) {
    this.resolve(this.reprParam(this.coefv));
    this.element.remove();
  }
}
