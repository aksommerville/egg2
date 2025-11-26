/* SdkInstrumentsService.js
 * Caches the SDK instruments and coordinates launching an editor for them.
 */
 
import { Dom } from "../Dom.js";
import { Comm } from "../Comm.js";
import { InstrumentsEditor } from "./InstrumentsEditor.js";
import { Song } from "./Song.js";

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
    this.song = null;
  }
  
  getInstruments() {
    if (this.song) return Promise.resolve(this.song);
    return this.getSerial().then(serial => {
      this.song = new Song(this.serial);
      return this.song;
    });
  }
  
  getSerial() {
    if (this.serial) return Promise.resolve(this.serial);
    if (this.serialPromise) return this.serialPromise;
    this.song = null;
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
      const controller = this.dom.spawnModal(InstrumentsEditor);
      controller.setup(new Song(serial), song => this.saveSoon(song));
    }).catch(e => {
      this.dom.modalError(e);
    });
  }
  
  saveSoon(song) {
    // Saving is expensive; debounce it.
    if (this.saveTimeout) this.window.clearTimeout(this.saveTimeout);
    this.saveTimeout = this.window.setTimeout(() => {
      this.saveTimeout = null;
      this.saveNow(song);
    }, 100);
  }
  
  saveNow(song) {
    const serial = song.encode();
    this.song = song;
    if (!this.serialPromise) { // Capture it locally even if saving fails.
      this.serial = serial;
    }
    this.comm.httpBinary("PUT", "/api/instruments", null, null, serial).then(rsp => {
      if (!this.serialPromise) {
        this.serial = serial;
      }
    }).catch(e => {
      this.dom.modalError(e);
    });
  }
}

SdkInstrumentsService.singleton = true;
