/* EnvUi.js
 */
 
import { Dom } from "../Dom.js";
import { EnvPointModal } from "./EnvPointModal.js";

// Background guidelines, both time and value, will never be closer to each other than this, in pixels.
const GRID_MIN_SPACING = 10;

const HANDLE_RADIUS = 6;
const HANDLE_RADIUS_PLUS_FORGIVENESS = 12; // Let them be a little OOB for hit-detection purposes. (OOB clicks otherwise mean nothing)

export class EnvUi {
  static getDependencies() {
    return [HTMLElement, Dom, "nonce", Window];
  }
  constructor(element, dom, nonce, window) {
    this.element = element;
    this.dom = dom;
    this.nonce = nonce;
    this.window = window;
    
    // Owner should set directly:
    this.ondirty = v => {};
    this.ontimescale = (torigin, trange) => {}; // Notify that user has adjusted time scale. For sharing scale across multiple EnvUi.
    this.signed = false; // Doesn't affect model, just for tattling.
    
    this.name = "";
    this.renderTimeout = null;
    this.zoomMouseListener = null;
    this.zoomAnchor = [0, 0];
    this.zoomRangeBase = [1, 1]; // (trange,vrange) at start of interaction.
    this.zoomClampTime = false; // true if time was at zero initially; we'll keep the left edge clamped.
    this.dragHandle = null;
    this.dragrt = 0; // Pointer's position relative to (dragHandle), in model space.
    this.dragrv = 0;
    this.dragtlo = 0; // Constraints while dragging, per neighbor points.
    this.dragthi = 0;
    this.torigin = 0; // Time at left edge of view, ms.
    this.trange = 1000; // Width of view, ms.
    this.vorigin = 0; // Value at bottom edge of view, normal units. NB V axis runs bottom-up.
    this.vrange = 65536; // Height of view, normal units.
    this.points = []; // {t,v,line:"lo"|"hi",lockt?:boolean}. Must sort by (t) per line, but the two lines may interleave.
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(env, name) {
    this.name = name;
    this.generatePoints(env);
    this.element.innerHTML = "";
    
    this.dom.spawn(this.element, "CANVAS", ["playground"], {
      "on-pointerdown": e => this.onPlaygroundPointerDown(e),
      "on-pointerup": e => this.onPlaygroundPointerUp(e),
      "on-pointermove": e => this.onPlaygroundPointerMove(e),
      "on-pointerleave": e => this.onPlaygroundPointerMove(e),
      "on-contextmenu": e => e.preventDefault(),
      "on-wheel": e => this.onPlaygroundWheel(e),
    });
    
    const controls = this.dom.spawn(this.element, "DIV", ["row", "controls"]);
    
    this.dom.spawn(controls, "DIV", ["zoom"], {
      "on-pointerdown": e => this.onZoomPointerDown(e),
      "on-pointerup": e => this.onZoomPointerUp(e),
    }, "Z");
    
    this.dom.spawn(controls, "SELECT", { name: "line", "on-change": () => this.onLineSelectChanged() },
      this.dom.spawn(null, "OPTION", { value: "lo" }, "Low"),
      this.dom.spawn(null, "OPTION", { value: "hi" }, "High"),
    );
    
    this.dom.spawn(controls, "INPUT", { name: "susp", type: "number", min: -1, max: 15, value: env.susp, "on-input": () => this.onSuspChanged() });
    
    const velocityCheckbox = this.dom.spawn(controls, "INPUT", ["toggle", "light"], {
      name: "velocity",
      type: "checkbox",
      id: `EnvUi-${this.nonce}-velocity`,
      "on-change": () => this.onVelocityChanged(),
    });
    this.dom.spawn(controls, "LABEL", ["toggle", "light"], { for: `EnvUi-${this.nonce}-velocity` }, "Velocity");
    if (env.flags & 0x01) velocityCheckbox.checked = true;
    
    this.dom.spawn(controls, "DIV", ["tattle"]);
    
    this.renderSoon();
  }
  
  /* If you have more than one EnvUi, listen on all "ontimescale", and call this for the others.
   * That way their displayed time scales stay in sync, which can really be helpful to the user.
   */
  setTimeScale(torigin, trange) {
    this.torigin = torigin;
    this.trange = trange;
    this.renderSoon();
  }
  
  setTattle(event, bounds) {
    const tattle = this.element.querySelector(".tattle");
    if (!event) {
      tattle.innerText = "";
    } else if (this.dragHandle) {
      tattle.innerText = `Point: ${this.dragHandle.t}ms, v=0x${this.dragHandle.v.toString(16).padStart(4, '0')}`;
    } else {
      const [t, v] = this.coordsModelFromView(event.x - bounds.x, event.y - bounds.y, bounds);
      tattle.innerText = `${t}ms, v=0x${v.toString(16).padStart(4, '0')}`;
    }
  }
  
  /* Model.
   **************************************************************************************/
   
  generatePoints(env) {
    this.points = [];
    this.points.push({
      t: 0,
      v: env.initlo,
      line: "lo",
      lockt: true,
    });
    this.points.push({
      t: 0,
      v: env.inithi,
      line: "hi",
      lockt: true,
    });
    // pitchenv and initials unset, set them to 0x8000 instead of zero.
    if ((this.name === "pitchenv") && !(env.flags & 0x02)) {
      this.points[0].v = this.points[1].v = 0x8000;
    }
    let tlo=0, thi=0;
    for (const src of env.points) {
      this.points.push({
        t: tlo += src.tlo,
        v: src.vlo,
        line: "lo",
      });
      this.points.push({
        t: thi += src.thi,
        v: src.vhi,
        line: "hi",
      });
    }
  }
  
  modelFromDom() {
    const lopoints = this.points.filter(p => p.line === "lo");
    const hipoints = this.points.filter(p => p.line === "hi");
    const env = {
      flags: 0,
      initlo: lopoints.find(p => !p.t)?.v || 0,
      inithi: hipoints.find(p => !p.t)?.v || 0,
      susp: -1,
      points: [], // {tlo,thi,vlo,vhi} All u16.
    };
    const velocity = this.element.querySelector("input[name='velocity']").checked;
    if (velocity) env.flags |= 0x01;
    else env.inithi = env.initlo;
    if (env.inithi || env.initlo) env.flags |= 0x02;
    if (velocity) {
      let tlo=0, thi=0;
      for (let i=1; i<lopoints.length; i++) {
        env.points.push({
          tlo: lopoints[i].t - tlo,
          vlo: lopoints[i].v,
          thi: hipoints[i]?.t - thi || 0,
          vhi: hipoints[i]?.v || 0,
        });
        tlo = lopoints[i].t;
        thi = hipoints[i]?.t || 0;
      }
    } else {
      let tlo=0;
      for (let i=1; i<lopoints.length; i++) {
        env.points.push({
          tlo: lopoints[i].t - tlo,
          vlo: lopoints[i].v,
          thi: lopoints[i].t - tlo,
          vhi: lopoints[i].v,
        });
        tlo = lopoints[i].t;
      }
    }
    env.susp = +this.element.querySelector("input[name='susp']").value;
    if (env.susp >= env.points.length) {
      env.susp = -1;
    } else if (env.susp >= 0) {
      env.flags |= 0x04;
    }
    return env;
  }
  
  deletePointAndPartner(point) {
    const lopoints = this.points.filter(p => p.line === "lo");
    const hipoints = this.points.filter(p => p.line === "hi");
    let p = lopoints.indexOf(point);
    if (p >= 0) {
      lopoints.splice(p, 1);
      hipoints.splice(p, 1);
    } else if ((p = hipoints.indexOf(point)) >= 0) {
      lopoints.splice(p, 1);
      hipoints.splice(p, 1);
    } else return;
    this.points = [...lopoints, ...hipoints];
    this.points.sort((a, b) => a.t - b.t);
    this.renderSoon();
  }
  
  addPoint(line, t, v) {
    const otherLine = (line === "hi") ? "lo" : "hi";
    const myPoints = this.points.filter(p => p.line === line);
    const otherPoints = this.points.filter(p => p.line === otherLine);
  
    // Find the new point's neighbors on this line.
    let leftIndex=-1, rightIndex=-1, leftTime=0, rightTime=0;
    for (let i=0; i<myPoints.length; i++) {
      const point = myPoints[i];
      if (point.t <= t) {
        leftIndex = i;
        leftTime = point.t;
      } else {
        rightIndex = i;
        rightTime = point.t;
        break;
      }
    }
    
    // Compose the other-line point proportionate to neighbors if possible.
    if ((leftIndex >= 0) && (rightIndex >= 0)) {
      const tp = (t - leftTime) / (rightTime - leftTime);
      const otherLeft = otherPoints[leftIndex];
      const otherRight = otherPoints[rightIndex];
      const ot = Math.round(otherLeft.t + (otherRight.t - otherLeft.t) * tp);
      const ov = Math.round(otherLeft.v + (otherRight.v - otherLeft.v) * tp);
      this.points.push({ t: ot, v: ov, line: otherLine });
    } else if (leftIndex >= 0) {
      const otherLeft = otherPoints[leftIndex];
      const ot = otherLeft.t + t - leftTime;
      const ov = otherLeft.v;
      this.points.push({ t: ot, v: ov, line: otherLine });
    } else {
      // If we didn't acquire a "left" point something is badly broken, get out.
      return;
    }
    
    // Add the new point, sort the list, and return it.
    const point = { t, v, line };
    this.points.push(point);
    this.points.sort((a, b) => a.t - b.t);
    return point;
  }
  
  /* Render.
   **************************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas.playground");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    
    this.renderGuides(ctx, canvas);
    const velocity = this.element.querySelector("input[name='velocity']").checked;
    if (!velocity) {
      this.renderLine(ctx, canvas, "lo", true);
    } else if (this.element.querySelector("select[name='line']").value === "lo") {
      this.renderLine(ctx, canvas, "hi", false);
      this.renderLine(ctx, canvas, "lo", true);
    } else {
      this.renderLine(ctx, canvas, "lo", false);
      this.renderLine(ctx, canvas, "hi", true);
    }
  }
  
  renderGuides(ctx, canvas) {
  
    /* Vertical lines at the lowest power of two that respects GRID_MIN_SPACING.
     */
    ctx.beginPath();
    const rowhv = 2 ** Math.max(0, Math.floor(Math.log2(this.vrange / GRID_MIN_SPACING)));
    for (let v=Math.floor(this.vorigin / rowhv)*rowhv; v<this.vorigin+this.vrange; v+=rowhv) {
      const y = canvas.height - Math.floor(((v - this.vorigin) * canvas.height) / this.vrange) + 0.5;
      ctx.moveTo(0, y);
      ctx.lineTo(canvas.width, y);
    }
    ctx.strokeStyle = "#224";
    ctx.stroke();
  
    /* Horizontal lines at powers of two in milliseconds.
     * This is computationally convenient, and I think 1024 ms is close enough to 1 second to be sensible.
     */
    ctx.beginPath();
    const colwt = 2 ** Math.max(0, Math.floor(Math.log2(this.trange / GRID_MIN_SPACING)));
    for (let t=Math.floor(this.torigin / colwt)*colwt; t<this.torigin+this.trange; t+=colwt) {
      const x = Math.floor(((t - this.torigin) * canvas.width) / this.trange) + 0.5;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, canvas.height);
    }
    ctx.stroke();
  }
  
  renderLine(ctx, canvas, name, foreground) {
    const handles = [];
    ctx.beginPath();
    let started = false;
    for (const point of this.points) {
      if (point.line !== name) continue;
      const [x, y] = this.coordsViewFromModel(point.t, point.v, canvas);
      handles.push([x, y, point]);
      if (started) {
        ctx.lineTo(x, y);
      } else {
        started = true;
        ctx.moveTo(x, y);
      }
    }
    ctx.strokeStyle = foreground ? "#fff" : "#555";
    ctx.stroke();
    if (foreground) {
      ctx.fillStyle = "#f00";
      ctx.strokeStyle = "#fff";
      for (const [x, y, point] of handles) {
        ctx.beginPath();
        ctx.ellipse(x, y, HANDLE_RADIUS, HANDLE_RADIUS, 0, 0, Math.PI * 2);
        if (point === this.dragHandle) {
          ctx.fillStyle = "#ff0";
          ctx.fill();
          ctx.fillStyle = "#f00";
        } else {
          ctx.fill();
        }
        ctx.stroke();
      }
      let susp = +this.element.querySelector("input[name='susp']").value;
      if ((susp >= 0) && (susp < handles.length - 1)) { // First handle doesn't count; it's the initials.
        const [x, y] = handles[1 + susp];
        ctx.beginPath();
        ctx.ellipse(x, y, HANDLE_RADIUS + 2, HANDLE_RADIUS + 2, 0, 0, Math.PI * 2);
        ctx.stroke();
      }
    } else {
      // Draw handles for background line too, but don't make as big a deal over it.
      ctx.fillStyle = "#666";
      for (const [x, y, point] of handles) {
        ctx.beginPath();
        ctx.ellipse(x, y, HANDLE_RADIUS, HANDLE_RADIUS, 0, 0, Math.PI * 2);
        ctx.fill();
      }
    }
  }
  
  /* Projection and canvas objects.
   ************************************************************************************/
   
  findHandle(event) {
  
    /* Find the nearest point on the focussed line, working only in view space.
     * It feels wrong to project every point every time, but the alternative I guess would be working in model space.
     * And that's even worse, because its axes are not scaled symmetrically.
     */
    const bounds = event.target.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    let bestPoint = null;
    let bestDistance = 999999;
    const line = this.element.querySelector("select[name='line']").value;
    for (const point of this.points) {
      if (point.line !== line) continue; // Only the focussed line is eligible.
      const [px, py] = this.coordsViewFromModel(point.t, point.v, bounds);
      const distance = (px - x) ** 2 + (py - y) ** 2;
      if (!bestPoint || (distance < bestDistance)) {
        bestPoint = point;
        bestDistance = distance;
      }
    }
    
    // Now project the found point into view space and confirm we're within the handle radius.
    if (!bestPoint) return null;
    const distance = Math.sqrt(bestDistance);
    if (distance > HANDLE_RADIUS_PLUS_FORGIVENESS) return null;
    return bestPoint;
  }
  
  // For (x,y) in canvas space, return [t,v] in model space.
  // Provide canvas bounds if you have them.
  coordsModelFromView(x, y, bounds) {
    if (!bounds) bounds = this.element.querySelector("canvas.playground").getBoundingClientRect();
    const t = Math.max(0, Math.round(this.torigin + (this.trange * x) / bounds.width));
    y = bounds.height - y;
    const v = Math.max(0, Math.min(0xffff, Math.round(this.vorigin + (this.vrange * y) / bounds.height)));
    return [t, v];
  }
  coordsModelFromViewUnclamped(x, y, bounds) {
    if (!bounds) bounds = this.element.querySelector("canvas.playground").getBoundingClientRect();
    const t = Math.round(this.torigin + (this.trange * x) / bounds.width);
    y = bounds.height - y;
    const v = Math.round(this.vorigin + (this.vrange * y) / bounds.height);
    return [t, v];
  }
  
  coordsViewFromModel(t, v, bounds) {
    if (!bounds) bounds = this.element.querySelector("canvas.playground").getBoundingClientRect();
    const x = Math.round(((t - this.torigin) * bounds.width) / this.trange);
    const y = bounds.height - Math.round(((v - this.vorigin) * bounds.height) / this.vrange);
    return [x, y];
  }
  
  /* Events.
   *************************************************************************************/
  
  onPlaygroundPointerDown(event) {
    if (this.playgroundMouseListener) return;
    
    if (event.ctrlKey) { // Control-click anywhere: Create point.
      const bounds = this.element.querySelector("canvas.playground").getBoundingClientRect();
      const [t, v] = this.coordsModelFromView(event.x - bounds.x, event.y - bounds.y, bounds);
      const line = this.element.querySelector("select[name='line']").value;
      if ((line === "hi") && !this.element.querySelector("input[name='velocity']").checked) return;
      if (this.dragHandle = this.addPoint(line, t, v)) {
        this.dragrt = t - this.dragHandle.t;
        this.dragrv = v - this.dragHandle.v;
        this.calculateDragConstraints();
        this.setTattle(event, bounds);
        this.renderSoon();
        this.ondirty(this.modelFromDom());
        event.target.setPointerCapture(event.pointerId);
      }
      return;
    }
    
    const handle = this.findHandle(event);
    if (!handle) return; // Unmodified click outside any point: noop
    
    if (event.button === 2) { // Right-click on point: Edit in modal.
      // If we spawn it synchronously, for some reason we get the undesired context menu.
      // No idea how to suppress that the right way, so whatever, just kick the modal spawn out to the next frame.
      this.window.setTimeout(() => {
        const modal = this.dom.spawnModal(EnvPointModal);
        modal.setup(handle);
        modal.result.then(rsp => {
          if (!rsp) return;
          if (rsp === "delete") return this.deletePointAndPartner(handle);
          const p = this.points.indexOf(handle);
          if (p < 0) return;
          this.points[p] = rsp;
          this.renderSoon();
          this.ondirty(this.modelFromDom());
        });
      }, 0);
      return;
    }
    
    // Left-click on point: Drag.
    event.target.setPointerCapture(event.pointerId);
    this.dragHandle = handle;
    const bounds = event.target.getBoundingClientRect();
    const [t, v] = this.coordsModelFromView(event.x - bounds.x, event.y - bounds.y, bounds);
    this.dragrt = t - handle.t;
    this.dragrv = v - handle.v;
    this.setTattle(event, bounds);
    this.renderSoon();
    this.calculateDragConstraints();
  }
  
  // Call after setting (this.dragHandle).
  calculateDragConstraints() {
    if (!this.dragHandle) return;
    if (this.dragHandle.lockt) {
      this.dragtlo = this.dragthi = this.dragHandle.t;
    } else {
      this.dragtlo = 0;
      this.dragthi = 999999;
      for (const point of this.points) {
        if (point === this.dragHandle) continue;
        if (point.line !== this.dragHandle.line) continue;
        if (point.t < this.dragHandle.t) {
          const ck = point.t + 1;
          if (ck > this.dragtlo) this.dragtlo = ck;
        } else {
          const ck = point.t - 1;
          if (ck < this.dragthi) this.dragthi = ck;
        }
      }
    }
  }
  
  onPlaygroundPointerUp(event) {
    if (this.dragHandle) {
      this.dragHandle = null;
      this.ondirty(this.modelFromDom());
      this.renderSoon();
    }
    const bounds = event.target.getBoundingClientRect();
    if ((event.x < bounds.x) || (event.y < bounds.y) || (event.x >= bounds.x + bounds.width) || (event.y >= bounds.y + bounds.height)) {
      this.setTattle(false);
    } else {
      this.setTattle(event, bounds);
    }
  }
  
  onPlaygroundPointerMove(event) {
    const bounds = event.target.getBoundingClientRect();
    if (!this.dragHandle) {
      // It's not enough to check bounds here, we must handle "pointerleave" special.
      // Sometimes we get fractional bounds, and pointerleave arrives when the point is mathematically still in bounds.
      // That has got to be a Chrome bug, right?
      if ((event.type === "pointerleave") || (event.x < bounds.x) || (event.y < bounds.y) || (event.x >= bounds.x + bounds.width) || (event.y >= bounds.y + bounds.height)) {
        this.setTattle(false);
      } else {
        this.setTattle(event, bounds);
      }
      return;
    }
    const [t, v] = this.coordsModelFromViewUnclamped(event.x - bounds.x, event.y - bounds.y, bounds);
    this.dragHandle.t = Math.round(t - this.dragrt);
    if (this.dragHandle.t < this.dragtlo) this.dragHandle.t = this.dragtlo;
    else if (this.dragHandle.t > this.dragthi) this.dragHandle.t = this.dragthi;
    this.dragHandle.v = Math.round(v - this.dragrv);
    if (this.dragHandle.v < 0) this.dragHandle.v = 0;
    else if (this.dragHandle.v > 0xffff) this.dragHandle.v = 0xffff;
    this.renderSoon();
    this.setTattle(event, bounds);
  }
  
  onPlaygroundWheel(event) {
    // Values of deltaX and deltaY are mysterious to me. They're supposed to be pixels... I get quanta of 120, which is too much.
    // Rather than figuring out and reversing that scaling -- which I believe is impossible -- we're going to treat each event as one click of the wheel.
    const scale = 0.125; // Relative to the viewing volume.
    const d = Math.sign(event.deltaY || event.deltaX) * scale;
    if (!d) return;
    if (event.shiftKey) { // Shift: Adjust time axis ie horizontal. Clamps left only.
      this.torigin += d * this.trange;
      if (this.torigin < 0) this.torigin = 0;
      this.ontimescale(this.torigin, this.trange);
    } else { // Plain: Adjust value axis ie vertical. Clamps both sides, and in most cases you view the whole range anyway.
      this.vorigin -= d * this.vrange; // sic '-': We render the value axis bottom-up.
      if (this.vorigin < 0) this.vorigin = 0;
      else if (this.vorigin + this.vrange > 0x10000) this.vorigin = 0x10000 - this.vrange;
    }
    this.setTattle(event, event.target.getBoundingClientRect());
    this.renderSoon();
  }
  
  onZoomPointerDown(event) {
    if (this.zoomMouseListener) return;
    this.zoomMouseListener = e => this.onZoomPointerMove(e);
    event.target.addEventListener("pointermove", this.zoomMouseListener);
    event.target.setPointerCapture(event.pointerId);
    this.zoomAnchor[0] = event.x;
    this.zoomAnchor[1] = event.y;
    this.zoomRangeBase[0] = this.trange;
    this.zoomRangeBase[1] = this.vrange;
    this.zoomClampTime = !this.torigin;
  }
  
  onZoomPointerUp(event) {
    if (this.zoomMouseListener) {
      event.target.removeEventListener("pointermove", this.zoomMouseListener);
      this.zoomMouseListener = null;
    }
  }
  
  onZoomPointerMove(event) {
    const dx = event.x - this.zoomAnchor[0];
    const dy = event.y - this.zoomAnchor[1];
    const tmid = this.torigin + this.trange / 2;
    const vmid = this.vorigin + this.vrange / 2;
    // 100 pixels to double or half the viewing volume.
    // 16 as the time axis's minimum gets it in to where each guide line is 1 ms, perfect.
    // 64 for value, and guide lines are wider than 1 point, but pixels are fine enough to address every value.
    this.trange = Math.min( 4096, Math.max( 16, this.zoomRangeBase[0] * Math.pow(2, dx / -100)));
    this.vrange = Math.min(65536, Math.max( 64, this.zoomRangeBase[1] * Math.pow(2, dy / 100)));
    if (!this.zoomClampTime) {
      const o0 = this.torigin;
      this.torigin = tmid - this.trange / 2;
      if (this.torigin < 0) this.torigin = 0;
    }
    this.vorigin = vmid - this.vrange / 2;
    if (this.vorigin < 0) this.vorigin = 0;
    else if (this.vorigin + this.vrange > 0x10000) this.vorigin = 0x10000 - this.vrange;
    this.renderSoon();
    this.ontimescale(this.torigin, this.trange);
  }
  
  onLineSelectChanged() {
    this.renderSoon();
  }
  
  onSuspChanged() {
    this.renderSoon();
  }
  
  onVelocityChanged() {
    this.renderSoon();
  }
}
