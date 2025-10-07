/* MapService.js
 * Proxies Data with extra map-specific logic.
 */
 
import { Data } from "../Data.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { MapRes, Poi } from "./MapRes.js";
import { Actions } from "../Actions.js";

export class MapService {
  static getDependencies() {
    return [Data, SharedSymbols, Actions];
  }
  constructor(data, sharedSymbols, actions) {
    this.data = data;
    this.sharedSymbols = sharedSymbols;
    this.actions = actions;
    
    this.resv = []; // {path,rid,serial,map}. We assume (serial) are immutable and compare them by identity.
    this.layerv = []; // MapLayer (see below)
    
    // If valid, MapEditor should focus this cell at load, and then reset to (-1,-1).
    this.focusx = -1;
    this.focusy = -1;
    
    this.neighborStrategy = "none";
    if (this.sharedSymbols.getValue("CMD", "map", "neighbors")) this.neighborStrategy = "pointer";
    else if (this.sharedSymbols.getValue("CMD", "map", "position")) this.neighborStrategy = "coords";
    
    this.tocListener = this.data.listenToc(() => this.onTocChanged());
    this.onTocChanged();
  }
  
  getByPath(path) {
    return this.resv.find(r => r.path === path)?.map;
  }
  
  getByRid(rid) {
    return this.resv.find(r => r.rid === rid)?.map;
  }
  
  dirtyRid(rid) {
    const res = this.resv.find(r => r.rid === rid);
    if (!res) return;
    this.data.dirty(res.path, () => res.map.encode());
  }
  
  onTocChanged() {
    const nresv = this.data.resv.filter(r => (r.type === "map"));
    /* Anything in (resv) but not (nresv) by path, remove it.
     * In (nresv) but not (resv) by path, add it.
     * In both but (serial) different, replace (serial,map).
     * In both and (serial) unchanged, keep it, do nothing.
     */
    for (let i=this.resv.length; i-->0; ) {
      if (!nresv.find(r => r.path === this.resv[i].path)) {
        this.resv.splice(i, 1);
      }
    }
    for (const res of nresv) {
      const old = this.resv.find(r => r.path === res.path);
      if (!old) {
        this.resv.push({
          ...res,
          map: this.generateNewMap(res.serial, res.rid),
        });
      } else if (old.map && (old.serial === res.serial)) {
      } else {
        old.serial = res.serial;
        old.map = this.generateNewMap(res.serial, res.rid);
      }
    }
    this.requireLayout();
  }
  
  /* Return a new instance of MapRes.
   * This is for new maps created without any context -- ones created by clicking on a neighbor slot do not use this.
   * If (serial) is present and not empty, use it verbatim, end of story.
   * Otherwise, scan the project for sensible defaults.
   * It's always legal to create a MapRes from empty serial, so this is only best*-effort.
   * [*] Expect less.
   */
  generateNewMap(serial, rid) {
    if (serial?.length) return new MapRes(serial, rid);
    const mapw = this.sharedSymbols.getValue("NS", "sys", "mapw");
    const maph = this.sharedSymbols.getValue("NS", "sys", "maph");
    const anyTilesheet = this.data.resv.find(r => r.type === "tilesheet");
    const map = new MapRes(null, rid);
    if (mapw && maph) map.resize(mapw, maph);
    if (anyTilesheet) {
      map.cmd.commands.push(["image", `image:${anyTilesheet.name || anyTilesheet.rid}`]);
    }
    return map;
  }
  
