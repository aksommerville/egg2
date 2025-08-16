/* MapPaint.js
 * Context for MapEditor, shareable across the various UI bits.
 * We are not a singleton! We live only as long as the MapEditor, and get injected as an override.
 * Everything interesting happens here.
 */
 
import { MapService } from "./MapService.js";
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { CommandListEditor } from "../std/CommandListEditor.js";
import { CommandList } from "../std/CommandList.js";
import { Tilesheet } from "../std/Tilesheet.js";
import { Selection } from "./Selection.js";
import { MapRes, Poi } from "./MapRes.js";
import { NewDoorModal } from "./NewDoorModal.js";
import { MapResizeModal } from "./MapResizeModal.js";
import { Actions } from "../Actions.js";
import { NewMapModal } from "./NewMapModal.js";
import { Override } from "../../Override.js";
 
export class MapPaint {
  static getDependencies() {
    return [MapService, Dom, Data, Window, Actions, Override];
  }
  constructor(mapService, dom, data, window, actions, override) {
    this.mapService = mapService;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.actions = actions;
    this.override = override;
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
    this.pvmx = -1; // Previous mouse position, compare to (mousex).
    this.pvmy = -1; // ''
    this.mousefx = -1; // "floating-point" or "full-precision"
    this.mousefy = -1; // ''
    this.poiPosition = -1;
    this.majorFocus = 4; // 0..8 = NW..SE, tracking neighbor regions.
    this.toolInProgress = ""; // Populated while dragging in progress.
    this.anchorx = 0; // monalisa and marquee
    this.anchory = 0;
    this.selection = null; // Selection or null
    this.tempSelection = null; // Selection or null; only exists while a marquee or lasso operation in progress.
    this.dragging = false; // marquee and lasso; are they doing the generic "drag selection"?
    this.selectMode = ""; // Relevant if not dragging and tempSelection present: "replace", "combine", "remove"
    this.poiv = []; // Poi, exported from MapRes.js
    
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
   * { type:"refreshDetailTattle" }
   * { type:"render" }
   * { type:"majorFocus", x, y }
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
    this.composePoiv();
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
  
  composePoiv() {
    this.poiv = this.map.cmd.commands.map(c => Poi.fromCommand(c, this.map.rid)).filter(v => v);
    for (const poi of this.mapService.findDoorsPointingTo(this.map)) {
      this.poiv.push(poi);
    }
    this.refreshPoiPositions();
    const iconPromises = [];
    for (const poi of this.poiv) {
      const promise = this.generatePoiIcon(poi);
      if (promise) iconPromises.push(promise.then(icon => {
        if (icon) poi.icon = icon;
      }));
    }
    if (iconPromises.length) {
      Promise.all(iconPromises)
        .then(() => this.broadcast({ type: "render" }))
        .catch(e => this.dom.modalError(e));
    }
  }
  
  /* Generate custom icon for POI that need it.
   * Returns null if we can determine synchronously that no custom icon is needed -- the usual case.
   * Otherwise a Promise resolving to null or something passable to CanvasRenderingContext2D.drawImage().
   */
  generatePoiIcon(poi) {
    const overfn = this.override.poiIconGenerators[poi.kw];
    if (overfn) return overfn(poi);
    switch (poi.kw) {
      case "sprite": {
          const res = this.data.findResource(poi.cmd[2], "sprite");
          if (!res) break;
          const commandList = new CommandList(res.serial);
          const imageName = commandList.getFirstArg("image");
          const tileParams = commandList.getFirstArgArray("tile");
          if (!imageName || !tileParams || (tileParams.length < 2)) break;
          const tileid = +tileParams[1];
          const xform = +tileParams[2] || 0;
          if (isNaN(tileid) || (tileid < 0) || (tileid > 0xff)) break;
          return this.data.getImageAsync(imageName).then(image => {
            return this.sliceTile(image, tileid, xform, "#0f0");
          }).catch(e => null);
        } break;
    }
    return null;
  }
  
  sliceTile(image, tileid, xform, bgcolor) {
    if (!image || (image.naturalWidth < 16) || (image.naturalHeight < 16)) return null;
    const tilesize = image.naturalWidth >> 4;
    const srcx = (tileid & 15) * tilesize;
    const srcy = (tileid >> 4) * tilesize;
    const canvas = this.dom.document.createElement("CANVAS");
    canvas.width = tilesize;
    canvas.height = tilesize;
    const ctx = canvas.getContext("2d");
    if (bgcolor) {
      ctx.globalAlpha = 0.5;
      ctx.fillStyle = bgcolor;
      ctx.fillRect(0, 0, tilesize, tilesize);
      ctx.globalAlpha = 1;
    }
    const halftile = tilesize >> 1;
    ctx.translate(halftile, halftile);
    switch (xform) {
      case 1: ctx.scale(-1, 1); break; // XREV
      case 2: ctx.scale(1, -1); break; // YREV
      case 3: ctx.scale(-1, -1); break; // XREV|YREV
      case 4: ctx.rotate(Math.PI / 2); ctx.scale(1, -1); break; // SWAP
      case 5: ctx.rotate(Math.PI / -2); break; // SWAP|XREV
      case 6: ctx.rotate(Math.PI / 2); break; // SWAP|YREV
      case 7: ctx.rotate(Math.PI / 2); ctx.scale(-1, 1); break; // SWAP|XREV|YREV
    }
    ctx.drawImage(image, srcx, srcy, tilesize, tilesize, -halftile, -halftile, tilesize, tilesize);
    return canvas;
  }
  
  refreshPoiPositions() {
    const countByCellp = [];
    for (const poi of this.poiv) {
      const cellp = poi.y * this.map.w + poi.x;
      poi.position = countByCellp[cellp] || 0;
      countByCellp[cellp] = poi.position + 1;
    }
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
    this.poiPosition = -1;
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
   *   {TOOLNAME}Motion(x,y): Called at each cell change, when not interacting. (to update tattle)
   *   {TOOLNAME}FineMotion(x,y): Same, but only when cell boundaries are not crossed.
   * You'll only be called with in-bounds coordinates, always integers.
   *******************************************************************************/
   
  checkMajorFocus(x, y) {
    if (!this.map) return;
    let np = this.majorFocus;
    if (x < 0) x = 0;
    else if (x < this.map.w) x = 1;
    else x = 2;
    if (y < 0) y = 0;
    else if (y < this.map.h) y = 1;
    else y = 2;
    np = y * 3 + x;
    if (np === this.majorFocus) return;
    this.majorFocus = np;
    this.broadcast({ type: "majorFocus", x, y });
  }
  
  setMouse(x, y) {
    this.mousefx = x;
    this.mousefy = y;
    x = Math.floor(x);
    y = Math.floor(y);
    this.checkMajorFocus(x, y);
    if (!this.map || (x < 0) || (x >= this.map.w) || (y < 0) || (y >= this.map.h)) x = y = -1;
    if ((x === this.mousex) && (y === this.mousey)) {
      const fnname = this.effectiveTool + "FineMotion";
      this[fnname]?.(this.mousex, this.mousey);
      return false;
    }
    if (this.mousex >= 0) {
      this.pvmx = this.mousex;
      this.pvmy = this.mousey;
    }
    this.mousex = x;
    this.mousey = y;
    this.broadcast({ type: "mouse", x, y });
    if (this.toolInProgress && (x >= 0)) {
      const fnname = this.toolInProgress + "Update";
      this[fnname]?.(this.mousex, this.mousey);
    } else {
      const fnname = this.effectiveTool + "Motion";
      this[fnname]?.(this.mousex, this.mousey);
    }
    return true;
  }
  
  onMouseDown() {
    if (this.toolInProgress) return;
    if (!this.map) return;
    if (this.majorFocus === 4) {
      if ((this.mousex < 0) || (this.mousey < 0) || (this.mousex >= this.map.w) || (this.mousey >= this.map.h)) return;
      const fnname = this.effectiveTool + "Begin";
      this.toolInProgress = this.effectiveTool;
      if (!this[fnname]?.(this.mousex, this.mousey)) {
        this.toolInProgress = "";
      }
    } else {
      const dx = this.majorFocus % 3 - 1;
      const dy = Math.floor(this.majorFocus / 3) - 1;
      const res = this.mapService.getNeighborResource(this.map, dx, dy);
      if (res) {
        this.actions.editResource(res.path);
      } else if (this.mapService.canGenerateNeighbor(this.map, dx, dy)) {
        this.generateNeighbor(dx, dy);
      }
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
    if (this.dragging = this.genericSelectionBegin(x, y)) return true;
    return true;
  }
  
  marqueeUpdate(x, y) {
    if (this.dragging) return this.genericSelectionUpdate(x, y);
    this.tempSelection.setRectangle(x, y);
    this.broadcast({ type: "selectionDirty" });
  }
  
  marqueeEnd() {
    this.genericSelectionEnd();
  }
  
  /* Lasso: Free selection and measurement.
   ****************************************************************/
   
  lassoBegin(x, y) {
    if (this.dragging = this.genericSelectionBegin(x, y)) return true;
    return true;
  }
  
  lassoUpdate(x, y) {
    if (this.dragging) return this.genericSelectionUpdate(x, y);
    if (this.tempSelection.addPoint(x, y)) {
      this.broadcast({ type: "selectionDirty" });
    }
  }
  
  lassoEnd() {
    // A familiar lasso tool would close its path and include the inner content.
    // We don't do that. We only select the cells that you directly visit.
    // (and in hindsight, "lasso" was probably the wrong name for it, but I'm not sure what a righter name would be).
    this.genericSelectionEnd();
  }
  
  /* Generic Selection.
   * Marquee and Lasso are essentially the same thing, so they lean on this shared behavior.
   * It's really only during Update that they behave a little different.
   ************************************************************************/
  
  // Call always, and if we return true, do nothing more -- that means we're dragging an existing selection.
  // If we return false, we'll have prepared (tempSelection).
  genericSelectionBegin(x, y) {
    this.selectMode = "replace";
    if (this.selection) {
      if (this.selection.contains(x, y)) {
        if (this.modifiers.includes("shift")) {
          this.selection.anchor(this.map);
          this.broadcast({ type: "cellDirty", x, y });
        }
        if (!this.modifiers.includes("ctl")) { // With ctl down, no dragging. (so you can lasso out individual cells).
          return true;
        }
      }
      if (this.modifiers.includes("shift")) {
        this.selectMode = "combine";
      } else if (this.modifiers.includes("ctl")) {
        this.selectMode = "remove";
      } else {
        this.selection.anchor(this.map);
        this.selection = null;
        this.broadcast({ type: "cellDirty", x, y });
      }
    }
    this.tempSelection = new Selection(x, y);
    this.broadcast({ type: "selectionDirty" });
    return false;
  }
  
  // Call only if we returned true at genericSelectionBegin, and in that case do nothing more, we got it.
  genericSelectionUpdate(x, y) {
    if (this.pvmx < 0) return; // Extremely unlikely, can only happen at the first event.
    const dx = x - this.pvmx;
    const dy = y - this.pvmy;
    this.selection.move(dx, dy);
    this.broadcast({ type: "selectionDirty" });
  }
  
  // Call whether dragging or not.
  genericSelectionEnd() {
    if (this.dragging) {
      this.dragging = false;
      // Nothing more to do.
    } else {
      // End of regular select operation.
      switch (this.selectMode) {
        case "replace": {
            this.selection = this.tempSelection;
            this.selection.float(this.map);
            this.tempSelection = null;
            this.broadcast({ type: "selectionDirty" });
            // Return without reporting cells dirty. Establishing a new selection doesn't dirty the map (selections are implicitly anchored before saving).
          } return;
        case "combine": this.selection.combine(this.tempSelection, this.map); break;
        case "remove": this.selection.remove(this.tempSelection, this.map); break;
      }
      this.tempSelection = null;
      this.broadcast({ type: "cellDirty", x: 0, y: 0 });
      this.broadcast({ type: "selectionDirty" });
    }
  }
  
  dropSelection() {
    if (!this.selection) return;
    if (this.toolInProgress) return;
    this.selection.anchor(this.map);
    this.selection = null;
    this.broadcast({ type: "cellDirty", x: 0, y: 0 });
    this.broadcast({ type: "selectionDirty" });
  }
  
  /* Poi Edit: Open a modal for the focussed POI.
   ***********************************************************/
   
  poieditBegin(x, y) {
    const poi = this.getFocusPoi();
    let prompt, initial;
    if (!poi) {
      prompt = "New command:";
      initial = `KEYWORD @${x},${y}`;
    } else if (poi.mapid !== this.map.rid) {
      prompt = `Command in remote map ${poi.mapid}:`;
      initial = poi.cmd.join(" ");
    } else {
      prompt = "Command:";
      initial = poi.cmd.join(" ");
    }
    this.dom.modalText(prompt, initial).then(rsp => {
      if (typeof(rsp) !== "string") return;
      if (rsp === initial) return;
      let map = this.map;
      if (poi && (poi.mapid !== this.map.rid)) {
        map = this.mapService.getByRid(poi.mapid);
        if (!map) return;
      }
      if (poi) {
        const cmdp = map.cmd.commands.indexOf(poi.cmd);
        if (cmdp < 0) return;
        map.cmd.commands[cmdp] = rsp.split(/\s+/g).filter(v => v);
        if (poi.mapid !== this.map.rid) this.mapService.dirtyRid(poi.mapid);
      } else {
        map.cmd.commands.push(rsp.split(/\s+/g).filter(v => v));
      }
      this.composePoiv();
      this.broadcast({ type: "refreshDetailTattle" });
      this.broadcast({ type: "commands" });
    });
    return false;
  }
  
  poieditMotion(x, y) {
    this.poiPosition = 0;
    if (this.mousefx % 1 >= 0.5) this.poiPosition |= 1;
    if (this.mousefy % 1 >= 0.5) this.poiPosition |= 2;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  poieditFineMotion(x, y) {
    let position = 0;
    if (this.mousefx % 1 >= 0.5) position |= 1;
    if (this.mousefy % 1 >= 0.5) position |= 2;
    if (position === this.poiPosition) return;
    this.poiPosition = position;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  getFocusPoi() {
    if (this.poiPosition < 0) {
      return this.poiv.find(p => ((p.x === this.mousex) && (p.y === this.mousey)));
    }
    return this.poiv.find(p => ((p.x === this.mousex) && (p.y === this.mousey) && (p.position === this.poiPosition)));
  }
  
  /* Poi Move: Move or duplicate POI.
   **************************************************************/
   
  poimoveBegin(x, y) {
    let poi = this.getFocusPoi();
    if (!poi) return false;
    if (this.modifiers.includes("shift")) {
      const cmd = poi.cmd;
      if (poi.mapid === this.map.rid) { // Shift-drag to copy only works for local POI.
        this.map.cmd.commands.push([...cmd]);
        this.composePoiv();
        this.broadcast({ type: "commands" });
        if (!(poi = this.poiv.find(p => p.cmd === cmd))) return; // composePoiv makes fresh objects, but must contain the same (cmd).
      }
    }
    this.dragPoi = poi;
    return true;
  }
  
  poimoveUpdate(x, y) {
    if (!this.dragPoi) return;
    this.dragPoi.x = x;
    this.dragPoi.y = y;
    this.refreshPoiPositions();
    this.broadcast({ type: "render" });
  }
  
  poimoveEnd() {
    if (!this.dragPoi) return;
    this.dragPoi.setLocation(this.mousex, this.mousey);
    this.broadcast({ type: "commands" });
    if (this.dragPoi.mapid !== this.map.rid) {
      this.mapService.dirtyRid(this.dragPoi.mapid);
    }
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  poimoveMotion(x, y) {
    this.poiPosition = 0;
    if (this.mousefx % 1 >= 0.5) this.poiPosition |= 1;
    if (this.mousefy % 1 >= 0.5) this.poiPosition |= 2;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  poimoveFineMotion(x, y) {
    let position = 0;
    if (this.mousefx % 1 >= 0.5) position |= 1;
    if (this.mousefy % 1 >= 0.5) position |= 2;
    if (position === this.poiPosition) return;
    this.poiPosition = position;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  /* Door: Travel through Door POI.
   **************************************************************/
   
  doorBegin(x, y) {
    let poi = this.getFocusPoi();
    if (poi && (poi.kw === "door")) {
      let nameOrId=null, loc=null;
      if (poi.mapid === this.map.rid) {
        // Traverse EXIT door, ie to poi's destination.
        nameOrId = poi.cmd[2];
        loc = poi.cmd[3];
      } else {
        // Traverse ENTRANCE door, ie to poi's source.
        nameOrId = poi.mapid;
        loc = poi.cmd[1];
      }
      const lmatch = loc?.match(/^@(\d+),(\d+)/);
      if (!lmatch) return false;
      const x = +lmatch[1];
      const y = +lmatch[2];
      this.mapService.navigate(nameOrId, x, y);
      return false;
    }
    if (!poi) {
      const modal = this.dom.spawnModal(NewDoorModal);
      modal.setup(this.map, x, y);
      modal.result.then(rsp => {
        if (!rsp) return;
        const srcres = this.data.findResource(this.map.rid, "map");
        if (!srcres) return;
        let dstres = this.data.findResource(rsp.dstmapid, "map");
        const dstmap = this.mapService.getByPath(dstres?.path);
        if (!dstmap) return this.dom.modalError(`Map '${rsp.dstmapid}' not found`);
        if (rsp.dstx < 0) rsp.dstx = 0; else if (rsp.dstx >= dstmap.w) rsp.dstx = dstmap.w - 1;
        if (rsp.dsty < 0) rsp.dsty = 0; else if (rsp.dsty >= dstmap.h) rsp.dsty = dstmap.h - 1;
        this.map.cmd.commands.push(["door", `@${x},${y}`, `map:${dstres.name || dstres.rid}`, `@${rsp.dstx},${rsp.dsty}`, "0x0000"]);
        this.data.dirty(srcres.path, () => this.map.encode());
        if (rsp.roundtrip) {
          dstmap.cmd.commands.push(["door", `@${rsp.dstx},${rsp.dsty}`, `map:${srcres.name || srcres.rid}`, `@${x},${y}`, "0x0000"]);
          this.data.dirty(dstres.path, () => dstmap.encode());
        }
        this.composePoiv();
        this.broadcast({ type: "commands" });
      });
    }
    return false;
  }
  
  doorMotion(x, y) {
    this.poiPosition = 0;
    if (this.mousefx % 1 >= 0.5) this.poiPosition |= 1;
    if (this.mousefy % 1 >= 0.5) this.poiPosition |= 2;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  doorFineMotion(x, y) {
    let position = 0;
    if (this.mousefx % 1 >= 0.5) position |= 1;
    if (this.mousefy % 1 >= 0.5) position |= 2;
    if (position === this.poiPosition) return;
    this.poiPosition = position;
    this.broadcast({ type: "refreshDetailTattle" });
  }
  
  /* Generate neighbor.
   ***********************************************************************************/
   
  generateNeighbor(dx, dy) {
    if (!this.map) return;
    const modal = this.dom.spawnModal(NewMapModal);
    modal.setup(this.map, dx, dy);
    modal.result.then(rsp => {
      if (!rsp) return null;
      return this.mapService.generateNeighbor(this.map, dx, dy, rsp.name, rsp.rid);
    }).then(res => {
      if (res) this.actions.editResource(res.path);
    }).catch(e => this.dom.modalError(e));
  }
  
  /* Impulse actions.
   *************************************************************************************/
  
  performAction(action) {
    if (!action.name.match(/^[a-z][a-zA-Z0-9_]*$/)) return;
    const fnname = "action_" + action.name;
    const fn = this[fnname];
    if (typeof(fn) === "function") {
      fn.bind(this)();
    } else {
      console.log(`MapPaint.performAction: Unknown action ${JSON.stringify(fnname)}`);
    }
  }
  
  action_commands() {
    const modal = this.dom.spawnModal(CommandListEditor);
    modal.setup(this.map.cmd, "map");
    let dirty = false;
    modal.ondirty = () => {
      dirty = true;
      this.broadcast({ type: "commands" });
    };
    modal.element.addEventListener("close", () => {
      if (dirty) {
        this.composePoiv();
        this.broadcast({ type: "commands" });
      }
    });
  }
  
  action_resize() {
    const modal = this.dom.spawnModal(MapResizeModal);
    modal.setup(this.map);
    modal.result.then(rsp => {
      if (!rsp) return;
      if ((typeof(rsp.w) !== "number") || (rsp.w < 1) || (rsp.w > 0xff)) return;
      if ((typeof(rsp.h) !== "number") || (rsp.h < 1) || (rsp.h > 0xff)) return;
      if ((rsp.w === this.map.w) && (rsp.h === this.map.h)) return;
      this.map.resize(rsp.w, rsp.h, rsp.anchor);
      this.broadcast({ type: "zoom" });
      this.broadcast({ type: "cellDirty", x: 0, y: 0 });
    });
  }
  
  action_healAll() {
    for (let y=0; y<this.map.h; y++) {
      for (let x=0; x<this.map.w; x++) {
        this.healUpdate(x, y, false);
      }
    }
    this.broadcast({ type: "cellDirty", x: 0, y: 0 });
  }
  
  action_resetZoom() {
    if (this.zoom === 1) return;
    this.zoom = 1;
    this.broadcast({ type: "zoom" });
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
    name: "resetZoom",
    label: "Reset Zoom",
  },
];
