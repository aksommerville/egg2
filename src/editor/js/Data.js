/* Data.js
 * Holds the live resource store and brokers sync with the server.
 */
 
import { Comm } from "./Comm.js";
 
const DIRTY_DEBOUNCE_TIME = 2000;
 
export class Data {
  static getDependencies() {
    return [Comm, Window];
  }
  constructor(comm, window) {
    this.comm = comm;
    this.window = window;
    
    this.defaultLanguage = "en"; // TODO Replace after we acquire metadata.
    this.textDecoder = new TextDecoder("utf8");
    this.resv = []; // {path,type,lang,rid:number,name,comment,format,serial:Uint8Array}
    this.tocListeners = [];
    this.dirtyListeners = [];
    this.nextListenerId = 1;
    this.dirtyState = "pending"; // "clean" | "dirty" | "pending" | "error"
    this.dirties = []; // {path,cb}
    this.dirtyTimeout = null;
    
    this.nextLoadPromise = new Promise((resolve, reject) => {
      this.nextLoadResolve = resolve;
      this.nextLoadReject = reject;
    });
    
    this.comm.httpBinary("GET", "/api/allcontent/data")
      .then(rsp => this.receiveArchive(new Uint8Array(rsp)))
      .catch(e => {
        console.error(e);
        this.broadcastDirty("error");
        this.nextLoadReject();
        this.nextLoadPromise = null;
        this.nextLoadResolve = null;
        this.nextLoadReject = null;
      });
  }
  
  whenLoaded() {
    return this.nextLoadPromise;
  }
  
  /* Resource path.
   * (path) is a string.
   * (split) is { type, lang, rid, name, comment, format }.
   ************************************************************************/
   
  evalPath(path) {
    if (path === "/data/metadata") return {
      type: "metadata",
      lang: "",
      rid: 1,
      name: "",
      comment: "",
      format: "",
    };
    const components = path.split("/");
    let type = "";
    if (components.length >= 2) {
      type = components[components.length - 2];
    }
    const base = components[components.length - 1] || "";
    const dottedBits = base.split(".");
    let lang="", rid=0, name="", comment="", format="";
    if (dottedBits.length >= 2) {
      format = dottedBits[dottedBits.length - 1];
      comment = dottedBits.slice(1, dottedBits.length - 1).join(".");
    }
    const match = dottedBits[0].match(/^([a-z]{2}-)?(\d+)(-.*)?$/);
    if (match) {
      if (match[1]) lang = match[1].substring(0, 2);
      rid = +match[2];
      if (match[3]) name = match[3].substring(1);
    }
    return { type, lang, rid, name, comment, format };
  }
  
  combinePath(split) {
    if (split.type === "metadata") return "/data/metadata";
    let path = "/data/";
    if (split.type) path += split.type;
    else path += "UNKNOWN_TYPE";
    path += "/";
    if (split.lang) path += split.lang + "-";
    path += split.rid || 0;
    if (split.name) path += "-" + split.name;
    if (split.comment) path += "." + split.comment;
    if (split.format || split.comment) path += "." + (split.format || "data");
    return path;
  }
  
  unusedId(type) {
    const used = new Set();
    for (const res of this.resv) {
      if (res.type !== type) continue;
      used.add(res.rid);
    }
    for (let rid=1; rid<0xffff; rid++) {
      if (!used.has(rid)) return rid;
    }
    return 0;
  }
  
  /* Resources.
   ***********************************************************************************/
  
  /* Resolves with the toc entry on success, or rejects on any error.
   * If we resolve, the new file exists.
   */
  createResource(path, serial) {
    if (!serial) serial = new Uint8Array();
    if (!(serial instanceof Uint8Array)) throw new Error(`New resource serial must be null or Uint8Array.`);
    if (!path?.startsWith("/data/")) return Promise.reject("Invalid path.");
    if (this.resv.find(r => r.path === path)) return Promise.reject("Resource already exists.");
    return this.comm.http("PUT", path, null, null, serial).then(() => {
      const res = { path, serial, ...this.evalPath(path) };
      this.resv.push(res);
      this.broadcastToc();
      return res;
    });
  }
  
