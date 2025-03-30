/* SharedSymbols.js
 * Coordinates access to the project's "shared_symbols.h" file, with helpful enums.
 * (ns) is "NS" or "CMD".
 * Namespace and symbol names come straight off the header.
 * RootUi waits for us to load before initializing, so most editors may assume that it's always ready.
 */
 
import { Comm } from "./Comm.js";

export class SharedSymbols {
  static getDependencies() {
    return [Comm];
  }
  constructor(comm) {
    this.comm = comm;
    
    this.symv = []; // {nstype,ns,k,v}
    
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
}

SharedSymbols.singleton = true;
