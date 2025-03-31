/* MapPaint.js
 * Context for MapEditor, shareable across the various UI bits.
 * We are not a singleton! We live only as long as the MapEditor, and get injected as an override.
 * Everything interesting happens here.
 */
 
import { MapService } from "./MapService.js";
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { CommandListEditor } from "../std/CommandListEditor.js";
import { Tilesheet } from "../std/Tilesheet.js";
 
export class MapPaint {
  static getDependencies() {
    return [MapService, Dom, Data, Window];
  }
  constructor(mapService, dom, data, window) {
    this.mapService = mapService;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.mapEditor = null; // MapEditor should inject itself here during construction.
    
    this.path = "";
    this.map = null;
    this.image = null;
    this.tilesheet = null;
    this.nextListenerId = 1;
    this.listeners = [];
    this.modifiers = []; // "ctl","shift"
    this.scrollTimeout = null;
    this.mousex = -1; // integer, in map cells
    this.mousey = -1; // ''
    this.mousefx = -1; // "floating-point" or "full-precision"
    this.mousefy = -1; // ''
    this.toolInProgress = ""; // Populated while dragging in progress.
    this.anchorx = 0; // monalisa and marquee
    this.anchory = 0;
    
    this.manualTool = "rainbow";
    this.effectiveTool = this.manualTool;
    this.tileid = 0x00;
    this.tilesize = 16; // Natural size.
    this.zoom = 3; // Displayed tilesize relative to natural.
    this.scrollx = 0;
    this.scrolly = 0;
    this.toggles = {
      image: true,
      grid: true,
      poi: true,
      physics: false,
      joinOutside: false,
    };
    this.loadSettings();
  }
  
  destroy() {
    this.mapEditor = null;
    this.map = null;
    this.image = null;
    if (this.scrollTimeout) {
      this.window.clearTimeout(this.scrollTimeout);
      this.scrollTimeout = null;
    }
  }
  
  loadSettings() {
    try {
      const src = JSON.parse(this.window.localStorage.getItem("eggEditor.MapPaint"));
      if (typeof(src.manualTool) === "string") this.effectiveTool = this.manualTool = src.manualTool;
      if (typeof(src.tileid) === "number") this.tileid = src.tileid;
      if (typeof(src.tilesize) === "number") this.tilesize = src.tilesize;
      if (typeof(src.zoom) === "number") this.zoom = src.zoom;
      if (typeof(src.scrollx) === "number") this.scrollx = src.scrollx;
      if (typeof(src.scrolly) === "number") this.scrolly = src.scrolly;
      if (src.toggles && (typeof(src.toggles) === "object")) this.toggles = src.toggles;
    } catch (e) {
    }
  }
  
  saveSettings() {
    this.window.localStorage.setItem("eggEditor.MapPaint", JSON.stringify({
      manualTool: this.manualTool,
      tileid: this.tileid,
      tilesize: this.tilesize,
      zoom: this.zoom,
      scrollx: this.scrollx,
      scrolly: this.scrolly,
      toggles: this.toggles,
    }));
  }
  
