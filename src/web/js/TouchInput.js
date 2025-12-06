/* TouchInput.js
 * Manages a fake finger-touchy gamepad for mobile devices.
 * Created by Input when it chooses. We only exist when we ought to be running.
 */
 
export class TouchInput {
  constructor(input) {
    this.input = input;
    this.onEvent = e => {}; // Caller should replace. e = {btnid,value}
    this.renderTimeout = null;
    this.buttons = []; // {btnid:u16,path:Path2D,state:boolean}
    this.size = [0, 0]; // [w,h]. We'll rebuild (buttons) at render if it doesn't match.
    this.state = 0;
    
    /* My phone evidently doesn't support. Desktop Chrome gives a sensible rejection, so we do appear to have the semantics right.
    console.log(`locking landscape orientation...`);
    if (!screen?.orientation?.lock?.("landscape").then(() => {
      console.log(`...locked`);
      this.buildUi();
    }).catch(e => {
      console.log(`...failed to lock orientation`, e);
    })) {
      console.log(`Screen orientation not available.`);
    }
    /**/
    /* Per StackOverflow, the right way to force landscape mode is with a WebApp Manifest: https://stackoverflow.com/questions/14360581/force-landscape-orientation-mode
     * I really don't want to add another file to the bundle. Especially for a feature that I really don't give a shit about.
     */
    this.buildUi();
  }
  