  /* From a given MapRes, call (cb(map,dx,dy)) for whichever cardinal and diagonal neighbors exist, up to 8 of them.
   * Caller may assume that if neighbors are in play, all maps are the same size.
   * There's at least two neighboring regimes that I plan to support: Absolute coordinates and pointers.
   * A third conceivable regime, where neighbors have mixed sizes and can slide anywhere on each other's edges, will not be supported.
   */
  forMapNeighbors(map, cb) {
    for (const layer of this.layout) {
      const p = layer.v.findIndex(r => r?.map === map);
      if (p < 0) continue;
      const y = Math.floor(p / layer.w); // relative to layer
      const x = p % layer.w;
      const ck = (dx, dy) => {
        const rx = x + dx;
        if ((rx < 0) || (rx >= layer.w)) return;
        const ry = y + dy;
        if ((ry < 0) || (ry >= layer.h)) return;
        const rres = layer.v[p + dy * layer.w + dx];
        if (!rres) return;
        cb(rres.map, dx, dy);
      };
      ck(-1, -1);
      ck(0, -1);
      ck(1, -1);
      ck(-1, 0);
      ck(1, 0);
      ck(-1, 1);
      ck(0, 1);
      ck(1, 1);
      return;
    }
  }
  
  getNeighborResource(map, dx, dy) {
    for (const layer of this.layout) {
      const p = layer.v.findIndex(r => r?.map === map);
      if (p < 0) continue;
      const y = Math.floor(p / layer.w) + dy;
      const x = p % layer.w + dx;
      if ((y < 0) || (x < 0) || (y >= layer.h) || (x >= layer.w)) return null;
      return layer.v[y * layer.w + x];
    }
    return null;
  }
  
  /* True if you should propose "new map" for the given direction.
   * It's implied that there is no neighbor in that direction already; you must check that first.
   */
  canGenerateNeighbor(map, dx, dy) {
    switch (this.neighborStrategy) {
      case "pointer": {
          // If it's a cardinal step, yes.
          if (!dx || !dy) return true;
          // Diagonals are only valid if one of the intervening cardinals exists.
          if (this.getNeighborResource(map, dx, 0) || this.getNeighborResource(map, 0, dy)) return true;
          return false;
        }
      case "coords": return true;
    }
    return false;
  }
  
  // Resolves with new resource toc entry on success.
  generateNeighbor(map, dx, dy, name, rid) {
    for (const layer of this.layout) {

      let nx, ny;
      if (this.neighborStrategy === "pointer") {
        const p = layer.v.findIndex(r => r?.map === map);
        if (p < 0) continue;
        const y = Math.floor(p / layer.w);
        const x = p % layer.w;
        nx = x + dx;
        ny = y + dy;
      } else if (this.neighborStrategy === "coords") {
        const position = map.cmd.getFirstArgArray("position");
        if (!position || (position.length < 3)) return Promise.reject("Invalid map position");
        nx = +position[1] + dx;
        ny = +position[2] + dy;
      }
      
      layer.growTo(nx, ny);
      const np = ny * layer.w + nx;
      if (layer.v[np]) return Promise.reject("Position already in use.");
      if (!rid) rid = this.data.unusedId("map");
      const path = "/data/map/" + rid + (name ? ("-" + name) : "");

      const nmap = new MapRes(null, rid);
      nmap.resize(
        this.sharedSymbols.getValue("NS", "sys", "mapw") || map.w,
        this.sharedSymbols.getValue("NS", "sys", "maph") || map.h
      );
      let cmd = map.cmd.getFirstArgArray("image");
      if (cmd) nmap.cmd.commands.push([...cmd]);
      if (this.neighborStrategy === "pointer") {
        const w = nx ? layer.v[np - 1] : null;
        const e = (nx < layer.w - 1) ? layer.v[np + 1] : null;
        const n = ny ? layer.v[np - layer.w] : null;
        const s = (ny < layer.h - 1) ? layer.v[np + layer.w] : null;
        nmap.cmd.commands.push([
          "neighbors",
          w ? `map:${w.name || w.rid}` : "0x0000",
          e ? `map:${e.name || e.rid}` : "0x0000",
          n ? `map:${n.name || n.rid}` : "0x0000",
          s ? `map:${s.name || s.rid}` : "0x0000",
        ]);
        this.replaceNeighborCommand(w?.map, 2, name || rid);
        this.replaceNeighborCommand(e?.map, 1, name || rid);
        this.replaceNeighborCommand(n?.map, 4, name || rid);
        this.replaceNeighborCommand(s?.map, 3, name || rid);
      } else if (this.neighborStrategy === "coords") {
        nmap.cmd.commands.push(["position", nx.toString(), ny.toString(), layer.z.toString()]);
      }
      // Don't install it in (layer.v). Once we add the resource in Data, it cascades back.
      const serial = nmap.encode();
      return this.data.createResource(path, serial);
    }
    return Promise.reject("Map not found.");
  }
  
