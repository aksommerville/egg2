/* SdkInstrumentsService.js
 * Caches the SDK instruments and coordinates launching an editor for them.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { SongEditor } from "./SongEditor.js";

export class SdkInstrumentsService {
  static getDependencies() {
    return [Dom, Comm, Window];
  }
  constructor(dom, comm, window) {
    this.dom = dom;
    this.comm = comm;
    this.window = window;
    
    this.serial = null;
    this.serialPromise = null;
    this.saveTimeout = null;
  }
  
  getSerial() {
    if (this.serial) return Promise.resolve(this.serial);
    if (this.serialPromise) return this.serialPromise;
    return this.serialPromise = this.comm.httpBinary("GET", "/api/instruments").then(result => {
      this.serialPromise = null;
      return this.serial = new Uint8Array(result);
    }).catch(e => {
      this.serialPromise = null;
      throw e;
    });
  }
  
  edit() {
    this.getSerial().then(serial => {
      const controller = this.dom.spawnModal(SongEditor);
      controller.setupSerial(serial, song => this.saveSoon(song));
    }).catch(e => {
      this.dom.modalError(e);
    });
  }
  
  saveSoon(song) {
    // SongEditor is actually pretty modest in its callbacks, but saving is expensive so ensure it can't go out too fast.
    if (this.saveTimeout) this.window.clearTimeout(this.saveTimeout);
    this.saveTimeout = this.window.setTimeout(() => {
      this.saveTimeout = null;
      this.saveNow(song);
    }, 100);
  }
  
  saveNow(song) {
    const serial = song.encode();
    this.comm.httpBinary("PUT", "/api/instruments", null, null, serial).catch(e => {
      this.dom.modalError(e);
    });
  }
}

SdkInstrumentsService.singleton = true;
