/* ReorderMapsModal.js
 * For games where each map is a level to be played in sequence.
 * Show all maps and allow visual reordering, which will reassign their rid in bulk.
 */
 
import { Dom } from "../Dom.js";
import { MapService, MapLayer } from "./MapService.js";
import { Actions } from "../Actions.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { Data } from "../Data.js";

export class ReorderMapsModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MapService, Actions, Window, SharedSymbols, Data];
  }
  constructor(element, dom, mapService, actions, window, sharedSymbols, data) {
    this.element = element;
    this.dom = dom;
    this.mapService = mapService;
    this.actions = actions;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    this.data = data;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.mapsContainNeighbors = false;
    this.mapsContiguousFromOne = true;
    this.sortedMaps = [...this.mapService.resv];
    this.sortedMaps.sort((a, b) => a.rid - b.rid);
    let expectRid = 1;
    for (const { rid, map } of this.sortedMaps) {
      if (rid !== expectRid++) this.mapsContiguousFromOne = false;
      if (!this.mapsContainNeighbors && map.cmd?.commands) {
        for (const cmd of map.cmd.commands) {
          if ((cmd[0] === "position") || (cmd[0] === "neighbors")) {
            this.mapsContainNeighbors = true;
          }
        }
      }
    }
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* UI.
   *******************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    if (this.mapsContainNeighbors) {
      this.dom.spawn(this.element, "DIV", "WARNING: One or more maps contain a 'position' or 'neighbors' command. Reordering here may break those relationships.");
    }
    if (!this.mapsContiguousFromOne) {
      this.dom.spawn(this.element, "DIV", "WARNING: IDs not currently contiguous from one. We will rewrite them so if you save.");
    }
    
    const scroller = this.dom.spawn(this.element, "DIV", ["scroller"]);
    const list = this.dom.spawn(scroller, "UL", ["list"]);
    for (const res of this.sortedMaps) {
      const row = this.dom.spawn(list, "LI", ["map"], { "data-path": res.path });
      this.populateMapRow(row, res);
    }
    
    const buttonsRow = this.dom.spawn(this.element, "DIV", ["buttonsRow"]);
    this.dom.spawn(buttonsRow, "INPUT", { type: "button", value: "Save", "on-click": () => this.onSave() });
  }
  
  populateMapRow(row, res) {
    row.innerHTML = "";
    const parts = res.path.split("/");
    const base = parts[parts.length - 1] || "";
    this.dom.spawn(row, "INPUT", { type: "button", value: "^", "on-click": () => this.onMoveRow(row, -1) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "v", "on-click": () => this.onMoveRow(row, 1) });
    this.dom.spawn(row, "DIV", ["path"], base);
    const canvas = this.dom.spawn(row, "CANVAS", ["thumbnail"]);
    this.renderThumbnail(canvas, res.map);
  }
  
  renderThumbnail(canvas, map) {
    const imageName = map.cmd.getFirstArg("image");
    this.data.getImageAsync(imageName)
      .then(image => this.renderMapWithImage(canvas, map, image))
      .catch(e => console.log(e));
  }
  
  renderMapWithImage(canvas, map, image) {
    const tilesize = image.naturalWidth >> 4;
    canvas.width = tilesize * map.w;
    canvas.height = tilesize * map.h;
    const ctx = canvas.getContext("2d");
    for (let y=0, yi=map.h, cellp=0; yi-->0; y+=tilesize) {
      for (let x=0, xi=map.w; xi-->0; cellp++, x+=tilesize) {
        const tileid = map.v[cellp] || 0;
        const srcx = (tileid & 15) * tilesize;
        const srcy = (tileid >> 4) * tilesize;
        ctx.drawImage(image, srcx, srcy, tilesize, tilesize, x, y, tilesize, tilesize);
      }
    }
  }
  
  /* Events.
   **********************************************************/
   
  onSave() {
    const toDelete = []; // path
    const toCreate = []; // res
    const rows = Array.from(this.dom.document.querySelectorAll(".list > li.map"));
    let newRid = 0;
    for (const row of rows) {
      newRid++;
      const oldPath = row.getAttribute("data-path");
      const oldBits = this.data.evalPath(oldPath);
      oldBits.rid = newRid;
      const newPath = this.data.combinePath(oldBits);
      if (newPath === oldPath) continue;
      const res = this.sortedMaps.find(r => r.path === oldPath);
      if (!res) continue;
      
      /* It is extremely likely that there will be overlap between the old and new paths; maps typically don't have a name.
       * So we can't just Data.renameResource() them all. That might delete the new ones.
       * We have to do one pass of deletions, followed by a separate pass of creations.
       */
      res.map.rid = newRid;
      toDelete.push(oldPath);
      toCreate.push({ ...res, rid: newRid, path: newPath });
    }
    let promise = Promise.resolve();
    for (const path of toDelete) {
      promise = promise.then(() => this.data.deleteResource(path));
    }
    for (const res of toCreate) {
      promise = promise.then(() => this.data.createResource(res.path, res.serial));
    }
    promise.then(() => {
      this.resolve();
      this.element.remove();
    }).catch(e => this.dom.modalError(e));
  }
  
  onMoveRow(row, d) {
    const rows = Array.from(this.dom.document.querySelectorAll(".list > li.map"));
    const p = rows.indexOf(row);
    console.log({ rows, p });
    if (p < 0) return;
    if (d > 0) d = 2;
    const np = p + d;
    if (np < 0) return;
    const before = rows[np]; // Can be null, at the bottom.
    const list = row.parentNode;
    list.insertBefore(row, before);
  }
}