  /* { type:"tool", tool:name }
   * { type:"tileid", tileid:0..255 }
   * { type:"zoom", zoom }
   * { type:"scroll", x, y } Delayed.
   * { type:"map", map, path, image:null }
   * { type:"image", image }
   * { type:"commands" }
   * { type:"toggle", k, v }
   * { type:"mouse", x, y } in map cells
   * { type:"cellDirty", x, y }
   */
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ id, cb });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  /* Editor state, data dispatch.
   *****************************************************************************************/
  
  setResource(path, map) {
    this.path = path;
    this.map = map;
    this.image = null;
    this.tilesheet = null;
    this.tilesize = 16;
    const imageName = this.map.cmd.getFirstArg("image");
    if (imageName) {
      this.data.getImageAsync(imageName).then(image => {
        this.image = image;
        this.tilesize = Math.max(1, image.naturalWidth >> 4);
        this.broadcast({ type: "image", image });
      }).catch(() => {});
      const tsres = this.data.findResourceOverridingType(imageName, "tilesheet");
      if (tsres) {
        this.tilesheet = new Tilesheet(tsres.serial);
      }
    }
    this.broadcast({ type: "map", map, path, image: null });
  }
  
  setPalette(tileid) {
    if ((typeof(tileid) !== "number") || (tileid < 0) || (tileid > 0xff)) return;
    if (tileid === this.tileid) return;
    this.tileid = tileid;
    this.saveSettings();
    this.broadcast({ type: "tileid", tileid: this.tileid });
  }
  
  setZoom(zoom) {
    if ((typeof(zoom) !== "number") || (zoom < 0.010) || (zoom > 100)) return;
    if (this.zoom === zoom) return;
    this.zoom = zoom;
    this.saveSettings();
    this.broadcast({ type: "zoom", zoom: this.zoom });
  }
  
  setScroll(x, y) {
    if ((x === this.scrollx) && (y === this.scrolly)) return;
    this.scrollx = x;
    this.scrolly = y;
    // Require one second idle before broadcasting or saving.
    if (this.scrollTimeout) this.window.clearTimeout(this.scrollTimeout);
    this.scrollTimeout = this.window.setTimeout(() => {
      this.scrollTimeout = null;
      this.broadcast({ type: "scroll", x: this.scrollx, y: this.crolly });
      this.saveSettings();
    }, 1000);
  }
  
  setToggle(k, v) {
    if (!k || (typeof(v) !== "boolean")) return;
    if (typeof(this.toggles[k]) !== "boolean") return;
    if (this.toggles[k] === v) return;
    this.toggles[k] = v;
    this.saveSettings();
    this.broadcast({ type: "toggle", k, v });
  }
  
  setTool(name) {
    if (name === this.manualTool) return;
    this.manualTool = name;
    this.effectiveTool = name;
    this.saveSettings();
    this.broadcast({ type: "tool", tool: this.effectiveTool });
  }
  
  setModifier(name, value) {
    if (!["ctl", "shift"].includes(name)) return;
    if (value) {
      if (this.modifiers.includes(name)) return;
      this.modifiers.push(name);
    } else {
      const p = this.modifiers.indexOf(name);
      if (p < 0) return;
      this.modifiers.splice(p, 1);
    }
    this.refreshEffectiveTool();
  }
  
  refreshEffectiveTool() {
    let nv = this.manualTool;
    const tool = MapPaint.TOOLS.find(t => t.name === this.manualTool);
    if (tool) {
      let ctl=false, shift=false;
      for (const mod of this.modifiers) {
        if (mod === "ctl") ctl = true;
        else if (mod === "shift") shift = true;
      }
      if (ctl && shift && tool.ctlShift) nv = tool.ctlShift;
      else if (ctl && tool.ctl) nv = tool.ctl;
      else if (shift && tool.shift) nv = tool.shift;
    }
    if (nv === this.effectiveTool) return;
    this.effectiveTool = nv;
    this.broadcast({ type: "tool", tool: nv });
  }
  
  /* Generic tools dispatch, driven by mouse events.
   * For each tool, optionally implement:
   *   {TOOLNAME}Begin(x,y): Return true to receive Update and End events.
   *   {TOOLNAME}Update(x,y): Called when cell changes.
   *   {TOOLNAME}End()
   * You'll only be called with in-bounds coordinates, always integers.
   *******************************************************************************/
  
  setMouse(x, y) {
    this.mousefx = x;
    this.mousefy = y;
    x = Math.floor(x);
    y = Math.floor(y);
    if (!this.map || (x < 0) || (x >= this.map.w) || (y < 0) || (y >= this.map.h)) x = y = -1;
    if ((x === this.mousex) && (y === this.mousey)) return false;
    this.mousex = x;
    this.mousey = y;
    this.broadcast({ type: "mouse", x, y });
    if (this.toolInProgress && (x >= 0)) {
      const fnname = this.toolInProgress + "Update";
      this[fnname]?.(this.mousex, this.mousey);
    }
    return true;
  }
  
  onMouseDown() {
    if (this.toolInProgress) return;
    if (!this.map || (this.mousex < 0) || (this.mousey < 0) || (this.mousex >= this.map.w) || (this.mousey >= this.map.h)) return;
    const fnname = this.effectiveTool + "Begin";
    if (this[fnname]?.(this.mousex, this.mousey)) {
      this.toolInProgress = this.effectiveTool;
    }
  }
  
  onMouseUp() {
    if (this.toolInProgress) {
      const fnname = this.toolInProgress + "End";
      this.toolInProgress = "";
      this[fnname]?.();
    }
  }
  
  /* Pencil: Copy palette cell verbatim.
   **********************************************************/
   
  pencilBegin(x, y) {
    this.pencilUpdate(x, y);
    return true;
  }
  
  pencilUpdate(x, y) {
    const p = y * this.map.w + x;
    if (this.map.v[p] === this.tileid) return;
    this.map.v[p] = this.tileid;
    this.broadcast({ type: "cellDirty", x, y });
  }
  
  /* Rainbow: Pencil+Heal.
   **************************************************************/
   
  rainbowBegin(x, y) {
    this.rainbowUpdate(x, y);
    return true;
  }
  
  rainbowUpdate(x, y) {
    const p = y * this.map.w + x;
    if (this.map.v[p] === this.tileid) return;
    this.map.v[p] = this.tileid;
    this.healAround(x, y);
    this.broadcast({ type: "cellDirty", x, y });
  }
  
  /* Heal: Apply join rules.
   **************************************************************/
   
  healAround(x, y) {
    for (let dy=-1; dy<=1; dy++) {
      const suby = y + dy;
      if ((suby < 0) || (suby >= this.map.h)) continue;
      for (let dx=-1; dx<=1; dx++) {
        const subx = x + dx;
        if ((subx < 0) || (subx >= this.map.w)) continue;
        this.healUpdate(subx, suby, false);
      }
    }
  }
   
  healBegin(x, y) {
    if (!this.tilesheet) return false;
    this.healUpdate(x, y);
    return true;
  }
  
  healUpdate(x, y, sendEvent=true) {
    if (!this.tilesheet?.tables.family || !this.tilesheet?.tables.neighbors) return;
    const p = y * this.map.w + x;
    const tileid = this.map.v[p];
    const family = this.tilesheet.tables.family[tileid];
    if (!family) return;
    const nmask = this.gatherNeighborMask(x, y, p, family);
    const ntileid = this.chooseTile(tileid, family, nmask);
    if (ntileid === tileid) return;
    this.map.v[p] = ntileid;
    if (sendEvent) this.broadcast({ type: "cellDirty", x, y });
  }
  
  // Choose a replacement for (tileid), which belongs to (family), given a neighbor mask.
  chooseTile(tileid, family, nmask) {
    const famv = this.tilesheet.tables.family;
    const neiv = this.tilesheet.tables.neighbors;
    const weiv = this.tilesheet.tables.weight; // OPTIONAL
    const popcnt8 = (src) => {
      let c = 0;
      if (src) for (let mask=0x80; mask; mask>>=1) {
        if (src & mask) c++;
      }
      return c;
    };
    
    /* Candidate tiles must belong to this family, and must have a neighbor mask entirely covered by (nmask).
     * (nmask) may contain bits not covered by the candidates' masks.
     * During this pass, we also determine the highest bit count among candidate neighbor masks, and exclude ones below that count.
     * We can't exclude all of them, so there's another pass after.
     */
    const forbitten = ~nmask;
    const candidatev = [];
    let popmax = 0;
    for (let ctileid=0; ctileid<0x100; ctileid++) {
      if (famv[ctileid] !== family) continue;
      if (neiv[ctileid] & forbitten) continue;
      if (weiv?.[ctileid] === 0xff) continue; // appointment only
      const pop = popcnt8(neiv[ctileid]);
      if (pop < popmax) continue;
      if (pop > popmax) popmax = pop;
      candidatev.push(ctileid);
    }
    if (popmax > 0) { // Eliminate any remaining candidates below the top bit count.
      for (let i=candidatev.length; i-->0; ) {
        const pop = popcnt8(neiv[candidatev[i]]);
        if (pop < popmax) candidatev.splice(i, 1);
      }
    }
    
    // Entirely possible to have no candidates at this point; keep whatever we had before.
    // Also very likely that there's just one candidate, in which case we have our answer.
    if (candidatev.length < 1) return tileid;
    if (candidatev.length === 1) return candidatev[0];
    
    // Multiple candidates, so select randomly among them according to their weight.
    let weightv;
    if (weiv) weightv = candidatev.map(tileid => 256 - weiv[tileid]);
    else weightv = candidatev.map(() => 1);
    let wsum = 0;
    for (const w of weightv) wsum += w;
    let decision = Math.random() * wsum;
    for (let i=0; i<candidatev.length; i++) {
      decision -= weightv[i];
      if (decision < 0) return candidatev[i];
    }
    return candidatev[0];
  }
  
  // (p,family) are knowable from (x,y) but we assume you already have them.
  gatherNeighborMask(x, y, p, family) {
    let mask = 0;
    const ck = (dx, dy, bit) => {
      let subx = x + dx;
      if (subx < 0) {
        if (this.toggles.joinOutside) subx = 0;
        else return;
      } else if (subx >= this.map.w) {
        if (this.toggles.joinOutside) subx = this.map.w - 1;
        else return;
      }
      let suby = y + dy;
      if (suby < 0) {
        if (this.toggles.joinOutside) suby = 0;
        else return;
      } else if (suby >= this.map.h) {
        if (this.toggles.joinOutside) suby = this.map.h - 1;
        else return;
      }
      const subtileid = this.map.v[suby * this.map.w + subx];
      if (this.tilesheet.tables.family[subtileid] === family) {
        mask |= bit;
      }
    };
    ck(-1, -1, 0x80);
    ck( 0, -1, 0x40);
    ck( 1, -1, 0x20);
    ck(-1,  0, 0x10);
    ck( 1,  0, 0x08);
    ck(-1,  1, 0x04);
    ck( 0,  1, 0x02);
    ck( 1,  1, 0x01);
    return mask;
  }
  
  /* Mona Lisa: Copy contiguous stretches of the tilesheet.
   ***************************************************************/
   
  monalisaBegin(x, y) {
    this.anchorx = x;
    this.anchory = y;
    this.monalisaUpdate(x, y);
    return true;
  }
  
  monalisaUpdate(x, y) {
    const dx = x - this.anchorx;
    const dy = y - this.anchory;
    const tcol = (this.tileid & 15) + dx;
    const trow = (this.tileid >> 4) + dy;
    if ((tcol < 0) || (tcol > 15) || (trow < 0) || (trow > 15)) return;
    const tileid = (trow << 4) | tcol;
    const p = y * this.map.w + x;
    if (this.map.v[p] === tileid) return;
    this.map.v[p] = tileid;
    this.broadcast({ type: "cellDirty", x, y });
  }
  
  /* Pickup: Replace palette from map.
   ****************************************************************/
   
  pickupBegin(x, y) {
    const tileid = this.map.v[y * this.map.w + x];
    this.setPalette(tileid);
    return false;
  }
  
  /* Marquee: Rectangular selection.
   **************************************************************/
   
  marqueeBegin(x, y) {
    //TODO If a selection exists, begin dragging it. Or if outside, anchor it first. If dragging with Shift, copy it down first.
    //TODO record anchor
    return true;
  }
  
  marqueeUpdate(x, y) {
    //TODO Drag selection if we're doing that.
    //TODO Compare to anchor and update provisional selection
    //TODO Update details tattle
  }
  
  marqueeEnd() {
    //TODO Commit selection (or do nothing if dragging).
  }
  
  /* Lasso: Free selection and measurement.
   ****************************************************************/
   
  lassoBegin(x, y) {
    //TODO If a selection exists, begin dragging it. Or if outside, anchor it first. If dragging with Shift, copy it down first.
    this.lassoUpdate(x, y);
    return true;
  }
  
  lassoUpdate(x, y) {
    //TODO Add to selection if absent, and add to pedometer regardless.
    //TODO Update details tattle.
  }
  
  lassoEnd() {
    //TODO Commit selection (or do nothing if dragging).
  }
  
  /* Poi Edit: Open a modal for the focussed POI.
   ***********************************************************/
   
  poieditBegin(x, y) {
    //TODO Locate POI, use this.mousefx,this.mousefy
    //TODO Modal
    return false;
  }
  
  /* Poi Move: Move or duplicate POI.
   **************************************************************/
   
  poimoveBegin(x, y) {
    //TODO Locate POI (f). Return false if none.
    //TODO If Shift, drop a copy of the POI first.
    return true;
  }
  
  poimoveUpdate(x, y) {
    //TODO Move POI.
  }
  
  /* Door: Travel through Door POI.
   **************************************************************/
   
  doorBegin(x, y) {
    //TODO Locate Door (f)
    //TODO Record destination coords in MapService, and fetch and clear those if present when loading a map.
    //TODO Open the remote map
    return false;
  }
  
  /* Impulse actions.
   *************************************************************************************/
  
  performAction(action) {
    if (!action.name.match(/^[a-z][a-zA-Z0-9_]*$/)) return;
    const fnname = "action_" + action.name;
    this[fnname]?.();
  }
  
  action_commands() {
    const modal = this.dom.spawnModal(CommandListEditor);
    modal.setup(this.map.cmd, "map");
    modal.ondirty = () => {
      this.broadcast({ type: "commands" });
    };
  }
  
  action_resize() {
    console.log(`MapPaint.action_resize`);//TODO
  }
  
  action_healAll() {
    console.log(`MapPaint.action_healAll`);//TODO
  }
  
  action_neighbors() {
    console.log(`MapPaint.action_neighbors`);//TODO
  }
}

MapPaint.singleton = false; // sic false, despite appearances.

/* Tool definitions.
 *******************************************************************/

MapPaint.TOOLS = [
  {
    name: "pencil",
    ctl: "pickup",
  },
  {
    name: "rainbow",
    ctl: "pickup",
  },
  {
    name: "monalisa",
    ctl: "pickup",
  },
  {
    name: "heal",
  },
  {
    name: "pickup",
  },
  {
    name: "marquee",
    // Don't assign ctlShift, ctl, or shift -- the tool uses them itself!
  },
  {
    name: "lasso",
    // Don't assign ctlShift, ctl, or shift -- the tool uses them itself!
  },
  {
    name: "poiedit",
    shift: "poimove",
  },
  {
    name: "poimove",
    // Uses shift for copy.
    ctl: "poiedit",
  },
  {
    name: "door",
    shift: "poimove",
    ctl: "poiedit",
  },
];

/* Impulse action definitions.
 **********************************************************************/
 
MapPaint.ACTIONS = [
  {
    name: "commands",
    label: "Commands...",
  },
  {
    name: "resize",
    label: "Resize...",
  },
  {
    name: "healAll",
    label: "Heal All",
  },
  {
    name: "neighbors",
    label: "Neighbors...",
  },
];