  deleteResource(path) {
    let p = this.resv.findIndex(r => r.path === path);
    if (p < 0) return Promise.reject("Resource not found.");
    return this.comm.http("DELETE", path).then(() => {
      // (resv) shouldn't change while we're out but you never know.
      p = this.resv.findIndex(r => r.path === path);
      if (p >= 0) {
        this.resv.splice(p, 1);
        this.broadcastToc();
      }
    });
  }
  
  renameResource(oldPath, newPath) {
    if (oldPath === newPath) return Promise.resolve();
    if (this.resv.find(r => r.path === newPath)) return Promise.reject("New path already in use.");
    if (!newPath?.startsWith("/data/")) return Promise.reject("Invalid path.");
    let op = this.resv.findIndex(r => r.path === oldPath);
    if (op < 0) return Promise.reject("Resource not found.");
    const res = this.resv[op];
    return this.comm.http("PUT", newPath, null, null, res.serial)
      .then(() => this.comm.http("DELETE", oldPath))
      .then(() => {
        op = this.resv.findIndex(r => r.path === oldPath);
        if (op >= 0) this.resv.splice(op, 1);
        const nres = {
          path: newPath,
          serial: res.serial,
          ...this.evalPath(newPath),
        };
        this.resv.push(nres);
        this.broadcastToc();
      });
  }
  
  /* (path) must name an existing resource. Use createResource() if it's new.
   * (cb) must return a Uint8Array of the new encoded content.
   * (cb) will not be called immediately. We debounce for a tasteful interval.
   */
  dirty(path, cb) {
    const res = this.resv.find(r => r.path === path);
    if (!res) return;
    let record = this.dirties.find(d => d.path === path);
    if (record) {
      record.cb = cb;
    } else {
      record = { path, cb };
      this.dirties.push(record);
    }
    // Wait for the timeout to elapse with no further edits.
    // If we wanted to, we could save on a regular rhythm, when edits are ongoing. Just remove this clear, and make a new one only if null.
    // But I think two seconds of idle before saving is not too much to ask.
    if (this.dirtyTimeout) {
      this.window.clearTimeout(this.dirtyTimeout);
    }
    this.dirtyTimeout = this.window.setTimeout(() => {
      this.dirtyTimeout = null;
      this.flushDirties();
    }, DIRTY_DEBOUNCE_TIME);
    this.broadcastDirty("dirty");
  }
  
  /* (req) may be path, rid, name, or qualified name.
   * (type) is optional; if false, (req) must be a qualified name or path.
   * This is meant to be the one-stop-shop for any way you might address a resource.
   */
  findResource(req, type) {
    // If (req) contains a slash, it must be the full path, and (type) is ignored.
    if ((typeof(req) === "string") && (req.indexOf("/") >= 0)) {
      return this.resv.find(r => r.path === req);
    }
    // If (req) contains a colon, the first bit replaces (type).
    if (typeof(req) === "string") {
      const sepp = req.indexOf(":");
      if (sepp >= 0) {
        type = req.substring(0, sepp);
        req = req.substring(sepp +1);
      }
    }
    // After splitting (req), (type) is required.
    if (!type) return null;
    // If (req) is a number, it's the rid.
    let rid = +req;
    if (!isNaN(rid)) {
      if ((rid < 1) || (rid > 0xffff)) return null;
      return this.resv.find(r => ((r.type === type) && (r.rid === rid)));
    }
    // (req) must be the name.
    return this.resv.find(r => ((r.type === type) && (r.name === req)));
  }
  
