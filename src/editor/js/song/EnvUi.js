/* EnvUi.js
 */
 
import { Dom } from "../Dom.js";
import { EnvPointModal } from "./EnvPointModal.js";

const HANDLE_RADIUS = 6;

export class EnvUi {
  static getDependencies() {
    return [HTMLElement, Dom, Window, "nonce"];
  }
  constructor(element, dom, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    this.onTimeChange = (p, c) => {}; // Notify of scroll or zoom on time axis. So parent can sync its children's time axes.
    
    this.env = {};
    this.usage = "levelenv";
    this.cb = v => {};
    this.renderTimeout = null;
    this.zoomPointerListener = null;
    this.trange = 800; // Horizontal view range in ms.
    this.vrange = 65535; // Vertical view range in level units.
    this.t0 = 0; // Time at left edge.
    this.v0 = 0; // Level at bottom edge -- vertical axis is positive up.
    this.dragHandle = null; // null or a point from (this.env).
    this.dragDt = 0; // Mouse position relative to drag handle, in model units.
    this.dragDv = 0;
    this.dragTMin = 0; // From the neighbor points; we don't let you move past them.
    this.dragTMax = 0;
    this.dragVOk = true; // False if value must be zero.
    this.zeroForeAndAft = true; // For "levelenv", don't allow nonzero value for the first or last point. (the model technically does allow it).
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
    
    this.zeroForeAndAft = false;
    switch (usage) {
      case "levelenv": {
          this.zeroForeAndAft = true;
        } break;
      case "pitchenv": {
          // pitchenv v axis is in signed cents. The full range is insane, 327 semitones each way. Narrow it to one octave each way.
          this.vrange = 2400;
          this.v0 = 0x8000 - 1200;
        } break;
    }
    
    this.buildUi();
  }
  
  // Companion to (onTimeChange).
  setTime(p, c) {
    if ((this.t0 === p) && (this.trange === c)) return;
    this.t0 = p;
    this.trange = c;
    this.renderSoon();
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
      "on-contextmenu": e => e.preventDefault(),
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
      this.dom.spawn(null, "INPUT", { type: "number", name: "susp", min: 0, max: 15, value: this.env.susp, "on-input": () => this.onSuspChange() }),
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
    let tguideunit = 1000;
    let tguidew = (tguideunit * bounds.width) / this.trange;
    const tguidetarget = bounds.width / 10;
    if (tguidew > tguidetarget) {
      while ((tguidew > tguidetarget) && (tguideunit > 10)) {
        tguideunit *= 0.5;
        tguidew *= 0.5;
      }
    } else {
      while (tguideunit < 100000) {
        const nextw = tguidew * 2;
        if (nextw >= tguidetarget) break;
        tguidew = nextw;
        tguideunit *= 2;
      }
    }
    let vguideunit = 1000;
    let vguideh = (vguideunit * bounds.height) / this.vrange;
    const vguidetarget = bounds.height / 10;
    if (vguideh > vguidetarget) {
      while ((vguideh > vguidetarget) && (vguideunit > 10)) {
        vguideunit *= 0.5;
        vguideh *= 0.5;
      }
    } else {
      while (vguideunit < 65536) {
        const nexth = vguideh * 2;
        if (nexth >= vguidetarget) break;
        vguideh = nexth;
        vguideunit *= 2;
      }
    }
    ctx.beginPath();
    for (let t=0, x=-(this.t0*bounds.width)/this.trange; x<bounds.width; t+=tguideunit, x+=tguidew) {
      ctx.moveTo(x, 0);
      ctx.lineTo(x, bounds.height);
    }
    for (let v=0, y=bounds.height+(this.v0*bounds.height)/this.vrange; y>=0; v+=vguideunit, y-=vguideh) {
      ctx.moveTo(0, y);
      ctx.lineTo(bounds.width, y);
    }
    ctx.strokeStyle = "#333";
    ctx.stroke();
    
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
      const x = Math.floor(((pt.t - this.t0) * canvas.width) / this.trange) + 0.5;
      const y = Math.floor(canvas.height - ((pt.v - this.v0) * canvas.height) / this.vrange) + 0.5;
      if (i) ctx.lineTo(x, y);
      else ctx.moveTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.stroke();
  }
  
