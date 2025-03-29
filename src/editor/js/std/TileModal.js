/* TileModal.js
 * Detailed editor for one cell of a tilesheet.
 * We update the provided model in place and coordinate changes with Data.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SharedSymbols } from "../SharedSymbols.js";

/* These are always presented as an option, whether present in the sheet or not.
 */
export const MANDATORY_TABLES = [
  "neighbors",
  "family",
  "weight",
  "physics",
];

// Tables for which we present a "New" button instead of description.
const PROPOSE_UNIQUE_ID = ["family"];

// Tables that take explicit images from macros, see below.
const MACRO_EXPLICIT_TABLES = ["neighbors", "weight"];

/* Macros, for updating multiple tiles at once with a predefined pattern.
 * Every table except (neighbors,weight) is copied verbatim from the reference tile.
 * (neighbors,weight) take their values from the macro's array, or remain untouched if that array doesn't exist.
 * The length of these arrays must be (w*h) exactly.
 * MACRO_EXPLICIT_TABLES drives that behavior; in the code it's entirely generic.
 */
const MACROS = [
  {
    name: "fat5x3",
    w: 5, h: 3,
    neighbors: [
      0x0b,0x1f,0x16,0xfe,0xfb,
      0x6b,0xff,0xd6,0xdf,0x7f,
      0x68,0xf8,0xd0,0x00,0x00,
    ],
  },
  {
    name: "fat6x3",
    w: 6, h: 3,
    neighbors: [
      0x0b,0x1f,0x16,0xfe,0xfb,0x00,
      0x6b,0xff,0xd6,0xdf,0x7f,0x00,
      0x68,0xf8,0xd0,0x7e,0xda,0x00,
    ],
  },
  {
    name: "skinny4x4",
    w: 4, h: 4,
    neighbors: [
      0x00,0x08,0x18,0x10,
      0x02,0x0a,0x1a,0x12,
      0x42,0x4a,0x5a,0x52,
      0x40,0x48,0x58,0x50,
    ],
  },
  {
    name: "square3x3",
    w: 3, h: 3,
    neighbors: [
      0x0b,0x1f,0x16,
      0x6b,0xff,0xd6,
      0x68,0xf8,0xd0,
    ],
  },
  {
    name: "exp4",
    w: 4, h: 1,
    weight: [254, 128, 64, 32],
  },
  {
    name: "exp8",
    w: 8, h: 1,
    weight: [254, 128, 64, 32, 16, 8, 2, 1],
  },
  {
    name: "eq4",
    w: 4, h: 1,
  },
  {
    name: "eq8",
    w: 8, h: 1,
  },
];

