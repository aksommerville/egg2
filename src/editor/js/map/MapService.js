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
    
    // If valid, MapEditor should focus this cell at load, and then reset to (-1,-1).
    this.focusx = -1;
    this.focusy = -1;
    
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
          map: new MapRes(res.serial, res.rid),
        });
      } else if (old.map && (old.serial === res.serial)) {
      } else {
        old.serial = res.serial;
        old.map = new MapRes(res.serial, res.rid);
      }
    }
  }
  
  /* From a given MapRes, call (cb(map,dx,dy)) for whichever cardinal and diagonal neighbors exist.
   * Caller may assume that if neighbors are in play, all maps are the same size.
   * There's at least two neighboring regimes that I plan to support: Absolute coordinates and pointers.
   * A third conceivable regime, where neighbors have mixed sizes and can slide anywhere on each other's edges, will not be supported.
   */
  forMapNeighbors(map, cb) {
    //TODO find neighbors
    //TODO Actually, do the real work at load and dirty. Keep a supermap.
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
}

MapService.singleton = true;
