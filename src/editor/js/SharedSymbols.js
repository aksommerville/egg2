/* SharedSymbols.js
 * Coordinates access to the project's "shared_symbols.h" file, with helpful enums.
 * (ns) is "NS" or "CMD".
 * Namespace and symbol names come straight off the header.
 * RootUi waits for us to load before initializing, so most editors may assume that it's always ready.
 */
 
import { Comm } from "./Comm.js";
import { Song } from "./song/Song.js";

export class SharedSymbols {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    
    this.symv = []; // {nstype,ns,k,v}
    this.instruments = null; // null or Song
    this.instrumentsPromise = null;
    
    this.loadingPromise = this.comm.httpJson("GET", "/api/symbols").then(rsp => {
      if (!(rsp instanceof Array)) throw new Error(`Expected array from /api/symbols`);
      this.symv = rsp;
    }).catch(error => {
      this.symv = [];
    });
  }
  
  whenLoaded() {
    return this.loadingPromise;
  }
  
  // Empty string if undefined.
  getName(nstype, ns, v) {
    return this.symv.find(s => ((s.nstype === nstype) && (s.ns === ns) && (s.v === v)))?.k || "";
  }
  
  // undefined or integer
  getValue(nstype, ns, name) {
    return this.symv.find(s => ((s.nstype === nstype) && (s.ns === ns) && (s.k === name)))?.v;
  }
  
  /* Resolves to an Instruments instance containing the SDK's default instruments.
   * Logs errors and never rejects.
   */
  getInstruments() {
    if (this.instruments) return Promise.resolve(this.instruments);
    if (this.instrumentsPromise) return this.instrumentsPromise;
    return this.instrumentsPromise = this.comm.httpBinary("GET", "/api/instruments").catch(e => {
      console.log(`GET /api/instruments failed`, e);
      return null;
    }).then(rsp => {
      this.instrumentsPromise = null;
      try {
        this.instruments = new Song(new Uint8Array(rsp));
      } catch (e) {
        console.log(`decode instruments failed`, e);
        this.instruments = new Song();
      }
      return this.instruments;
    });
  }
}

SharedSymbols.singleton = true;