export class TileModal {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window, SharedSymbols];
  }
  constructor(element, dom, data, window, sharedSymbols) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    
    this.path = "";
    this.tilesheet = null;
    this.tileid = 0;
    this.image = null;
    this.mouseListener = null;
    this.neighborDrawMode = false;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.dropMouseListener();
  }
  
  setup(path, tilesheet, tileid, image) {
    this.sharedSymbols.whenLoaded().then(() => {
      this.path = path;
      this.tilesheet = tilesheet;
      this.tileid = tileid;
      this.image = image;
      this.populateUi();
    });
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    
    const neighborsTable = this.dom.spawn(topRow, "TABLE", ["neighbors"],
      { "on-mousedown": e => this.onNeighborsMouseDown(e) },
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", { "data-mask": 128 }),
        this.dom.spawn(null, "TD", { "data-mask": 64 }),
        this.dom.spawn(null, "TD", { "data-mask": 32 }),
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", { "data-mask": 16 }),
        this.dom.spawn(null, "TD",
          this.dom.spawn(null, "CANVAS", ["thumbnail"])
        ),
        this.dom.spawn(null, "TD", { "data-mask": 8 }),
      ),
      this.dom.spawn(null, "TR",
        this.dom.spawn(null, "TD", { "data-mask": 4 }),
        this.dom.spawn(null, "TD", { "data-mask": 2 }),
        this.dom.spawn(null, "TD", { "data-mask": 1 }),
      ),
    );
    
    this.dom.spawn(topRow, "DIV", ["tileid"]);
    
    const navTable = this.dom.spawn(topRow, "TABLE", ["nav"]);
    for (let dy=-1; dy<=1; dy++) {
      const tr = this.dom.spawn(navTable, "TR");
      for (let dx=-1; dx<=1; dx++) {
        const td = this.dom.spawn(tr, "TD");
        let value = null;
        if (dy < 0) {
          if (!dx) value = "^";
        } else if (dy === 0) {
          if (dx === -1) value = "<";
          else if (dx === 1) value = ">";
        } else {
          if (!dx) value = "v";
        }
        if (value) {
          this.dom.spawn(td, "INPUT", { type: "button", value, "on-click": () => this.onNav(dx, dy) });
        }
      }
    }
    
    this.dom.spawn(this.element, "TABLE", ["tables"]);
    
    const macros = this.dom.spawn(this.element, "DIV", ["macros"]);
    for (const macro of MACROS) {
      const button = this.dom.spawn(macros, "BUTTON", { "data-name": macro.name, "on-click": () => this.onApplyMacro(macro) });
      this.dom.spawn(button, "CANVAS", ["macroPreview"]);
      this.dom.spawn(button, "LABEL", macro.name);
    }
  }
  
  populateUi() {
    if (!this.tilesheet) return;
    
    this.element.querySelector(".tileid").innerText = "0x" + this.tileid.toString(16).padStart(2, '0');
    
    if (this.image) {
      const thumbnail = this.element.querySelector("canvas.thumbnail");
      thumbnail.width = this.image.naturalWidth >> 4;
      thumbnail.height = this.image.naturalHeight >> 4;
      const ctx = thumbnail.getContext("2d");
      ctx.clearRect(0, 0, thumbnail.width, thumbnail.height);
      const srcx = (this.tileid & 15) * thumbnail.width;
      const srcy = (this.tileid >> 4) * thumbnail.height;
      ctx.drawImage(this.image, srcx, srcy, thumbnail.width, thumbnail.height, 0, 0, thumbnail.width, thumbnail.height);
    }
    
    this.populateNeighborsTable();
    
    const tables = this.element.querySelector(".tables");
    tables.innerHTML = "";
    for (const k of Array.from(new Set([
      ...Object.keys(this.tilesheet.tables),
      ...MANDATORY_TABLES,
    ])).sort()) {
      const value = this.tilesheet.tables[k]?.[this.tileid] || 0;
      const tr = this.dom.spawn(tables, "TR", { "data-key": k });
      this.dom.spawn(tr, "TD", ["key"], k);
      this.dom.spawn(tr, "TD", ["value"],
        this.dom.spawn(null, "INPUT", {
          type: "number", min: 0, max: 255, name: k, value,
          "on-input": e => this.onInput(e),
        })
      );
      if (PROPOSE_UNIQUE_ID.includes(k)) {
        this.dom.spawn(tr, "TD", ["desc"],
          this.dom.spawn(null, "INPUT", { type: "button", value: "New", "on-click": () => this.onUniqueId(k) })
        );
      } else {
        this.dom.spawn(tr, "TD", ["desc"], this.describeValue(k, value));
      }
    }
    
    this.populateMacroPreviews();
  }
  
  populateNeighborsTable() {
    const neighbors = this.tilesheet.tables.neighbors?.[this.tileid] || 0;
    for (let mask=0x80; mask; mask>>=1) {
      const td = this.element.querySelector(`table.neighbors td[data-mask='${mask}']`);
      if (neighbors & mask) td.classList.add("present");
      else td.classList.remove("present");
    }
  }
  
  populateMacroPreviews() {
    const x = this.tileid & 15;
    const y = this.tileid >> 4;
    const tilesize = Math.max(1, (this.image?.naturalWidth || 0) >> 4);
    for (const macro of MACROS) {
      const canvas = this.element.querySelector(`.macros button[data-name='${macro.name}'] canvas.macroPreview`);
      if (!canvas) continue;
      const ctx = canvas.getContext("2d");
      
      if ((x + macro.w > 16) || (y + macro.h > 16) || !this.image) {
        canvas.width = 16;
        canvas.height = 16;
        ctx.clearRect(0, 0, 16, 16);
        ctx.beginPath();
        ctx.moveTo(0, 0);
        ctx.lineTo(16, 16);
        ctx.moveTo(16, 0);
        ctx.lineTo(0, 16);
        ctx.strokeStyle = "#f00";
        ctx.stroke();
        continue;
      }
      
      canvas.width = tilesize * macro.w;
      canvas.height = tilesize * macro.h;
      ctx.fillStyle = "#ccc"; // Opaque non-white background so we can see eg if an entire column is blank.
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.drawImage(this.image, x * tilesize, y * tilesize, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height);
    }
  }
  
  describeValue(k, v) {
    if (PROPOSE_UNIQUE_ID.includes(k)) return "";
    return this.sharedSymbols.getName("NS", k, v);
  }
  
  updateDesc(k) {
    if (PROPOSE_UNIQUE_ID.includes(k)) return;
    const element = this.element.querySelector(`tr[data-key='${k}'] td.desc`);
    if (!element) return;
    const v = this.tilesheet?.tables[k]?.[this.tileid] || 0;
    element.innerText = this.describeValue(k, v);
  }
  
  /* Events.
   *************************************************************************/
   
  dirty() {
    if (!this.tilesheet) return;
    this.data.dirty(this.path, () => this.tilesheet.encode());
  }
   
  dropMouseListener() {
    if (!this.mouseListener) return;
    this.window.removeEventListener("mousemove", this.mouseListener);
    this.window.removeEventListener("mouseup", this.mouseListener);
    this.mouseListener = null;
  }
  
  onNeighborsMouseMoveOrUp(event) {
    if (event.type === "mouseup") {
      this.dropMouseListener();
      return;
    }
    const mask = +event.target?.getAttribute("data-mask");
    if (!mask) return;
    const t = this.tilesheet.tables.neighbors;
    if (this.neighborDrawMode) {
      if (t[this.tileid] & mask) return;
      t[this.tileid] |= mask;
    } else {
      if (!(t[this.tileid] & mask)) return;
      t[this.tileid] &= ~mask;
    }
    this.populateNeighborsTable();
    this.element.querySelector("input[name='neighbors']").value = t[this.tileid];
    this.updateDesc("neighbors");
    this.dirty();
  }
  
  onNeighborsMouseDown(event) {
    if (this.mouseListener) return;
    if (!this.tilesheet) return;
    const mask = +event.target.getAttribute("data-mask");
    if (!mask) return;
    if (!this.tilesheet.tables.neighbors) {
      this.tilesheet.tables.neighbors = new Uint8Array(256);
    }
    this.neighborDrawMode = !(this.tilesheet.tables.neighbors[this.tileid] & mask);
    this.mouseListener = e => this.onNeighborsMouseMoveOrUp(e);
    this.window.addEventListener("mousemove", this.mouseListener);
    this.window.addEventListener("mouseup", this.mouseListener);
    this.mouseListener(event); // effect the initial change
  }
  
  onNav(dx, dy) {
    let x = (this.tileid & 15) + dx;
    let y = (this.tileid >> 4) + dy;
    if ((x < 0) || (y < 0) || (x >= 16) || (y >= 16)) return;
    this.tileid = (y << 4) | x;
    this.populateUi();
  }
  
  onInput(event) {
    if (!this.tilesheet) return;
    const k = event.target.name;
    const v = +event.target.value;
    if ((typeof(v) !== "number") || (v < 0) || (v > 0xff)) return;
    let table = this.tilesheet.tables[k];
    if (!table) table = this.tilesheet.tables[k] = new Uint8Array(256);
    if (table[this.tileid] === v) return;
    table[this.tileid] = v;
    this.updateDesc(k);
    this.dirty();
  }
  
  onApplyMacro(macro) {
    if (!this.tilesheet) return;
    const x = this.tileid & 15;
    const y = this.tileid >> 4;
    if ((x + macro.w > 16) || (y + macro.h > 15)) {
      return this.dom.modalError(`Can't use ${JSON.stringify(macro.name)}, it exceeds the right or bottom edge.`);
    }
    for (const tname of Object.keys(this.tilesheet.tables)) {
      if (MACRO_EXPLICIT_TABLES.includes(tname)) continue;
      const table = this.tilesheet.tables[tname];
      this.copyCell(x, y, macro.w, macro.h, table);
    }
    for (const tname of MACRO_EXPLICIT_TABLES) {
      const src = macro[tname];
      if (!src) continue;
      let table = this.tilesheet.tables[tname];
      if (!table) table = this.tilesheet.tables[tname] = new Uint8Array(256);
      this.applyExplicitTable(table, src, x, y, macro.w, macro.h);
    }
    this.populateUi();
    this.dirty();
  }
  
  copyCell(x, y, w, h, table) {
    let dstrowp = y * 16 + x;
    const v = table[dstrowp];
    for (let yi=h; yi-->0; dstrowp+=16) {
      for (let dstp=dstrowp, xi=w; xi-->0; dstp++) {
        table[dstp] = v;
      }
    }
  }
  
  applyExplicitTable(table, src, x, y, w, h) {
    for (let dstrowp=y*16+x, srcp=0, yi=h; yi-->0; dstrowp+=16) {
      for (let dstp=dstrowp, xi=w; xi-->0; dstp++, srcp++) {
        table[dstp] = src[srcp];
      }
    }
  }
  
  onUniqueId(tname) {
    if (!this.tilesheet) return;
    let table = this.tilesheet.tables[tname];
    if (!table) table = this.tilesheet.tables[tname] = new Uint8Array(256);
    try {
      const v = this.uniqueId(table);
      table[this.tileid] = v;
      this.dirty();
      this.element.querySelector(`input[name='${tname}']`).value = v;
      // No need to update other UI; tables with the unique ID option don't show a description, and no other table changed.
    } catch (e) {
      this.dom.modalError(e);
    }
  }
  
  uniqueId(table) {
    for (let id=1; id<256; id++) {
      if (table.indexOf(id) < 0) return id;
    }
    throw new Error(`Table's value space is exhausted.`);
  }
}
