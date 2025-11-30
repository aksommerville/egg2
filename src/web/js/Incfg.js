/* Incfg.js
 * Interactive input configurer.
 * Similar to Umenu, we exploit browser facilities to manage our UI.
 */
 
export class Incfg {
  constructor(rt) {
    this.rt = rt;
    
    this.gpx = 0; // Bounds of the gamepad diagram.
    this.gpy = 0;
    this.gpw = 0;
    this.gph = 0;
    this.buttons = []; // {x,y,w,h,btnid,enable,name}
    this.buttonp = -1;
    this.framec = 0;
    this.devid = null;
    this.devidDisplay = null;
    this.assignments = []; // [devid,srcbtnid,dstbtnid]
    this.timeout = 15; // Counts down. This initial timeout is for device detection; the per-button timeout is shorter.
  }
  
  // This won't get called from outside, because we've stopped input.
  stop() {
    const container = document.querySelector("#incfg");
    if (container) container.remove();
    this.rt.incfg = null; // Destroy self.
    this.rt.input.endRawMode(this.assignments);
  }
  
  // Owner will always call after instantiating us.
  start() {
    const fbouter = document.querySelector("#fbouter");
    this.canvas = document.createElement("CANVAS");
    this.canvas.id = "incfg";
    const fb = this.rt.video.canvas;
    const cbounds = fb.getBoundingClientRect();
    this.canvas.style.left = cbounds.x + "px";
    this.canvas.style.top = cbounds.y + "px";
    this.canvas.style.width = cbounds.width + "px";
    this.canvas.style.height = cbounds.height + "px";
    // For aesthetic consistency, match the game's framebuffer size.
    this.canvas.width = fb.width;
    this.canvas.height = fb.height;
    fbouter.appendChild(this.canvas);
    this.layout();
    
    const incfgMask = this.rt.rom.getMeta("incfgMask") || "dswen123lrLR";
    let incfgNames = +this.rt.rom.getMeta("incfgNames");
    const sortedButtons = [];
    const yoinkButton = (btnid) => {
      if (!btnid) return;
      const p = this.buttons.findIndex(b => b.btnid === btnid);
      if (p < 0) return;
      const button = this.buttons[p];
      this.buttons.splice(p, 1);
      button.enable = true;
      if (incfgNames) button.name = this.rt.rom.getString(this.rt.lang, 1, incfgNames);
      sortedButtons.push(button);
      return button;
    };
    for (const ch of incfgMask) {
      let btnid=0;
      if (ch === "d") {// dpad is 4 buttons
        let name = "";
        if (incfgNames) {
          name = this.rt.rom.getString(this.rt.lang, 1, incfgNames);
          incfgNames++;
        }
        yoinkButton(0x0001).name = name;
        yoinkButton(0x0002).name = name;
        yoinkButton(0x0004).name = name;
        yoinkButton(0x0008).name = name;
      } else { // All the rest are just one.
        let name="", button=null;
        if (incfgNames) {
          name = this.rt.rom.getString(this.rt.lang, 1, incfgNames);
          incfgNames++;
        }
        switch (ch) {
          case "s": button = yoinkButton(0x0010); break;
          case "w": button = yoinkButton(0x0020); break;
          case "e": button = yoinkButton(0x0040); break;
          case "n": button = yoinkButton(0x0080); break;
          case "l": button = yoinkButton(0x0100); break;
          case "r": button = yoinkButton(0x0200); break;
          case "L": button = yoinkButton(0x0400); break;
          case "R": button = yoinkButton(0x0800); break;
          case "1": button = yoinkButton(0x1000); break;
          case "2": button = yoinkButton(0x2000); break;
          case "3": button = yoinkButton(0x4000); break;
        }
        if (button) button.name = name;
      }
    }
    for (const button of this.buttons) sortedButtons.push(button);
    this.buttons = sortedButtons;
    
    this.rt.input.beginRawMode((devid, btnid, value) => this.onRawInput(devid, btnid,value));
  }
  
  /* UI.
   ***************************************************************************/
   