  replaceNeighborCommand(map, argp, v) {
    if (!map) return;
    let cmd = map.cmd.getFirstArgArray("neighbors");
    if (!cmd) {
      cmd = ["neighbors", "0x0000", "0x0000", "0x0000", "0x0000"];
      map.cmd.commands.push(cmd);
    }
    cmd[argp] = `map:${v}`;
    this.data.dirtyRid("map", map.rid, () => map.encode());
  }
  
  /* Search the entire store for "door" commands pointing to this map.
   * Returns an array of Poi from those commands.
   */
  findDoorsPointingTo(dstmap) {
    const doors = [];
    for (const { map } of this.resv) {
      for (const cmd of map.cmd.commands) {
        if (cmd[0] !== "door") continue;
        // cmd[2] is the destination map id. But of course it can take various shapes.
        // We'll let Data do the work for it. Some legal constructions like "(u16)123" will not work here. But I expect "map:NAME" to be the norm.
        const res = this.data.findResource(cmd[2], "map");
        if (!res) continue;
        if (res.rid !== dstmap.rid) continue;
        const poi = Poi.fromCommand(cmd, map.rid, 3);
        if (poi) doors.push(poi);
      }
    }
    return doors;
  }
  
  navigate(nameOrId, x, y) {
    const dres = this.data.findResource(nameOrId, "map");
    if (!dres) return;
    this.focusx = x;
    this.focusy = y;
    this.actions.editResource(dres.path);
  }
  
  /* Global map of maps.
   *****************************************************************************/
  
  /* Review current state of layout, rebuild if it's out of date.
   * Call after changing map commands.
   */
  requireLayout() {
    if (this.neighborStrategy === "none") {
      this.layout = [];
      return false;
    }
    for (const res of this.resv) res.present = false;
    for (const layer of this.layerv) {
      if (!layer.neighborsConsistent(this.neighborStrategy)) return this.rebuildLayout(); // Some relationship changed.
      for (const lres of layer.v) {
        if (!lres) continue;
        if (this.resv.indexOf(lres) < 0) return this.rebuildLayout(); // Something in the layout that's been deleted.
        lres.present = true;
      }
    }
    if (this.resv.find(r => !r.present)) return this.rebuildLayout(); // Something in resv that's been added.
    // Layout still valid.
    return false;
  }
  
  // Force rebuild.
  rebuildLayout() {
    this.layout = [];
    switch (this.neighborStrategy) {
      case "pointer": this.rebuildLayoutPointer(); break;
      case "coords": this.rebuildLayoutCoords(); break;
      default: return false;
    }
    return true;
  }
  
  rebuildLayoutPointer() {
    const unplaced = [...this.resv];
    while (unplaced.length > 0) {
      /* In this outer loop, pop any map and put it in a new layer.
       * Then recursively build up that layer by walking neighbors, ensuring to remove from (unplaced) as we go.
       * At the limit, each map goes into its own layer.
       */
      const res = unplaced.pop();
      const layer = new MapLayer(1, 1);
      layer.v[0] = res;
      this.layout.push(layer);
      this.populatePointerLayer(layer, res, 0, 0, unplaced);
    }
  }
  
