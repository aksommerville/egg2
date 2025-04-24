/* TilesheetEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Tilesheet } from "./Tilesheet.js";
import { TileModal, MANDATORY_TABLES } from "./TileModal.js";

/* 256 colors, corresponding to tilesheet cell values.
 * Zero will never be rendered.
 * Populated the first time TilesheetEditor gets instantiated.
 * Try to keep the low values distinct (eg don't just use a grayscale ramp or something).
 * They repeat after 150 entries. That's not ideal, but even if they were unique, you can't really distinguish 256 unique colors at a glance, can you?
 */
const VALUE_COLORS = [];
function requireValueColors() {
  if (VALUE_COLORS.length) return;
  for (let i=0; i<256; i++) {
    let r,g,b;
    switch (i % 12) {
      case 0: r=0xff; g=0x00; b=0x00; break;
      case 1: r=0x00; g=0xff; b=0x00; break;
      case 2: r=0x00; g=0x00; b=0xff; break;
      case 3: r=0xff; g=0xff; b=0x00; break;
      case 4: r=0xff; g=0x00; b=0xff; break;
      case 5: r=0x00; g=0xff; b=0xff; break;
      case 6: r=0xff; g=0x80; b=0x00; break;
      case 7: r=0xff; g=0x00; b=0x80; break;
      case 8: r=0x80; g=0xff; b=0x00; break;
      case 9: r=0x80; g=0x00; b=0xff; break;
      case 10:r=0x00; g=0xff; b=0x80; break;
      case 11:r=0x00; g=0x80; b=0xff; break;
    }
    const shade = 1 - Math.min(1, Math.max(0.125, i / 192)); // let it saturate at both ends, and run high to low.
    r = Math.floor(shade * r);
    g = Math.floor(shade * g);
    b = Math.floor(shade * b);
    VALUE_COLORS.push(`rgb(${r},${g},${b})`);
  }
}