  layout() {
    const fbw=this.canvas.width, fbh=this.canvas.height;
    this.gpw = Math.max(40, Math.floor(fbw / 3));
    this.gph = this.gpw >> 1;
    this.gpx = (fbw >> 1) - (this.gpw >> 1) + 0.5;
    this.gpy = (fbh >> 1) - (this.gph >> 1) + 0.5;
    
    // Establish some guides.
    const dpadl  = this.gpx + Math.floor((this.gpw * 1) / 16);
    const lr     = this.gpx + Math.floor((this.gpw * 2) / 16);
    const dpadml = this.gpx + Math.floor((this.gpw * 3) / 16);
    const dpadmr = this.gpx + Math.floor((this.gpw * 5) / 16);
    const auxl   = this.gpx + Math.floor((this.gpw * 6) / 16);
    const dpadr  = this.gpx + Math.floor((this.gpw * 7) / 16);
    const midx   = this.gpx + Math.floor((this.gpw * 8) / 16);
    const btnl   = this.gpx + Math.floor((this.gpw * 9) / 16);
    const auxr   = this.gpx + Math.floor((this.gpw *10) / 16);
    const btnml  = this.gpx + Math.floor((this.gpw *11) / 16);
    const btnmr  = this.gpx + Math.floor((this.gpw *13) / 16);
    const rl     = this.gpx + Math.floor((this.gpw *14) / 16);
    const btnr   = this.gpx + Math.floor((this.gpw *15) / 16);
    const trt    = this.gpy - Math.floor((this.gph * 1) / 8); // Triggers extend above the shell.
    const dpadt  = this.gpy + Math.floor((this.gph * 1) / 8);
    const dpadmt = this.gpy + Math.floor((this.gph * 3) / 8);
    const dpadmb = this.gpy + Math.floor((this.gph * 5) / 8);
    const auxt   = this.gpy + Math.floor((this.gph * 6) / 8);
    const dpadb  = this.gpy + Math.floor((this.gph * 7) / 8);
    
    this.buttons.push({ btnid:0x0001, x:dpadl, y:dpadmt, w:dpadml-dpadl, h:dpadmb-dpadmt });
    this.buttons.push({ btnid:0x0002, x:dpadmr, y:dpadmt, w:dpadr-dpadmr, h:dpadmb-dpadmt });
    this.buttons.push({ btnid:0x0004, x:dpadml, y:dpadt, w:dpadmr-dpadml, h:dpadmt-dpadt });
    this.buttons.push({ btnid:0x0008, x:dpadml, y:dpadmb, w:dpadmr-dpadml, h:dpadb-dpadmb });
    this.buttons.push({ btnid:0x0010, x:btnml, y:dpadmb, w:btnmr-btnml, h:dpadb-dpadmb });
    this.buttons.push({ btnid:0x0020, x:btnl, y:dpadmt, w:btnml-btnl, h:dpadmb-dpadmt });
    this.buttons.push({ btnid:0x0040, x:btnmr, y:dpadmt, w:btnr-btnmr, h:dpadmb-dpadmt });
    this.buttons.push({ btnid:0x0080, x:btnml, y:dpadt, w:btnmr-btnml, h:dpadmt-dpadt });
    this.buttons.push({ btnid:0x0100, x:this.gpx, y:trt, w:lr-this.gpx, h:this.gpy-trt });
    this.buttons.push({ btnid:0x0200, x:rl, y:trt, w:this.gpx+this.gpw-rl, h:this.gpy-trt });
    this.buttons.push({ btnid:0x0400, x:dpadml, y:trt, w:dpadmr-dpadml, h:this.gpy-trt });
    this.buttons.push({ btnid:0x0800, x:btnml, y:trt, w:btnmr-btnml, h:this.gpy-trt });
    this.buttons.push({ btnid:0x1000, x:midx, y:auxt, w:auxr-midx, h:dpadb-auxt });
    this.buttons.push({ btnid:0x2000, x:auxl, y:auxt, w:midx-auxl, h:dpadb-auxt });
    this.buttons.push({ btnid:0x4000, x:dpadr, y:dpadt, w:btnl-dpadr, h:Math.floor(this.gph/8) });
  }
  
  /* Update.
   ********************************************************************************/
   
  advance() {
    if (!this.devid) {
      // We land here if timing out without a device. No worries, just pretend we've completed the set.
      this.buttonp = this.buttons.length;
      return;
    }
    for (;;) {
      this.buttonp++;
      if (this.buttonp >= this.buttons.length) return; // Let update() stop us.
      const button = this.buttons[this.buttonp];
      if (!button.enable || button.done) continue;
      this.timeout = 10; // Per-button timeout.
      break;
    }
  }
  
  update(elapsed) {
    if (this.buttonp >= this.buttons.length) {
      this.stop();
      return;
    }
    if ((this.timeout -= elapsed) <= 0.0) {
      this.advance();
      return;
    }
  }
  
