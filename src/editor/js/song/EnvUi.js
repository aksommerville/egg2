/* EnvUi.js
 */
 
import { Dom } from "../Dom.js";

export class EnvUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, "nonce"];
  }
  constructor(element, dom, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    this.env = {};
    this.usage = "levelenv";
    this.cb = v => {};
    this.renderTimeout = null;
    this.zoomPointerListener = null;
    this.trange = 500; // Horizontal view range in ms.
    this.vrange = 65535; // Vertical view range in level units.
    this.t0 = 0; // Time at left edge.
    this.v0 = 0; // Level at bottom edge -- vertical axis is positive up.
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(env, usage, cb) {
    this.env = env;
    this.usage = usage;
    this.cb = cb;
    this.buildUi();
  }
  
  /* UI.
   **************************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    this.dom.spawn(this.element, "CANVAS", ["visual"], {
      "on-pointerdown": e => this.onVisualDown(e),
      "on-pointerup": e => this.onVisualUp(e),
      "on-pointermove": e => this.onVisualMove(e),
      "on-pointerleave": e => this.onVisualLeave(e),
      "on-wheel": e => this.onWheel(e),
    });
    
    this.dom.spawn(this.element, "DIV", ["controls"],
      this.dom.spawn(null, "INPUT", ["toggle",], {
        id: `EnvUi-${this.nonce}-velocity`,
        type: "checkbox",
        name: "velocity",
        checked: this.env.hi ? "checked" : undefined,
        "on-change": e => this.onVelocityChange(e),
      }),
      this.dom.spawn(null, "LABEL", ["toggle", "light"], { for: `EnvUi-${this.nonce}-velocity` }, "Velocity"),
      this.dom.spawn(null, "DIV", ["zoom"], { "on-pointerdown": e => this.onZoomDown(e) },
        this.dom.spawn(null, "DIV", "Z")
      ),
      this.dom.spawn(null, "DIV", ["tattle"])
    );
    
    this.renderSoon();
  }
  
  /* Render.
   ************************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 20);
  }
  
  renderNow() {
    const canvas = this.element.querySelector(".visual");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    const hienable = this.element.querySelector("input[name='velocity']")?.checked && this.env.hi;
    
    // Visual guides.
    //TODO
    
    // Lines and handles.
    this.renderLine(canvas, ctx, this.env.lo, "#c60");
    if (hienable) this.renderLine(canvas, ctx, this.env.hi, "#ff0");
    this.renderHandles(canvas, ctx, this.env.lo, "#c60");
    if (hienable) this.renderHandles(canvas, ctx, this.env.hi, "#ff0");
  }
  
  renderLine(canvas, ctx, line, color) {
    ctx.beginPath();
    for (let i=0; i<line.length; i++) {
      const pt = line[i];
      const x = Math.floor((pt.t * canvas.width) / this.trange - this.t0) + 0.5;
      const y = Math.floor(canvas.height - (pt.v * canvas.height) / this.vrange - this.v0) + 0.5;
      if (i) ctx.lineTo(x, y);
      else ctx.moveTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.stroke();
  }
  
  renderHandles(canvas, ctx, line, color) {
  }
  
  /* Coordinates.
   */
   
  logicalCoordsFromEvent(event) {
    const canvas = this.element.querySelector("canvas.visual");
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const t = Math.max(0, Math.round(this.t0 + (x * this.trange) / bounds.width));
    const v = Math.max(0, Math.min(0xffff, Math.round(this.v0 + ((bounds.height - y) * this.vrange) / bounds.height)));
    return { t, v, x, y };
  }
  
  /* Scroll.
   */
  
  scroll(dx, dy) {
    console.log(`EnvUi.scroll ${dx},${dy}`);
  }
  
  zoom(dx, dy, ref) {
    console.log(`EnvUi.zoom ${dx},${dy}`);
  }
  
  /* Events.
   ***************************************************************************************/
   
  onVisualDown(event) {
    event.target.setPointerCapture(event.pointerId);
  }
  
  onVisualUp(event) {
    console.log(`onVisualUp`);
  }
  
  onVisualMove(event) {
    const ref = this.logicalCoordsFromEvent(event);
    this.element.querySelector(".tattle").innerText = `0x${ref.v.toString(16).padStart(4, '0')} @ ${ref.t.toString().padStart(4)} ms`;
  }
  
  onVisualLeave(event) {
    this.element.querySelector(".tattle").innerText = "";
  }
  
  onWheel(event) {
    event.preventDefault();
    event.stopPropagation();
    const d = event.deltaX || event.deltaY;
    if (!d) return;
    if (event.ctrlKey) {
      const ref = this.logicalCoordsFromEvent(event);
      if (event.shiftKey) {
        if (d < 0) this.zoom(-1, 0, ref);
        else this.zoom(1, 0, ref);
      } else {
        if (d < 0) this.zoom(0, -1, ref);
        else this.zoom(0, 1, ref);
      }
    } else {
      if (event.shiftKey) {
        if (d < 0) this.scroll(-1, 0);
        else this.scroll(1, 0);
      } else {
        if (d < 0) this.scroll(0, -1);
        else this.scroll(0, 1);
      }
    }
  }
  
  onZoomDown(event) {
    console.log(`onZoomDown`);
  }
  
  onVelocityChange(event) {
    if (event.target.checked) {
      if (this.env.hirestore && (this.env.hirestore.length === this.env.lo.length)) {
        this.env.hi = this.env.hirestore;
      } else {
        this.env.hi = this.env.lo.map(pt => ({...pt}));
      }
      delete this.env.hirestore;
    } else {
      this.env.hirestore = this.env.hi;
      delete this.env.hi;
    }
    this.cb(this.env);
    this.renderSoon();
  }
}