  stop() {
    if (this.renderTimeout) {
      window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    const element = document.getElementById("touch");
    if (element) {
      element.remove();
    }
  }
  
  /* UI.
   ******************************************************************************/
  
  buildUi() {
    const container = document.createElement("CANVAS");
    container.id = "touch";
    container.addEventListener("touchstart", e => this.onTouch(e));
    container.addEventListener("touchend", e => this.onTouch(e));
    container.addEventListener("touchmove", e => this.onTouch(e));
    document.body.appendChild(container);
    this.renderSoon();
  }
  
  rebuildButtons(w, h) {
    
    /* Button states get dropped on a rebuild.
     * Report any releases.
     */
    for (const button of this.buttons) {
      if (button.state) {
        if (!(this.state & button.btnid)) continue; // Shouldn't happen; there should only be one button per bit.
        this.state &= ~button.btnid;
        this.onEvent({ btnid: button.btnid, value: 0 });
      }
    }
    
    /* Record bounds and wipe the button list.
     */
    this.size[0] = w;
    this.size[1] = h;
    this.buttons = [];
    this.state = 0;
    
    /* Which buttons does the game use? We'll only create UI for those.
     */
    const incfgMask = this.input.rt.rom.getMeta("incfgMask") || "dswen123lrLR";
    
    /* Dpad on the left.
     */
    if (incfgMask.indexOf("d") >= 0) {
      const dpw = Math.max(100, Math.floor(w / 4));
      const dph = dpw;
      const dpx = [0, Math.round(dpw / 3), Math.round(dpw * 0.5), Math.round((dpw * 2) / 3), dpw];
      const dpy = [h - dph]; //[(h >> 1) - (dph >> 1)];
      dpy.push(dpy[0] + Math.round(dph / 3));
      dpy.push(dpy[0] + Math.round(dph * 0.5));
      dpy.push(dpy[0] + Math.round((dph * 2) / 3));
      dpy.push(dpy[0] + dph);
      { // Left.
        const path = new Path2D();
        path.moveTo(dpx[0], dpy[3]);
        path.lineTo(dpx[0], dpy[1]);
        path.lineTo(dpx[1], dpy[1]);
        path.lineTo(dpx[2], dpy[2]);
        path.lineTo(dpx[1], dpy[3]);
        path.lineTo(dpx[0], dpy[3]);
        this.buttons.push({ btnid: 0x0001, path, state: false });
      }
      { // Right.
        const path = new Path2D();
        path.moveTo(dpx[4], dpy[1]);
        path.lineTo(dpx[4], dpy[3]);
        path.lineTo(dpx[3], dpy[3]);
        path.lineTo(dpx[2], dpy[2]);
        path.lineTo(dpx[3], dpy[1]);
        path.lineTo(dpx[4], dpy[1]);
        this.buttons.push({ btnid: 0x0002, path, state: false });
      }
      { // Up.
        const path = new Path2D();
        path.moveTo(dpx[1], dpy[0]);
        path.lineTo(dpx[3], dpy[0]);
        path.lineTo(dpx[3], dpy[1]);
        path.lineTo(dpx[2], dpy[2]);
        path.lineTo(dpx[1], dpy[1]);
        path.lineTo(dpx[1], dpy[0]);
        this.buttons.push({ btnid: 0x0004, path, state: false });
      }
      { // Down.
        const path = new Path2D();
        path.moveTo(dpx[3], dpy[4]);
        path.lineTo(dpx[1], dpy[4]);
        path.lineTo(dpx[1], dpy[3]);
        path.lineTo(dpx[2], dpy[2]);
        path.lineTo(dpx[3], dpy[3]);
        path.lineTo(dpx[3], dpy[4]);
        this.buttons.push({ btnid: 0x0008, path, state: false });
      }
      //TODO Corner buttons for the diagonals. Tricky! They'll have two bits in btnid.
    }
    
    /* Thumb buttons.
     */
    const thw = Math.max(100, Math.floor(w / 4));
    const thh = thw;
    const thx0 = w - thw;
    const thy0 = h - thh;
    const thr = thw / 6;
    const thx = [thx0 + thr, thx0 + thw * 0.5, thx0 + thw - thr];
    const thy = [thy0 + thr, thy0 + thh * 0.5, thy0 + thh - thr];
    if (incfgMask.indexOf("s") >= 0) { // South.
      const path = new Path2D();
      path.ellipse(thx[1], thy[2], thr, thr, 0, 0, Math.PI * 2);
      this.buttons.push({ btnid: 0x0010, path, state: false });
    }
    if (incfgMask.indexOf("w") >= 0) { // West.
      const path = new Path2D();
      path.ellipse(thx[0], thy[1], thr, thr, 0, 0, Math.PI * 2);
      this.buttons.push({ btnid: 0x0020, path, state: false });
    }
    if (incfgMask.indexOf("e") >= 0) { // East.
      const path = new Path2D();
      path.ellipse(thx[2], thy[1], thr, thr, 0, 0, Math.PI * 2);
      this.buttons.push({ btnid: 0x0040, path, state: false });
    }
    if (incfgMask.indexOf("n") >= 0) { // North.
      const path = new Path2D();
      path.ellipse(thx[1], thy[0], thr, thr, 0, 0, Math.PI * 2);
      this.buttons.push({ btnid: 0x0080, path, state: false });
    }
    
    /* Auxes and triggers.
     * These will all go along the top row: L1 L2 AUX3 AUX2 AUX1 R2 R1
     */
    const topRowButtons = "lL321Rr";
    const ty = 1;
    const th = Math.max(30, Math.round(h / 8)); // height divisor is entirely arbitrary.
    for (let i=0; i<topRowButtons.length; i++) {
      const btnName = topRowButtons[i];
      if (incfgMask.indexOf(btnName) < 0) continue;
      let btnid = 0;
      switch (btnName) {
        case "l": btnid = 0x0100; break;
        case "r": btnid = 0x0200; break;
        case "L": btnid = 0x0400; break;
        case "R": btnid = 0x0800; break;
        case "1": btnid = 0x1000; break;
        case "2": btnid = 0x2000; break;
        case "3": btnid = 0x4000; break;
        default: continue;
      }
      const xa = Math.floor((i * w) / topRowButtons.length) + 1;
      const xz = Math.floor(((i+1) * w) / topRowButtons.length) - 1;
      const path = new Path2D();
      path.rect(xa, ty, xz - xa, th);
      this.buttons.push({ btnid, path, state: false });
    }
  }
  
  findButtonAtPoint(x, y) {
    const canvas = document.getElementById("touch");
    if (!canvas) return null;
    const ctx = canvas.getContext("2d");
    return this.buttons.find(button => ctx.isPointInPath(button.path, x, y));
  }
  
  /* Render.
   *******************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 100);
  }
  
  renderNow() {
    const canvas = document.getElementById("touch");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    
    if ((this.size[0] !== bounds.width) || (this.size[1] !== bounds.height)) {
      this.rebuildButtons(bounds.width, bounds.height);
    }
    
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    
    ctx.lineWidth = 3;
    ctx.fillStyle = "#fffc";
    ctx.strokeStyle = "#8888";
    for (const button of this.buttons) {
      if (button.state) {
        ctx.fill(button.path);
      } else {
        ctx.stroke(button.path);
      }
    }
  }
  
  /* Events.
   * One handler for all of: touchstart, touchend, touchmove.
   * We reassess the entire state of all touches on each event.
   *******************************************************************/
   
  onTouch(event) {
    event.stopPropagation();
    event.preventDefault();
    const touchedButtons = [];
    for (const touch of event?.touches || []) {
      const button = this.findButtonAtPoint(touch.clientX, touch.clientY);
      if (!button) continue;
      if (touchedButtons.includes(button)) continue;
      touchedButtons.push(button);
    }
    let dirty = false; // True if any of our button states changed -- not necessarily the reported state.
    for (const button of this.buttons) {
      if (touchedButtons.includes(button)) {
        if (button.state) continue; // Still pressed.
        dirty = true;
        button.state = true;
        if (!(this.state & button.btnid)) {
          this.state |= button.btnid;
          this.onEvent({ btnid: button.btnid, value: 1 });
        }
      } else {
        if (!button.state) continue; // Still released.
        dirty = true;
        button.state = false;
        if (this.state & button.btnid) {
          this.state &= ~button.btnid;
          this.onEvent({ btnid: button.btnid, value: 0 });
        }
      }
    }
    if (dirty && !this.renderTimeout) {
      this.renderNow();
    }
  }
}