  onRawInput(devid, btnid, value) {
    
    // If a zero value slips thru, ignore it.
    if (!value) return;
    
    // If we haven't got a device yet, now we do. Do not process the stroke.
    if (!this.devid) {
      this.devid = devid;
      this.devidDisplay = devid.split("(")[0].trim() || "Unknown device";
      this.advance();
      return;
    }
    
    // Waiting for a button? Assign it.
    const button = this.buttons[this.buttonp];
    if (button) {
      const assignment = [devid, btnid, button.btnid];
      if (this.rt.input.buttonIsAxis(devid, btnid)) {
        switch (button.btnid) {
          case 0x0001: assignment[2] |= 0x0002; this.autoAssign(0x0002); if (value > 0) assignment[1] = -assignment[1]; break;
          case 0x0002: assignment[2] |= 0x0001; this.autoAssign(0x0001); if (value < 0) assignment[1] = -assignment[1]; break;
          case 0x0004: assignment[2] |= 0x0008; this.autoAssign(0x0008); if (value > 0) assignment[1] = -assignment[1]; break;
          case 0x0008: assignment[2] |= 0x0004; this.autoAssign(0x0004); if (value < 0) assignment[1] = -assignment[1]; break;
        }
      }
      this.assignments.push(assignment);
      this.advance();
    }
  }
  
  autoAssign(btnid) {
    const button = this.buttons.find(b => b.btnid === btnid);
    if (!button) return;
    button.done = true;
  }
  
  /* Render.
   * We are opaque. Runtime should not render the game below us.
   *******************************************************************************/
   
  render() {
    this.framec++;
    const blink = (this.framec % 60) >= 30;
    const ctx = this.canvas.getContext("2d");
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    
    this.renderShell(ctx);
    
    // Fill buttons, and stroke all except the dpad.
    let l,r,u,d;
    for (let i=0; i<this.buttons.length; i++) {
      const button = this.buttons[i];
      ctx.beginPath();
      if (button.btnid & 0x00f0) { // Thumb buttons are circles; everything else is a rectangle.
        ctx.ellipse(button.x + button.w * 0.5, button.y + button.h * 0.5, button.w * 0.5, button.h * 0.5, 0, 0, Math.PI * 2);
      } else {
        ctx.rect(button.x, button.y, button.w, button.h);
      }
      if (!button.enable) ctx.fillStyle = "#777"; // Disabled.
      else if (i === this.buttonp) ctx.fillStyle = blink ? "#ff0" : "#800"; // In progress.
      else if (i > this.buttonp) ctx.fillStyle = "#ccc"; // Pending.
      else ctx.fillStyle = "#080"; // Complete.
      ctx.fill();
      switch (button.btnid) { // Capture the dpad buttons. Stroke the rest.
        case 0x0001: l = button; break;
        case 0x0002: r = button; break;
        case 0x0004: u = button; break;
        case 0x0008: d = button; break;
        default: {
            if (button.enable) {
              ctx.strokeStyle = "#fff";
            } else {
              ctx.strokeStyle = "#666";
            }
            ctx.stroke();
          }
      }
    }
    
    // Stroke the dpad, just its combined outline.
    if (l && r && u && d) {
      const x0 = l.x;
      const x1 = u.x;
      const x2 = r.x;
      const x3 = r.x + r.w;
      const y0 = u.y;
      const y1 = l.y;
      const y2 = d.y;
      const y3 = d.y + d.h;
      ctx.beginPath();
      ctx.moveTo(x0, y1);
      ctx.lineTo(x0, y2);
      ctx.lineTo(x1, y2);
      ctx.lineTo(x1, y3);
      ctx.lineTo(x2, y3);
      ctx.lineTo(x2, y2);
      ctx.lineTo(x3, y2);
      ctx.lineTo(x3, y1);
      ctx.lineTo(x2, y1);
      ctx.lineTo(x2, y0);
      ctx.lineTo(x1, y0);
      ctx.lineTo(x1, y1);
      ctx.lineTo(x0, y1);
      ctx.strokeStyle = "#fff";
      ctx.stroke();
    }
    
    // Name of device, centered above the diagram.
    if (this.devid) {
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillStyle = "#ff0";
      ctx.fillText(this.devidDisplay, this.canvas.width >> 1, this.gpy >> 1);
    }
    
    // Name of next button, centered below the diagram.
    if ((this.buttonp >= 0) && (this.buttonp < this.buttons.length)) {
      const button = this.buttons[this.buttonp];
      if (button.name) {
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillStyle = "#ff0";
        const y = this.gpy + this.gph + (this.canvas.height - this.gph - this.gpy) / 3;
        ctx.fillText(button.name, this.canvas.width >> 1, y);
      }
    }
    
    // Timeout below the button name, if under 5s.
    if (this.timeout < 5) {
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillStyle = "#f88";
      const y = this.gpy + this.gph + ((this.canvas.height - this.gph - this.gpy) * 2) / 3;
      ctx.fillText(Math.ceil(this.timeout).toString(), this.canvas.width >> 1, y);
    }
  }
  
  renderShell(ctx) {
    ctx.beginPath();
    ctx.rect(this.gpx, this.gpy, this.gpw, this.gph);
    ctx.fillStyle = "#888";
    ctx.fill();
    ctx.strokeStyle = "#fff";
    ctx.stroke();
  }
}