  // Any neighbor of (res) which is present in (unplaced), remove from (unplaced), add to (layer), and reenter for it.
  populatePointerLayer(layer, res, x, y, unplaced) {
    const cmd = res.map.cmd.getFirstArgArray("neighbors");
    if (!cmd) return;
    layer.growTo(x - 1, y - 1);
    layer.growTo(x + 1, y + 1);
    const ck = (cmdp, dx, dy) => {
      let name = cmd[cmdp];
      if (!name) return;
      if (name.startsWith("map:")) name = name.substring(4);
      const rid = +name;
      let srcp = -1;
      if (rid > 0) srcp = unplaced.findIndex(r => (r.rid === rid));
      else if (!isNaN(rid)) return;
      else srcp = unplaced.findIndex(r => (r.name === name));
      if (srcp < 0) return; // Could mean a nonexistent map is referenced, but usually only means we've already placed it.
      const nres = unplaced[srcp];
      unplaced.splice(srcp, 1);
      const dstp = (y + dy - layer.y) * layer.w + x + dx - layer.x;
      if (layer.v[dstp]) {
        console.warn(`Maps ${layer.v[dstp].path} and ${nres.path} ended up in the same position.`);
        return;
      }
      layer.v[dstp] = nres;
      this.populatePointerLayer(layer, nres, x + dx, y + dy, unplaced);
    };
    let neighbor;
    ck(1, -1, 0);
    ck(2, 1, 0);
    ck(3, 0, -1);
    ck(4, 0, 1);
  }
  
  rebuildLayoutCoords() {
    const layerByElevation = []; // sparse, indexed by z
    const readPosition = (res) => {
      const cmd = res.map.cmd.getFirstArgArray("position");
      if (!cmd) return [0, 0, 0];
      const x = +cmd[1] || 0;
      const y = +cmd[2] || 0;
      const z = +cmd[3] || 0;
      return [x, y, z];
    };
    for (const res of this.resv) {
      const [x, y, z] = readPosition(res);
      let layer = layerByElevation[z];
      if (!layer) {
        layer = layerByElevation[z] = new MapLayer(1, 1);
        layer.x = x;
        layer.y = y;
        layer.z = z;
        this.layout.push(layer);
      }
      layer.growTo(x, y);
      const p = (y - layer.y) * layer.w + x - layer.x;
      if (layer.v[p]) {
        console.warn(`World position (${x},${y},${z}) occupied by multiple maps (${layer.v[p].path} and ${res.path})`);
        continue;
      }
      layer.v[p] = res;
    }
  }
}

MapService.singleton = true;

// Must be at least 1. How far to extend when reallocating.
// Mind that it's two-dimensional and grows on both sides, so this can get out of hand fast if too big.
const MAP_LAYER_GROWTH = 2;

/* MapLayer, for MapService.layout.
 * Layers are 2-dimensional. If the world map is 3-dimensional, there are separate layers for each 'z' value.
 */
export class MapLayer {
  constructor(w, h) {
    if ((w < 1) || (h < 1)) throw new Error(`Invalid MapLayer dimensions ${w},${h}`);
    this.x = 0;
    this.y = 0;
    this.z = 0;
    this.w = w;
    this.h = h;
    this.v = []; // null or a member of MapService.resv.
    for (let i=w*h; i-->0; ) this.v.push(null);
  }
  
  /* Ensure there is at least one unit of empty border on all 4 sides.
   * Returns true if we reallocated -- any positions you calculated before are now invalid.
   */
  grow() {
    let result = false;
    if (this.rectContainsMap(0, 0, 1, this.h)) { this.growForce(-MAP_LAYER_GROWTH, 0); result = true; }
    if (this.rectContainsMap(0, 0, this.w, 1)) { this.growForce(0, -MAP_LAYER_GROWTH); result = true; }
    if (this.rectContainsMap(this.w - 1, 0, 1, this.h)) { this.growForce(MAP_LAYER_GROWTH, 0); result = true; }
    if (this.rectContainsMap(0, this.h - 1, this.w, 1)) { this.growForce(0, MAP_LAYER_GROWTH); result = true; }
    return result;
  }
  
