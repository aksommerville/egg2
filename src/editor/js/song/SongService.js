/* SongService.js
 * Proxies Data with extra Song-specific logic.
 * ** We are not a singleton! **
 * A new SongService gets created for each SongEditor, and must be passed as an override to all the sub-controllers.
 */
 
import { Data } from "../Data.js";
import { Comm } from "../Comm.js";
import { Dom } from "../Dom.js";
import { Song } from "./Song.js";
import { Audio } from "../Audio.js"; // From the Egg Web Runtime, not part of editor.

export class SongService {
  static getDependencies() {
    return [Data, Comm, Audio, Dom, Window, "nonce"];
  }
  constructor(data, comm, audio, dom, window, nonce) {
    this.data = data;
    this.comm = comm;
    this.audio = audio;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    this.song = null; // Set by SongEditor when it's alive.
    this.rid = 0;
    this.visChid = null; // null or chid, visibility filter.
    this.playing = false;
    this.muteChids = []; // Per-channel switches, set by SongChannelsUi and influencing playSong().
    this.soloChids = [];
    this.noPostChids = [];
    
    this.listeners = []; // {id,cb}
    this.nextListenerId = 1;
    
    this.window.setInterval(() => {
      if (this.audio.ctx) this.audio.update();
    }, 1000);
  }
  
  /* We're a singleton, but we're also pretty context-sensitive.
   * SongEditor should call reset() as it loads so we can clear any transient state.
   * XXX This shouldn't be necessary anymore; we're not a singleton now.
   */
  reset(song, rid) {
    this.audio.playEauSong(null, false);
    this.playing = false;
    this.song = song;
    this.rid = rid;
    this.visChid = null;
    this.muteChids = [];
    this.soloChids = [];
    this.noPostChids = [];
  }
  
  /* SongEditor calls this on the way out.
   * Beware the another SongEditor may have already reset us by the time this happens.
   */
  unload() {
    this.audio.playEauSong(null, false);
    this.playing = false;
  }
  
  playSong(song, rid) {
    try {
      if (typeof(rid) !== "number") rid = this.rid;
      let serial = null, duration = 0;
      if (song instanceof Song) {
        serial = song.encodeWithChidFilters(this.muteChids, this.soloChids, this.noPostChids);
        duration = song.calculateDuration();
      } else if (song instanceof Uint8Array) serial = song;
      else if (!song) ;
      else throw new Error(`Unexpected input to playSong()`);
      this.audio.playEauSong(serial, duration >= 5);
      this.playing = !!serial;
    } catch (e) {
      this.dom.modalError(e);
    }
  }
  
  getNormalizedPlayhead() {
    if (!this.playing) return 0;
    return this.audio.getNormalizedPlayhead();
  }
  
  setNormalizedPlayhead(p) {
    if (!this.playing) return;
    this.audio.setNormalizedPlayhead(p);
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
  
  /* (name) is "solo", "mute", "post".
   * Note that "post" is backward; its default state should be true.
   * This does not take effect until you restart the song.
   */
  setPlaybackControl(name, chid, checked) {
    switch (name) {
      case "solo": {
          const p = this.soloChids.indexOf(chid);
          if (checked) {
            if (p >= 0) return;
            this.soloChids.push(chid);
          } else {
            if (p < 0) return;
            this.soloChids.splice(p, 1);
          }
        } break;
      case "mute": {
          const p = this.muteChids.indexOf(chid);
          if (checked) {
            if (p >= 0) return;
            this.muteChids.push(chid);
          } else {
            if (p < 0) return;
            this.muteChids.splice(p, 1);
          }
        } break;
      case "post": {
          const p = this.noPostChids.indexOf(chid);
          if (!checked) {
            if (p >= 0) return;
            this.noPostChids.push(chid);
          } else {
            if (p < 0) return;
            this.noPostChids.splice(p, 1);
          }
        } break;
    }
  }
  
  getPlaybackControl(name, chid) {
    switch (name) {
      case "solo": return (this.soloChids.indexOf(chid) >= 0);
      case "mute": return (this.muteChids.indexOf(chid) >= 0);
      case "post": return (this.noPostChids.indexOf(chid) < 0);
    }
    return false;
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
   *   "dirty"
   *   "channelSetChanged"
   *   "eventsChanged"
   * Or a structured event:
   * Whatever it is, we send to all listeners verbatim.
   */
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
}

