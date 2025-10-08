/* SharedSymbols.js
 * Coordinates access to the project's "shared_symbols.h" file, with helpful enums.
 * (ns) is "NS" or "CMD".
 * Namespace and symbol names come straight off the header.
 * RootUi waits for us to load before initializing, so most editors may assume that it's always ready.
 */
 
import { Comm } from "./Comm.js";
import { Song } from "./song/Song.js";
import { Dom } from "./Dom.js"; // for error messages only

export class SharedSymbols {
  static getDependencies() {
    return [Comm, Dom];
  }
  constructor(comm, dom) {
    this.comm = comm;
    this.dom = dom;
    
    this.symv = []; // {nstype,ns,k,v,comment?,argc?}
    this.instruments = null; // null or Song
    this.instrumentsPromise = null;
    this.projname = "";
    
    this.loadingPromise = this.comm.httpJson("GET", "/api/symbols").then(rsp => {
      if (!(rsp instanceof Array)) throw new Error(`Expected array from /api/symbols`);
      this.symv = this.digestSymbols(rsp);
    }).catch(error => {
      this.symv = [];
    }).then(() => {
      return this.comm.httpText("GET", "/api/projname");
    }).then(rsp => {
      if (rsp && (typeof(rsp) === "string")) this.projname = rsp;
    }).catch(() => {});
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
  
  // Nonzero if we have at least one "CMD_" symbol for the given type.
  typeNameIsCommandList(tname) {
    return !!this.symv.find(sym => ((sym.nstype === "CMD") && (sym.ns === tname)));
  }
  
  // Empty string if undefined.
  getComment(nstype, ns, name) {
    return this.symv.find(s => ((s.nstype === nstype) && (s.ns === ns) && (s.k === name)))?.comment || "";
  }
  
  /* Resolves to an Instruments instance containing the SDK's default instruments.
   * Logs errors and never rejects.
   */
  getInstruments() {
    if (this.instruments) return Promise.resolve(this.instruments);
    if (this.instrumentsPromise) return this.instrumentsPromise;
    return this.instrumentsPromise = this.comm.httpBinary("GET", "/api/instruments").catch(e => {
      if (e?.text) e?.text().then(msg => this.dom.modalError(msg || e));
      else this.dom.modalError(e);
      return null;
    }).then(rsp => {
      this.instrumentsPromise = null;
      try {
        this.instruments = new Song(new Uint8Array(rsp));
      } catch (e) {
        this.dom.modalError(e);
        this.instruments = new Song();
      }
      return this.instruments;
    });
  }
  
  digestSymbols(symv) {
    return symv.map(sym => {
      if (sym.nstype === "CMD") {
        sym = {...sym};
        if (sym.comment) {
          sym.argc = sym.comment.split(/[,\s]+/g).length;
        } else {
          sym.argc = 0;
        }
      }
      return sym;
    });
  }
}

SharedSymbols.singleton = true;