  /* Ensure that the layer covers position (x,y).
   */
  growTo(x, y) {
    if (x < this.x) this.growForce(x - this.x - MAP_LAYER_GROWTH, 0);
    if (y < this.y) this.growForce(0, y - this.y - MAP_LAYER_GROWTH);
    if (x >= this.x + this.w) this.growForce(x - this.w - this.x + MAP_LAYER_GROWTH, 0);
    if (y >= this.y + this.h) this.growForce(0, y - this.h - this.y + MAP_LAYER_GROWTH);
  }
  
  // Nonzero if there's at least one map in this rect. Local coordinates.
  rectContainsMap(x, y, w, h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > this.w) w = this.w - x;
    if (y + h > this.h) h = this.h - y;
    let rowp = y * this.w + x;
    for (let yi=h; yi-->0; rowp+=this.w) {
      for (let xi=w, p=rowp; xi-->0; p++) {
        if (this.v[p]) return true;
      }
    }
    return false;
  }
  
  growForce(dx, dy) {
    const nw = this.w + Math.abs(dx);
    const nh = this.h + Math.abs(dy);
    const nv = [];
    for (let i=nw*nh; i-->0; ) nv.push(null);
    this.copy(nv, nw, nh, this.v, this.w, this.h, (dx < 0) ? -dx : 0, (dy < 0) ? -dy : 0);
    if (dx < 0) this.x += dx;
    if (dy < 0) this.y += dy;
    this.w = nw;
    this.h = nh;
    this.v = nv;
  }
  
  // Copy all of (src) over some portion of (dst). Caller ensures that it fits.
  copy(dst, dstw, dsth, src, srcw, srch, dstx, dsty) {
    let dstrowp = dsty * dstw + dstx;
    let srcp = 0;
    for (let yi=srch; yi-->0; dstrowp+=dstw) {
      for (let xi=srcw, dstp=dstrowp; xi-->0; dstp++, srcp++) {
        dst[dstp] = src[srcp];
      }
    }
  }
  
  neighborsConsistent(strat) {
    for (let y=0, p=0; y<this.h; y++) {
      for (let x=0; x<this.w; x++, p++) {
        const res = this.v[p];
        if (!res) continue;
        
        if (strat === "pointer") {
          const cmd = res.map.cmd.getFirstArgArray("neighbors");
          if (!this.validatePointerNeighbor(res, x, y, p, -1,  0, cmd[1])) return false;
          if (!this.validatePointerNeighbor(res, x, y, p,  1,  0, cmd[2])) return false;
          if (!this.validatePointerNeighbor(res, x, y, p,  0, -1, cmd[3])) return false;
          if (!this.validatePointerNeighbor(res, x, y, p,  0,  1, cmd[4])) return false;
        
        } else if (strat === "coords") {
          const cmd = res.map.cmd.getFirstArgArray("position");
          if (!cmd) return false;
          const mapx = +cmd[1];
          if (mapx !== this.x + x) return false;
          const mapy = +cmd[2];
          if (mapy !== this.y + y) return false;
        }
      }
    }
    return true;
  }
  
  validatePointerNeighbor(res, x, y, p, dx, dy, src) {
    let nsrc = +src;
    const nx = x + dx;
    const ny = y + dy;
    const np = p + dy * this.w + dx;
    if (!src || (!nsrc && !isNaN(nsrc))) {
      if ((nx < 0) || (ny < 0) || (nx >= this.w) || (ny >= this.h)) return true;
      if (!this.v[np]) return true;
      return false; // Expected nothing but found something.
    }
    if ((nx < 0) || (ny < 0) || (nx >= this.w) || (ny >= this.h)) return false;
    const ores = this.v[np];
    if (!ores) return false;
    if (src.startsWith("map:")) src = src.substring(4);
    if (src === ores.name) return true;
    nsrc = +src;
    if (nsrc === ores.rid) return true;
    return false;
  }
}
