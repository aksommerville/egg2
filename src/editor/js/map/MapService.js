/* MapService.js
 * Proxies Data with extra map-specific logic.
 */
 
import { Data } from "../Data.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { MapRes } from "./MapRes.js";

export class MapService {
  static getDependencies() {
    return [Data, SharedSymbols];
  }
  constructor(data, sharedSymbols) {
    this.data = data;
    this.sharedSymbols = sharedSymbols;
    
    this.resv = []; // {path,rid,serial,map}. We assume (serial) are immutable and compare them by identity.
    
    this.tocListener = this.data.listenToc(() => this.onTocChanged());
    this.onTocChanged();
  }
  
  getByPath(path) {
    return this.resv.find(r => r.path === path)?.map;
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
}

MapService.singleton = true;
