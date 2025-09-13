/* DecalsheetEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { Decalsheet } from "./Decalsheet.js";

export class DecalsheetEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window, SharedSymbols];
  }
  constructor(element, dom, data, window, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    
    this.res = null; // REQUIRED
    this.decalsheet = null; // REQUIRED
    this.image = null; // OPTIONAL
    this.imageData = null;
    
    this.hoverDecal = null;
    this.autoDecal = null; // Populated during a click-to-auto-set operation.
    this.autoMode = ""; // "tight"|"loose"|"frame"
    this.renderTimeout = null;
    
    this.resizeObserver = new window.ResizeObserver(e => this.onResize(e));
    this.resizeObserver.observe(this.element);
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
      this.resizeObserver = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "decalsheet") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.decalsheet = new Decalsheet(res.serial);
    this.sharedSymbols.whenLoaded()
      .then(() => this.data.getImageAsync(res.rid))
      .then(image => {
        this.image = image;
        this.buildUi();
      }).catch(e => {
        // Won't be much use without an image, but do let them carry on.
        this.image = null;
        this.buildUi();
      });
  }
  
  /* UI.
   ******************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const sidebar = this.dom.spawn(this.element, "DIV", ["sidebar"]);
    
    const panelsByIndex = [];
    for (const decal of this.decalsheet.decals) {
      const panel = this.dom.spawn(sidebar, "DIV", ["panel"], {
        "on-input": e => this.onInput(e, decal),
        "on-mouseenter": e => this.onHoverDecal(decal),
        "on-mouseleave": e => this.onUnhoverDecal(decal),
      });
      panelsByIndex.push(panel);
      this.dom.spawn(panel, "DIV", ["row"],
        this.dom.spawn(null, "INPUT", { type: "text", name: "id", value: decal.id, "on-input": e => this.validateId(e) }),
        this.dom.spawn(null, "DIV", ["spacer"]),
        this.dom.spawn(null, "INPUT", { type: "button", value: "tight", "on-click": () => this.onBeginSelection("tight", decal, event) }),
        this.dom.spawn(null, "INPUT", { type: "button", value: "loose", "on-click": () => this.onBeginSelection("loose", decal, event) }),
        this.dom.spawn(null, "INPUT", { type: "button", value: "frame", "on-click": () => this.onBeginSelection("frame", decal, event) }),
        this.dom.spawn(null, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteDecal(decal) }),
      );
      this.dom.spawn(panel, "DIV", ["row"],
        this.dom.spawn(null, "INPUT", ["coords"], { type: "number", name: "x", min: 0, max: 0xffff, value: decal.x }),
        this.dom.spawn(null, "INPUT", ["coords"], { type: "number", name: "y", min: 0, max: 0xffff, value: decal.y }),
        this.dom.spawn(null, "INPUT", ["coords"], { type: "number", name: "w", min: 0, max: 0xffff, value: decal.w }),
        this.dom.spawn(null, "INPUT", ["coords"], { type: "number", name: "h", min: 0, max: 0xffff, value: decal.h }),
      );
      this.dom.spawn(panel, "INPUT", { type: "text", name: "comment", value: this.reprComment(decal.comment), "on-input": e => this.validateComment(e) });
      this.dom.spawn(panel, "DIV", ["advice", "validation"]);
    }
    
    // With the panels all created, run thru again and validate initial values.
    // This is a separate pass because they need to look at each other.
    for (let i=0; i<panelsByIndex.length; i++) {
      const decal = this.decalsheet.decals[i];
      const panel = panelsByIndex[i];
      const msg = this.getIdValidationMessage(decal.id);
      if (!msg) continue;
      panel.querySelector("input[name='id']").classList.add("invalid");
      panel.querySelector(".validation").innerText = msg;
    }
    
    this.dom.spawn(sidebar, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() });
    
    this.dom.spawn(this.element, "DIV", ["previewWrapper"],
      this.dom.spawn(null, "CANVAS", ["preview"], { "on-click": e => this.onClickPreview(e) })
    );
    
    this.onResize();
    this.renderSoon();
  }
  
  reprComment(src) {
    if (!src?.length) return "";
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += src[i].toString(16).padStart(2, '0');
    }
    return dst;
  }
  
  evalComment(src) {
    if (!src) return undefined;
    src = src.replace(/[^0-9a-fA-F]/g, "");
    const dstc = src.length >> 1;
    if (!dstc) return undefined;
    const dst = new Uint8Array(dstc);
    for (let dstp=0, srcp=0; srcp<src.length; dstp++, srcp+=2) {
      dst[dstp] = parseInt(src.substring(srcp, srcp+2), 16);
    }
    return dst;
  }
  
  /* Render.
   *****************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    if (!this.decalsheet) return;
    const canvas = this.element.querySelector("canvas.preview");
    
    // In every sensible case, we have an image and we use its bounds exactly.
    // But we do allow images not to exist, we have to, and in that case we make up bounds.
    // We'll use multiples of 256 that contain the known decals.
    // But also bound it to 4k or so.
    let imagew, imageh;
    if (this.image) {
      imagew = this.image.naturalWidth;
      imageh = this.image.naturalHeight;
    } else {
      imagew = 256;
      imageh = 256;
      for (const decal of this.decalsheet.decals) {
        const x = decal.x + decal.w;
        if (x > imagew) imagew = (x + 256) & ~255;
        const y = decal.y + decal.h;
        if (y > imageh) imageh = (y + 256) & ~255;
      }
      if (imagew > 4096) imagew = 4096;
      if (imageh > 4096) imageh = 4096;
    }
    canvas.width = imagew;
    canvas.height = imageh;
    
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#333"; // Fill with something a little different than the background, so you can see the border.
    ctx.fillRect(0, 0, imagew, imageh);
    if (this.image) {
      ctx.drawImage(this.image, 0, 0, imagew, imageh, 0, 0, imagew, imageh);
    }
    
    ctx.fillStyle = "#0f0";
    ctx.globalAlpha = 0.5;
    for (const decal of this.decalsheet.decals) {
      if (decal === this.hoverDecal) continue;
      if (decal === this.autoDecal) continue;
      ctx.fillRect(decal.x, decal.y, decal.w, decal.h);
    }
    
    if (this.hoverDecal) {
      ctx.fillStyle = "#0ef";
      ctx.fillRect(this.hoverDecal.x, this.hoverDecal.y, this.hoverDecal.w, this.hoverDecal.h);
    }
    ctx.globalAlpha = 1;
    
    // Don't draw (this.autoDecal) -- it's being interactively replaced, so hiding is appropriate.
  }
  
  /* Events.
   *****************************************************************************/
   
  dirty() {
    if (!this.decalsheet) return;
    this.data.dirty(this.res.path, () => this.decalsheet.encode());
  }
  
  /* Highlight this ID field if we can determine it's invalid.
   * Note that we do not prevent saving when invalid.
   * It's possible the user hasn't gotten around to defining his decal names yet, that's fine.
   */
  validateId(event) {
    let panel = event.target;
    while (panel && !panel.classList.contains("panel")) panel = panel.parentNode;
    const validation = panel?.querySelector(".validation");
    const msg = this.getIdValidationMessage(event.target.value);
    if (msg) {
      event.target.classList.add("invalid");
      if (validation) validation.innerText = msg;
    } else {
      event.target.classList.remove("invalid");
      if (validation) validation.innerText = "";
    }
  }
  
  // Empty if valid.
  getIdValidationMessage(v) {
  
    // First some gross sanity checks.
    if (!v || (typeof(v) !== "string")) return "ID required.";
    if (v.match(/[^a-zA-Z0-9_]/)) return "ID must be 1..255 or C ident.";
    
    // If the first character is a digit, they must all be digits and must be in 1..255.
    // Check collisions numerically.
    const lead = v.charCodeAt(0);
    if ((lead >= 0x30) && (lead <= 0x39)) { // Decimal integer?
      if (!v.match(/^\d+$/)) return "Malformed integer.";
      v = +v;
      if ((v < 1) || (v > 255)) return "ID must be in 1..255.";
      let samec = 0;
      for (const fld of this.element.querySelectorAll(".panel input[name='id']")) {
        const fldv = +fld.value;
        if (fldv === v) {
          if (++samec > 1) return "Duplicate ID.";
        }
      }
      return "";
    }
    
    // String IDs must be listed in shared symbols.
    const vn = this.sharedSymbols.getValue("NS", "decal", v);
    if (!vn) return "Expected 1..255 or NS_decal_ symbol.";
    if ((vn < 1) || (vn > 255)) return `NS_decal_${v} = ${vn}, must be 1..255.`;
    
    // Check for collisions by string compare, and also numeric.
    // We're not checking for decal symbols that resolve to integers that collide. Maybe we should.
    let samec = 0;
    for (const fld of this.element.querySelectorAll(".panel input[name='id']")) {
      if ((fld.value === v) || (+fld.value === vn)) {
        if (++samec > 1) return "Duplicate ID.";
      }
    }
    
    return "";
  }
  
  validateComment(event) {
    // Allow spaces; we'll harmlessly discard them at encode and the user might want them for visual separation.
    const invalid = event.target.value.match(/[^0-9a-fA-F ]/);
    if (invalid) {
      event.target.classList.add("invalid");
    } else {
      event.target.classList.remove("invalid");
    }
  }
  
  onResize(event) {
    const wrapper = this.element.querySelector(".previewWrapper");
    if (!wrapper) return;
    const canvas = wrapper.querySelector("canvas.preview");
    if (!canvas) return;
    if (this.image) {
      const bounds = wrapper.getBoundingClientRect();
      const imgw = this.image.naturalWidth;
      const imgh = this.image.naturalHeight;
      const xscale = Math.floor(bounds.width / imgw);
      const yscale = Math.floor(bounds.height / imgh);
      const scale = Math.max(1, Math.min(xscale, yscale));
      canvas.style.width = (imgw * scale) + "px";
      canvas.style.height = (imgh * scale) + "px";
    } else {
      delete canvas.style.width;
      delete canvas.style.height;
    }
  }
  
  onInput(event, decal) {
    const k = event.target?.name || "";
    if ((k === "id") || (k === "comment")) {
      // Changing id or comment definitely does not impact the preview.
    } else {
      this.renderSoon();
    }
    if (decal) {
      let v = event.target?.value || "";
      switch (k) {
        case "x": case "y": case "w": case "h": {
            v = +v;
            if (!v || (v < 0)) v = 0;
            else if (v > 0xffff) v = 0xffff;
          } break;
        case "comment": v = this.evalComment(v); break;
      }
      decal[k] = v;
      this.dirty();
    }
  }
  
  onAdd() {
    if (!this.decalsheet) return;
    this.decalsheet.decals.push({
      id: "",
      x: 0,
      y: 0,
      w: 0,
      h: 0,
    });
    this.buildUi(); // too heavy-handed?
    const input = this.element.querySelector(".panel:last-of-type input[name='id']");
    if (input) {
      input.focus();
      input.select();
    }
    this.dirty();
  }
  
  onDeleteDecal(decal) {
    if (!this.decalsheet) return;
    const p = this.decalsheet.decals.indexOf(decal);
    if (p < 0) return;
    this.decalsheet.decals.splice(p, 1);
    this.buildUi();
    this.dirty();
  }
  
  onHoverDecal(decal) {
    if (this.hoverDecal === decal) return;
    this.hoverDecal = decal;
    this.renderSoon();
  }
  
  onUnhoverDecal(decal) {
    if (this.hoverDecal !== decal) return;
    this.hoverDecal = null;
    this.renderSoon();
  }
  
  onBeginSelection(mode, decal, event) {
    if ((mode === this.autoMode) && (decal === this.autoDecal)) {
      this.autoMode = "";
      this.autoDecal = null;
      this.element.querySelector("input.autoSetting")?.classList.remove("autoSetting");
    } else {
      this.autoMode = mode;
      this.autoDecal = decal;
      if (event.target) event.target.classList.add("autoSetting");
    }
    this.renderSoon();
  }
  
  onClickPreview(event) {
    if (!this.autoDecal) return;
    this.element.querySelector("input.autoSetting")?.classList.remove("autoSetting");
    if (!this.decalsheet) return;
    
    const canvas = this.element.querySelector("canvas.preview");
    const bounds = canvas.getBoundingClientRect();
    let x = Math.floor(((event.x - bounds.x) * canvas.width) / bounds.width);
    let y = Math.floor(((event.y - bounds.y) * canvas.height) / bounds.height);
    const ndecal = this.autoBounds(this.image, x, y, this.autoMode);
    if (ndecal) {
      this.autoDecal.x = ndecal.x;
      this.autoDecal.y = ndecal.y;
      this.autoDecal.w = ndecal.w;
      this.autoDecal.h = ndecal.h;
      this.dirty();
    }
    
    this.autoMode = "";
    this.autoDecal = null;
    this.buildUi(); // Easier than figuring out which panel they belong to.
  }
  
  /* Auto-detect bounds.
   * This is massively expensive; we have to make two new copies of the image to get at its pixel data.
   * Yuck.
   *******************************************************************************/
   
  autoBounds(image, x, y, mode) {
    const w = image.naturalWidth;
    const h = image.naturalHeight;
    if (!this.imageData) {
      // What a pain... You can't get ImageData from an Image, you have to render it to an intermediate canvas.
      const canvas = this.dom.spawn(null, "CANVAS");
      canvas.width = w;
      canvas.height = h;
      const ctx = canvas.getContext("2d");
      ctx.drawImage(image, 0, 0, w, h, 0, 0, w, h);
      this.imageData = ctx.getImageData(0, 0, w, h);
    }
    /* Regardless of (mode), the basic algorithm:
     *  - Start with a 1x1 rectangle on the requested point, after clamping.
     *  - Expand each edge by one pixel, if that edge does not satify the mode and can expand within the image's bounds.
     *  - If nothing expanded, we're done.
     */
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    let subw=1, subh=1;
    for (;;) {
      let done = true;
      if (x && !this.autoModeSatisfied(this.imageData, x, y, 1, subh, mode, -1, 0)) { done = false; x--; subw++; }
      if (y && !this.autoModeSatisfied(this.imageData, x, y, subw, 1, mode, 0, -1)) { done = false; y--; subh++; }
      if ((x + subw < w) && !this.autoModeSatisfied(this.imageData, x + subw - 1, y, 1, subh, mode, 1, 0)) { done = false; subw++; }
      if ((y + subh < h) && !this.autoModeSatisfied(this.imageData, x, y + subh - 1, subw, 1, mode, 0, 1)) { done = false; subh++; }
      if (done) return { x, y, w: subw, h: subh };
    }
  }
  
  autoModeSatisfied(img, x, y, w, h, mode, dx, dy) {
    switch (mode) {
    
      case "tight": { // True if the next step is OOB or all transparent.
          x += dx;
          y += dy;
          if ((x < 0) || (y < 0) || (x >= img.width) || (y >= img.height)) return true;
          return this.allTransparent(img, x, y, w, h);
        }
        
      case "loose": { // True if the current step is all transparent (we won't get called if OOB).
          return this.allTransparent(img, x, y, w, h);
        }
        
      case "frame": { // True if the next step is OOB or all the same non-transparent color.
          x += dx;
          y += dy;
          if ((x < 0) || (y < 0) || (x >= img.width) || (y >= img.height)) return true;
          return this.allSameColor(img, x, y, w, h);
        }
    
    }
    return true; // Unknown mode, call it satified.
  }
  
  allTransparent(img, x, y, w, h) {
    let rowp = (y * img.width + x) * 4 + 3;
    for (let yi=h; yi-->0; rowp+=img.width*4) {
      for (let p=rowp, xi=w; xi-->0; p+=4) {
        if (img.data[p]) return false;
      }
    }
    return true;
  }
  
  allSameColor(img, x, y, w, h) {
    let rowp = (y * img.width + x) * 4;
    const r = img.data[rowp];
    const g = img.data[rowp+1];
    const b = img.data[rowp+2];
    const a = img.data[rowp+3];
    if (!a) return false;
    for (let yi=h; yi-->0; rowp+=img.width*4) {
      for (let p=rowp, xi=w; xi-->0; p+=4) {
        if (img.data[p+0] !== r) return false;
        if (img.data[p+1] !== g) return false;
        if (img.data[p+2] !== b) return false;
        if (img.data[p+3] !== a) return false;
      }
    }
    return true;
  }
}