  /* (type) is required, and if (req) contains a type identifier, we strip it.
   */
  findResourceOverridingType(req, type) {
    if (!type) return null;
    if (typeof(req) === "string") {
      const sepp = req.indexOf(":");
      if (sepp >= 0) req = req.substring(sepp + 1);
    }
    return this.findResource(req, type);
  }
  
  /* Images.
   ***********************************************************************************/
  
  // Resolves with Image, or rejects if anything goes wrong.
  getImageAsync(rid) {
    const res = this.findResource(rid, "image");
    if (!res) return Promise.reject(`image:${rid} not found`);
    if (res.image) return Promise.resolve(res.image); // If image present, assume it's loaded. This won't always be so, does it matter?
    return new Promise((resolve, reject) => {
      const blob = new Blob([res.serial]);
      const url = URL.createObjectURL(blob);
      const image = new Image();
      image.addEventListener("load", () => {
        URL.revokeObjectURL(url);
        resolve(image);
      }, { once: true });
      image.addEventListener("error", error => {
        URL.revokeObjectURL(url);
        delete res.image;
        reject(error);
      }, { once: true });
      image.src = url;
      res.image = image;
    });
  }
  
  /* Subscriptions.
   ***********************************************************************************/
   
  listenToc(cb) {
    const id = this.nextListenerId++;
    this.tocListeners.push({ id, cb });
    return id;
  }
  
  listenDirty(cb) {
    const id = this.nextListenerId++;
    this.dirtyListeners.push({ id, cb });
    return id;
  }
  
  unlistenToc(id) {
    const p = this.tocListeners.findIndex(l => l.id === id);
    if (p >= 0) this.tocListeners.splice(p, 1);
  }
  
  unlistenDirty(id) {
    const p = this.dirtyListeners.findIndex(l => l.id === id);
    if (p >= 0) this.dirtyListeners.splice(p, 1);
  }
  
  broadcastToc() {
    for (const { cb } of this.tocListeners) cb(this.resv);
  }
  
  broadcastDirty(state) {
    if (state === this.dirtyState) return;
    this.dirtyState = state;
    for (const { cb } of this.dirtyListeners) cb(state);
  }
  
  /* Private.
   **********************************************************************************/
   
  receiveArchive(src) {
    this.resv = [];
    let srcp = 0;
    while (srcp < src.length) {
      const pathc = src[srcp++];
      if (srcp > src.length - pathc) break;
      const path = this.textDecoder.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, pathc));
      srcp += pathc;
      if (srcp > src.length - 3) break;
      const bodyc = (src[srcp] << 16) | (src[srcp+1] << 8) | src[srcp+2];
      srcp += 3;
      if (srcp > src.length - bodyc) break;
      const body = new Uint8Array(src.buffer, src.byteOffset + srcp, bodyc);
      srcp += bodyc;
      const res = this.evalPath(path);
      res.path = path;
      res.serial = body;
      this.resv.push(res);
    }
    this.dirties = [];
    this.broadcastDirty("clean");
    this.broadcastToc();
    if (this.nextLoadPromise) {
      this.nextLoadResolve();
      this.nextLoadPromise = null;
      this.nextLoadResolve = null;
      this.nextLoadReject = null;
    }
  }
  
  flushDirties() {
    const records = this.dirties;
    this.dirties = [];
    this.broadcastDirty("pending");
    const promises = [];
    for (const { path, cb } of records) {
      const res = this.resv.find(r => r.path === path);
      if (!res) continue; // Maybe they deleted it after dirtying? Whatever.
      const serial = cb();
      res.serial = serial;
      promises.push(this.comm.http("PUT", res.path, null, null, serial));
    }
    Promise.all(promises).then(() => {
      this.broadcastDirty("clean");
    }).catch(e => {
      console.log(e);
      this.broadcastDirty("error");
      for (const record of records) this.dirties.push(record);
      // Put them back in (dirties) but don't start a new timeout.
      // If we did, we'd get stuck in a loop when the server shuts down.
    });
  }
}

Data.singleton = true;