export class TilesheetEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, "nonce", Window];
  }
  constructor(element, dom, data, nonce, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.nonce = nonce;
    this.window = window;
    
    requireValueColors();
    
    this.res = null;
    this.tilesheet = null;
    this.visibleTables = []; // names
    this.options = { // Rendering options. Free to add whatever, then effect at renderNow().
      showImage: true,
      showGrid: true,
      numeric: false,
      bg: false,
    };
    // (layout) gets replaced at each render; it's mostly for mouse events' consumption.
    this.layout = {
      tilesize: 16, // Visible tile size, usually not interesting.
      stride: 17, // Spacing between tiles. >tilesize if (showGrid), the grid doesn't actually cover tiles.
      x0: 0, // In canvas coordinates, top left pixel of top left tile.
      y0: 0,
      srctilesize: 16,
    };
    this.renderTimeout = null;
    this.image = null;
    this.selp = -1;
    this.mousep = -1;
    this.copying = false;
    this.mouseUpListener = null;
    
    this.loadState();
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.mouseUpListener) {
      this.window.removeEventListener("mouseup", this.mouseUpListener);
      this.mouseUpListener = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "tilesheet") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.tilesheet = new Tilesheet(res.serial);
    this.image = null;
    this.data.getImageAsync(res.rid).then(image => {
      this.image = image;
      this.renderSoon();
    }).catch(e => {});
    this.populateUi();
  }
  
  static getColors() {
    requireValueColors();
    return VALUE_COLORS;
  }
  
  loadState() {
    try {
      const state = JSON.parse(this.window.localStorage.getItem("egg2.TilesheetEditor"));
      this.visibleTables = state.visibleTables || [];
      this.options = {
        showImage: true,
        showGrid: true,
        numeric: false,
        bg: false,
        ...(state.options || {}),
      };
    } catch (e) {}
  }
  
  saveState() {
    this.window.localStorage.setItem("egg2.TilesheetEditor", JSON.stringify({
      visibleTables: this.visibleTables,
      options: this.options,
    }));
  }
  
  /* UI setup.
   **************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    const toolbar = this.dom.spawn(this.element, "DIV", ["toolbar"]);
    this.dom.spawn(toolbar, "DIV", ["advice"],
      this.dom.spawn(null, "DIV", "ctl-click to select"),
      this.dom.spawn(null, "DIV", "shift-click to copy, visible tables only")
    );
    this.dom.spawn(toolbar, "DIV", ["spacer"]);
    this.dom.spawn(toolbar, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddTable() });
    const visibility = this.dom.spawn(toolbar, "DIV", ["visibility"]);
    this.dom.spawn(toolbar, "DIV", ["spacer"]);
    const options = this.dom.spawn(toolbar, "DIV", ["options"]);
    for (const k of Object.keys(this.options)) {
      const id = `TilesheetEditor-${this.nonce}-options-${k}`;
      const button = this.dom.spawn(options, "INPUT", ["toggle"], { id, name: k, type: "checkbox", "on-change": () => this.onOptionsChanged() });
      button.checked = this.options[k];
      this.dom.spawn(options, "LABEL", ["toggle"], { for: id }, k);
    }
    this.dom.spawn(toolbar, "DIV", ["tattle"], "--");
    
    this.dom.spawn(this.element, "CANVAS", ["canvas"], {
      "on-mousedown": e => this.onMouseDown(e),
      "on-contextmenu": e => { e.preventDefault(); e.stopPropagation(); },
      "on-mousemove": e => this.onMouseMove(e),
      "on-mouseenter": e => this.onMouseMove(e),
      "on-mouseleave": e => this.onMouseMove(e),
    });
  }
  
  populateUi() {
    const visibility = this.element.querySelector(".visibility");
    visibility.innerHTML = "";
    if (this.tilesheet) {
      const tables = Array.from(new Set([
        ...Object.keys(this.tilesheet.tables),
        ...MANDATORY_TABLES,
      ])).sort();
      for (const table of tables) {
        const id = `TilesheetEditor-${this.nonce}-visibility-${table}`;
        const button = this.dom.spawn(visibility, "INPUT", ["toggle"], { id, name: table, type: "checkbox", "on-change": () => this.onVisibilityChanged() });
        button.checked = this.visibleTables.includes(table);
        this.dom.spawn(visibility, "LABEL", ["toggle"], { for: id }, table);
      }
    }
    this.renderSoon();
  }
  
  /* Render.
   ************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas.canvas");
    this.recalculateLayout(canvas);
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = "#222";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    if (!this.tilesheet) return;
    
    // Background and image for each cell.
    if (this.options.bg || (this.options.showImage && this.image)) {
      for (let y=this.layout.y0, yi=16, p=0; yi-->0; y+=this.layout.stride) {
        for (let x=this.layout.x0, xi=16; xi-->0; x+=this.layout.stride, p++) {
          if (this.options.bg) {
            ctx.fillStyle = "#666";
            ctx.fillRect(x, y, this.layout.tilesize, this.layout.tilesize);
          }
          if (this.options.showImage && this.image) {
            const srcx = (p & 15) * this.layout.srctilesize;
            const srcy = (p >> 4) * this.layout.srctilesize;
            ctx.drawImage(this.image, srcx, srcy, this.layout.srctilesize, this.layout.srctilesize, x, y, this.layout.tilesize, this.layout.tilesize);
          }
        }
      }
    }
    
    // Decorations per table.
    let position = 0;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    const subsize = Math.max(1, Math.floor(this.layout.tilesize / 3));
    for (const k of this.visibleTables) {
      const table = this.tilesheet.tables[k];
      if (table) {
        let y = this.layout.y0 + (position % 3) * subsize;
        for (let yi=16, p=0; yi-->0; y+=this.layout.stride) {
          let x = this.layout.x0 + Math.floor(position / 3) * subsize;
          for (let xi=16; xi-->0; x+=this.layout.stride, p++) {
            const v = table[p];
            if (!v) continue; // Values of zero are always empty, regardless of table or render mode.
            if (this.options.numeric) this.renderNumber(ctx, x, y, subsize, v);
            else if (k === "neighbors") this.renderNeighbors(ctx, x, y, subsize, v);
            else this.renderColor(ctx, x, y, subsize, v);
          }
        }
      }
      position++;
    }
    
    // Selection.
    if ((this.selp >= 0) && (this.selp < 0x100)) {
      const x = this.layout.x0 + (this.selp & 15) * this.layout.stride;
      const y = this.layout.y0 + (this.selp >> 4) * this.layout.stride;
      ctx.fillStyle = "#0ff";
      ctx.globalAlpha = 0.5;
      ctx.fillRect(x, y, this.layout.tilesize, this.layout.tilesize);
      ctx.globalAlpha = 1;
    }
  }
  
  renderNumber(ctx, x, y, size, v) {
    ctx.fillStyle = "#fff";
    ctx.globalAlpha = 0.750;
    ctx.fillRect(x, y, size - 1, size - 1);
    ctx.fillStyle = "#000";
    ctx.globalAlpha = 1;
    ctx.fillText(v.toString(16).padStart(2, '0'), x + (size >> 1), y + (size >> 1));
  }
  
  renderColor(ctx, x, y, size, v) {
    ctx.fillStyle = VALUE_COLORS[v];
    ctx.fillRect(x, y, size - 1, size - 1);
  }
  
  renderNeighbors(ctx, x, y, size, v) {
    const halfsize = size / 2;
    const midx = x + halfsize;
    const midy = y + halfsize;
    for (let row=0, mask=0x80, yi=3; yi-->0; row++) {
      for (let col=0, xi=3; xi-->0; col++) {
        if ((row === 1) && (col === 1)) continue; // Important that we do NOT shift mask here.
        ctx.beginPath();
        ctx.moveTo(midx, midy);
        ctx.lineTo(x + halfsize * col, y + halfsize * row);
        if (v & mask) ctx.strokeStyle = "#fff";
        else ctx.strokeStyle = "#444";
        ctx.stroke();
        mask >>= 1;
      }
    }
  }
  
  recalculateLayout(canvas) {
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const narrower = Math.min(bounds.width, bounds.height);
    const stride = Math.max(2, Math.floor(narrower / 16));
    const tilesize = stride + (this.options.showGrid ? -1 : 0);
    const fullw = stride * 16;
    const fullh = stride * 16;
    const x0 = (bounds.width >> 1) - (fullw >> 1);
    const y0 = (bounds.height >> 1) - (fullh >> 1);
    let srctilesize = 16;
    if (this.image) {
      srctilesize = Math.max(1, Math.floor(Math.min(this.image.naturalWidth, this.image.naturalHeight) / 16));
    }
    this.layout = { tilesize, stride, x0, y0, srctilesize };
  }
  
  /* Events.
   ***********************************************************************/
   
  dirty() {
    if (!this.tilesheet || !this.res) return;
    this.data.dirty(this.res.path, () => this.tilesheet.encode());
  }
  
  onOptionsChanged() {
    for (const k of Object.keys(this.options)) {
      const checkbox = this.element.querySelector(`.options input[name='${k}']`);
      if (!checkbox) continue;
      this.options[k] = checkbox.checked;
    }
    this.renderSoon();
    this.saveState();
  }
  
  onVisibilityChanged() {
    this.visibleTables = Array.from(this.element.querySelectorAll(".visibility input:checked")).map(e => e.name);
    this.renderSoon();
    this.saveState();
  }
   
  onAddTable() {
    if (!this.tilesheet) return;
    //TODO Would be helpful to list the known tables per shared symbols.
    this.dom.modalText("Table name:", "").then(name => {
      if (!name) return;
      if (!name.match(/^[a-zA-Z_][0-9a-zA-Z_]{0,31}$/)) {
        return this.dom.modalError("Invalid tilesheet table name.");
      }
      if (this.tilesheet.tables[name]) {
        return this.dom.modalError(`Table ${JSON.stringify(name)} already exists.`);
      }
      this.tilesheet.tables[name] = new Uint8Array(256);
      this.visibleTables.push(name);
      this.dirty();
      this.populateUi();
    });
  }
  
  onMouseUp(event) {
    this.window.removeEventListener("mouseup", this.mouseUpListener);
    this.mouseUpListener = null;
    this.copying = false;
  }
   
  onMouseDown(event) {
    if (this.mousep < 0) return;
    if (this.mouseUpListener) return; // mouse activity already in progress, abort.
    if (!this.tilesheet) return;
    
    // Shift-drag to copy selection.
    if (event.shiftKey) {
      if (this.selp < 0) return;
      this.copying = true;
      this.mouseUpListener = e => this.onMouseUp(e);
      this.window.addEventListener("mouseup", this.mouseUpListener);
      this.copyCell();
      return;
    }
    
    // Control-click to set or unset selection.
    if (event.ctrlKey) {
      if (this.mousep === this.selp) {
        this.selp = -1;
      } else {
        this.selp = this.mousep;
      }
      this.renderSoon();
      return;
    }
    
    // Regular click to edit cell in a modal.
    const modal = this.dom.spawnModal(TileModal);
    modal.setup(this.res.path, this.tilesheet, this.mousep, this.image);
    modal.element.addEventListener("close", () => {
      this.renderSoon();
    });
  }
  
  onMouseMove(event) {
    const p = this.tileidFromEvent(event);
    if (p === this.mousep) return;
    this.mousep = p;
    this.element.querySelector(".tattle").innerText = (this.mousep < 0) ? "--" : this.mousep.toString(16).padStart(2, '0');
    if (this.copying) {
      this.copyCell();
    }
  }
  
  copyCell() {
    if ((this.selp < 0) || (this.mousep < 0) || (this.selp === this.mousep) || !this.tilesheet) return;
    let changed = false;
    for (const tname of this.visibleTables) {
      const table = this.tilesheet.tables[tname];
      if (!table) continue;
      if (table[this.mousep] === table[this.selp]) continue;
      table[this.mousep] = table[this.selp];
      changed = true;
    }
    if (changed) {
      this.dirty();
      this.renderSoon();
    }
  }
  
  tileidFromEvent(event) {
    const canvas = this.element.querySelector("canvas.canvas");
    const bounds = canvas.getBoundingClientRect();
    let x = event.x - bounds.x;
    let y = event.y - bounds.y;
    x = Math.floor((x - this.layout.x0) / this.layout.stride);
    if ((x < 0) || (x >= 16)) return -1;
    y = Math.floor((y - this.layout.y0) / this.layout.stride);
    if ((y < 0) || (y >= 16)) return -1;
    return (y << 4) | x;
  }
}
