/* SongService.js
 * Proxies Data with extra Song-specific logic.
 */
 
import { Data } from "../Data.js";
import { Comm } from "../Comm.js";
import { Song } from "./Song.js";

export class SongService {
  static getDependencies() {
    return [Data, Comm];
  }
  constructor(data, comm) {
    this.data = data;
    this.comm = comm;
    
    this.song = null; // Set by SongEditor when it's alive.
    this.visChid = null; // null or chid, visibility filter.
    
    this.listeners = []; // {id,cb}
    this.nextListenerId = 1;
  }
  
  /* We're a singleton, but we're also pretty context-sensitive.
   * SongEditor should call reset() as it loads so we can clear any transient state.
   */
  reset(song) {
    this.song = song;
    this.visChid = null;
  }
  
  /* Returns a Promise resolving to a Song instance.
   * This usually makes a round trip to the server to convert to EAU first.
   */
  getSong(path) {
    const res = this.data.resv.find(r => r.path === path);
    if (!res) return Promise.reject(`Song ${JSON.stringify(path)} not found.`);
    if (this.isEau(res.serial)) return Promise.resolve(new Song(res.serial));
    return this.comm.http("POST", "/api/convert", { dstfmt: "eau" }, null, res.serial, "arrayBuffer").then(rsp => {
      return new Song(new Uint8Array(rsp));
    });
  }
  
  /* Registers the Song as dirty within Data, and arranges to convert back to its original format before writing.
   */
  dirtySong(path, song) {
    // Depend entirely on (path) suffix to judge whether it ought to convert.
    const sfx = path.replace(/^.*\.([^\.]*)$/, "$1").toLowerCase();
    if ((sfx === "mid") || (sfx === "eaut")) {
      this.data.dirty(path, () => {
        return this.comm.http("POST", "/api/convert", { dstfmt: sfx }, null, song.encode(), "arrayBuffer").then(rsp => {
          return new Uint8Array(rsp);
        });
      });
    } else {
      this.data.dirty(path, () => song.encode());
    }
  }
  
  isEau(serial) {
    if (serial.length < 4) return false;
    if (serial[0] !== 0x00) return false;
    if (serial[1] !== 0x45) return false;
    if (serial[2] !== 0x41) return false;
    if (serial[3] !== 0x55) return false;
    return true;
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ cb, id });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  /* (event) may be a stateless string:
   *   "visibility"
   * Or a structured event:
   * Whatever it is, we send to all listeners verbatim.
   */
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
}

SongService.singleton = true;