  renderHandles(canvas, ctx, line, color) {
    ctx.fillStyle = color;
    for (let i=0; i<line.length; i++) {
      const pt = line[i];
      const x = Math.floor(((pt.t - this.t0) * canvas.width) / this.trange) + 0.5;
      const y = Math.floor(canvas.height - ((pt.v - this.v0) * canvas.height) / this.vrange) + 0.5;
      ctx.beginPath();
      ctx.arc(x, y, HANDLE_RADIUS, 0, 2 * Math.PI);
      ctx.fill();
      if (i && (i === this.env.susp)) {
        ctx.beginPath();
        ctx.arc(x, y, HANDLE_RADIUS + 2, 0, 2 * Math.PI);
        ctx.strokeStyle = "#fff";
        ctx.stroke();
      }
    }
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
  
  /* Scroll and zoom.
   * Return true if handled.
   */
  
  scroll(dx, dy) {
    let handled = false;
    if (dx) {
      this.t0 += Math.round(dx * this.trange * 0.100);
      if (this.t0 < 0) this.t0 = 0;
      this.renderSoon();
      handled = true;
      this.onTimeChange(this.t0, this.trange);
    }
    if (dy) {
      this.v0 -= Math.round(dy * this.vrange * 0.100);
      if (this.v0 < 0) this.v0 = 0;
      else if (this.v0 > 65536 - this.vrange) this.v0 = 65536 - this.vrange;
      this.renderSoon();
      handled = true;
    }
    if (!handled) return false;
    this.setTattle(null); // Could supply the original event and we could recalc this but meh. User, just jiggle the mouse to refresh it.
    return true;
  }
  
  zoom(dx, dy, ref) {
    if (!ref) ref = {
      t: this.t0 + this.trange / 2,
      v: this.v0 + this.vrange / 2,
    };
    if (dx) { // Zoom in time.
      const rel = this.t0 ? ((ref.t - this.t0) / this.trange) : 0; // If we're flush on the left already, keep it so. (typical)
      const nrange = Math.max(100, (dx < 0) ? (this.trange / 1.200) : (this.trange * 1.200));
      this.trange = nrange;
      if (this.t0) this.t0 = ref.t - rel * this.trange;
      if (this.t0 < 0) this.t0 = 0;
      this.renderSoon();
      this.onTimeChange(this.t0, this.trange);
    }
    if (dy) { // Zoom in value.
      const rel = (ref.v - this.v0) / this.vrange;
      const nrange = Math.max(100, Math.min(65536, (dy < 0) ? (this.vrange / 1.200) : (this.vrange * 1.200)));
      this.vrange = nrange;
      this.v0 = ref.v - rel * this.vrange;
      if (this.v0 < 0) this.v0 = 0;
      else if (this.v0 > 65536 - this.vrange) this.v0 = 65536 - this.vrange;
      this.renderSoon();
    }
    // Call it "handled" whether we did something or not. Don't let the browser change the global zoom (grrr why does such a thing even exist?)
    // Don't bother with the tattle. It can be wrong now due to clamping and rounding, but ordinarily it stays close. And it's just a jiggle away from exact.
    return true;
  }
  
  setTattle(pt) {
    const tattle = this.element.querySelector(".tattle");
    if (!pt) tattle.innerText = "";
    else if (this.usage === "pitchenv") tattle.innerText = `${this.centsFromValue(pt.v)} @ ${pt.t} ms`;
    else tattle.innerText = `0x${pt.v.toString(16).padStart(4, '0')} @ ${pt.t} ms`;
  }
  
  centsFromValue(v) {
    return `${(v > 0x8000) ? '+' : ''}${v - 0x8000} c`;
  }
  
  /* Events.
   ***************************************************************************************/
   
  manualEditHandle(handle) {
    if (!handle) return;
    const modal = this.dom.spawnModal(EnvPointModal);
    modal.setup(handle);
    modal.result.then(rsp => {
      if (!rsp) return;
      if (rsp === "delete") {
        let p = this.env.lo.indexOf(handle);
        if (!p) return; // Can't delete point zero.
        if ((p < 0) && this.env.hi) p = this.env.hi.indexOf(handle);
        if (p < 0) return;
        this.env.lo.splice(p, 1);
        if (this.env.hi) this.env.hi.splice(p, 1);
      } else {
        handle.t = rsp.t;
        handle.v = rsp.v;
      }
      this.cb(this.env);
      this.renderSoon();
    });
  }
   
  onVisualDown(event) {
    if (this.dragHandle) return;
    const ref = this.logicalCoordsFromEvent(event);
    
    // Control: Add point and begin dragging (hi if both).
    if (event.ctrlKey) {
      const line = this.env.hi || this.env.lo;
      let insp = 1;
      while (insp < line.length) {
        if (line[insp].t > ref.t) break;
        insp++;
      }
      this.dragTMin = line[insp-1].t;
      if (insp < line.length) this.dragTMax = line[insp].t; else this.dragTMax = 10000;
      this.dragHandle = { t: ~~ref.t, v: ~~ref.v };
      line.splice(insp, 0, this.dragHandle);
      if (line === this.env.hi) {
        let lot=ref.t, lov=ref.v;
        if (insp < this.env.lo.length) {
          const nt = (ref.t - this.env.hi[insp-1].t) / (this.env.hi[insp+1].t - this.env.hi[insp-1].t);
          const nv = (ref.v - this.env.hi[insp-1].v) / (this.env.hi[insp+1].v - this.env.hi[insp-1].v);
          lot = ~~(this.env.lo[insp-1].t + nt * (this.env.lo[insp].t - this.env.lo[insp-1].t));
          lov = ~~(this.env.lo[insp-1].v + nv * (this.env.lo[insp].v - this.env.lo[insp-1].v));
        }
        this.env.lo.splice(insp, 0, { t: lot, v: lov });
      }
      this.cb(this.env);
      this.renderSoon();
      // Pass thru -- we'll proceed to edit or drag it.
    }
    
    // Natural click: Begin dragging if in range.
    const tradius = (HANDLE_RADIUS * this.trange) / event.target.width;
    const vradius = (HANDLE_RADIUS * this.vrange) / event.target.height;
    const hienable = this.element.querySelector("input[name='velocity']")?.checked && this.env.hi;
    if (hienable && !this.dragHandle) {
      for (let i=0; i<this.env.hi.length; i++) {
        const pt = this.env.hi[i];
        if (Math.abs(ref.t - pt.t) > tradius) continue;
        if (Math.abs(ref.v - pt.v) > vradius) continue;
        this.dragHandle = pt;
        this.dragTMin = i ? this.env.hi[i - 1].t : 0;
        this.dragTMax = (i + 1 < this.env.hi.length) ? this.env.hi[i + 1].t : 10000;
        if (!pt.t && !i) this.dragTMax = 0; // Points at time zero must stay there.
        break;
      }
    }
    if (!this.dragHandle) {
      for (let i=0; i<this.env.lo.length; i++) {
        const pt = this.env.lo[i];
        if (Math.abs(ref.t - pt.t) > tradius) continue;
        if (Math.abs(ref.v - pt.v) > vradius) continue;
        this.dragHandle = pt;
        this.dragTMin = i ? this.env.lo[i - 1].t : 0;
        this.dragTMax = (i + 1 < this.env.lo.length) ? this.env.lo[i + 1].t : 10000;
        if (!pt.t && !i) this.dragTMax = 0; // Points at time zero must stay there.
        break;
      }
    }
    if (!this.dragHandle) return;
    
    // If it's a right click, launch the edit modal and don't hold on to it.
    if (event.button === 2) {
      event.stopPropagation();
      event.preventDefault();
      // There's some bug (?) in Chrome, where nixing the context menu doesn't work if we create a modal in the same frame.
      this.window.setTimeout(() => {
        this.manualEditHandle(this.dragHandle);
        this.dragHandle = null;
      }, 10);
      return;
    }
    
    this.dragVOk = true;
    if (this.zeroForeAndAft) {
      let p = this.env.lo.findIndex(pt => pt === this.dragHandle);
      if ((p < 0) && this.env.hi) p = this.env.hi.findIndex(pt => pt === this.dragHandle);
      if ((p === 0) || (p === this.env.lo.length - 1)) {
        this.dragVOk = false;
      }
    }
    this.dragDt = ref.t - this.dragHandle.t;
    this.dragDv = ref.v - this.dragHandle.v;
    this.setTattle(this.dragHandle);
    event.target.setPointerCapture(event.pointerId);
  }
  
  onVisualUp(event) {
    if (this.dragHandle) {
      this.dragHandle = null;
      this.cb(this.env);
      const ref = this.logicalCoordsFromEvent(event);
      this.setTattle(ref);
    }
  }
  
  onVisualMove(event) {
    const ref = this.logicalCoordsFromEvent(event);
    if (this.dragHandle) {
      this.setTattle(this.dragHandle);
      this.dragHandle.t = ~~(ref.t - this.dragDt);
      if (this.dragHandle.t < this.dragTMin) this.dragHandle.t = this.dragTMin;
      else if (this.dragHandle.t > this.dragTMax) this.dragHandle.t = this.dragTMax;
      if (this.dragVOk) {
        this.dragHandle.v = ~~(ref.v - this.dragDv);
        if (this.dragHandle.v < 0) this.dragHandle.v = 0;
        else if (this.dragHandle.v > 0xffff) this.dragHandle.v = 0xffff;
      } else {
        this.dragHandle.v = 0;
      }
      this.env.isDefault = false;
      this.renderSoon();
    } else {
      this.setTattle(ref);
    }
  }
  
  onVisualLeave(event) {
    this.setTattle(null);
  }
  
  onWheel(event) {
    const d = event.deltaX || event.deltaY;
    if (!d) return;
    let handled = false;
    if (event.ctrlKey) {
      const ref = this.logicalCoordsFromEvent(event);
      if (event.shiftKey) {
        if (d < 0) handled = this.zoom(-1, 0, ref);
        else handled = this.zoom(1, 0, ref);
      } else {
        if (d < 0) handled = this.zoom(0, -1, ref);
        else handled = this.zoom(0, 1, ref);
      }
    } else {
      if (event.shiftKey) {
        if (d < 0) handled = this.scroll(-1, 0);
        else handled = this.scroll(1, 0);
      } else {
        if (d < 0) handled = this.scroll(0, -1);
        else handled = this.scroll(0, 1);
      }
    }
    if (!handled) return;
    event.preventDefault();
    event.stopPropagation();
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
  
  onSuspChange() {
    const input = this.element.querySelector("input[name='susp']");
    this.env.susp = +input.value;
    this.cb(this.env);
    this.renderSoon();
  }
}
