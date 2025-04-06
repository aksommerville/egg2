/* Song.js
 * Data model for SongEditor.
 * We can work with any of EAU, EAU-Text, and MIDI.
 * The live model will be very close to EAU. So conversion to and from MIDI is expensive and potentially lossy.
 * Because there's three separate formats, and encode/decode for all of them is non-trivial, that logic lives in separate files.
 */
 
import { eauSongEncode, eauSongDecode } from "./eauSong.js";
import { eautSongEncode, eautSongDecode } from "./eautSong.js";
import { midiSongEncode, midiSongDecode } from "./midiSong.js";
 
import { Encoder } from "../Encoder.js";
 
export class Song {
  constructor(src) {
    this._init();
    if (!src) ;
    else if (src instanceof Song) this._copy(src);
    else if (src instanceof Uint8Array) this._decode(src);
    else if (typeof(src) === "string") eautSongDecode(this, src);
    else throw new Error(`Inappropriate input to Song`);
  }
  
  _init() {
    this.format = "mid"; // "mid", "eau", "eaut". It's fine to change this on a live object, to encode as a different format.
    this.tempo = 500; // ms/qnote
    this.channels = []; // SongChannel. Indexed by chid, may be sparse.
    this.events = []; // SongEvent.
    this.rangeNames = []; // {lo,hi,name}, only relevant for the built-in instruments song.
  }
  
  _copy(src) {
    this.format = src.format;
    this.tempo = src.tempo;
    this.channels = src.channels.map(c => new SongChannel(c));
    this.events = src.events.map(e => new SongEvent(e));
    this.rangeNames = src.rangeNames.map(r => ({...r}));
  }
  
  _decode(src) {
    if (!src.length) return;
    if (Encoder.checkSignature(src, "\u0000EAU")) return eauSongDecode(this, src);
    if (Encoder.checkSignature(src, "MThd\u0000\u0000\u0000\u0006")) return midiSongDecode(this, src);
    return eautSongDecode(this, new TextDecoder("utf8").decode(src));
  }
  
  encode() {
    switch (this.format) {
      case "eau": return eauSongEncode(this);
      case "eaut": return new TextEncoder("utf8").encode(eautSongEncode(this));
      case "mid": return midiSongEncode(this);
    }
    return eauSongEncode(this);
  }
  
  /* Accessors.
   ****************************************************************************/
   
  getTrackIds() {
    const trackIds = new Set();
    for (const event of this.events) {
      trackIds.add(event.trackId);
    }
    trackIds.delete(undefined);
    return Array.from(trackIds);
  }
  
  getChids() {
    const chids = [];
    for (let chid=0; chid<this.channels.length; chid++) {
      if (this.channels[chid]) chids.push(chid);
    }
    return chids;
  }
}

/* Channel.
 *************************************************************************/
 
export class SongChannel {
  constructor(src) {
    this._init(src);
    if (!src) ;
    else if (typeof(src) === "number") ;
    else if (src instanceof SongChannel) this._copy(src);
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Inappropriate input to SongChannel`);
  }
  
  /* Return an array of SongChannel indexed by chid, possibly sparse.
   * (src) is a Uint8Array starting with a Channel ID, and not including the 0xff Channels Terminator.
   * ie the format of our Meta 0x77 chunk.
   */
  static decodeHeaders(src) {
    const channels = [];
    for (let srcp=0; srcp<src.length; ) {
      const len = SongChannel.measure(src, srcp);
      if (len < 1) break;
      channels.push(new SongChannel(src.slice(srcp, srcp + len)));
      srcp += len;
    }
    return channels;
  }
  
  static measure(src, srcp) {
    if (srcp > src.length - 8) return 0;
    const paylen = (src[srcp + 4] << 8) | src[srcp + 5];
    let postp = srcp + 6 + paylen;
    let postlen = src[postp++] << 8;
    postlen |= src[srcp++];
    if (postp > src.length - postlen) return 0;
    return postp + postlen;
  }
  
  _init(chid) {
    this.chid = (typeof(chid) === "number") ? chid : 0;
    this.bankhi = 0;
    this.banklo = 0;
    this.pid = 0;
    if (chid === 9) { // Channel 9 is drums by default, ie program 128.
      this.banklo = 1;
    }
    this.trim = 0x80;
    this.pan = 0x80;
    this.mode = 0; // (0,1,2,3) = (NOOP,DRUM,FM,SUB)
    this.payload = []; // Uint8Array if not empty.
    this.post = []; // Uint8Array if not empty.
    this.name = "";
  }
  
  _copy(src) {
    this.chid = src.chid;
    this.bankhi = src.bankhi;
    this.banklo = src.banklo;
    this.pid = src.pid;
    this.trim = src.trim;
    this.pan = src.pan;
    this.mode = src.mode;
    this.payload = new Uint8Array(src.payload);
    this.post = new Uint8Array(src.post);
  }
  
  // From EAU format, starting at Channel ID. Caller must measure it on their own.
  _decode(src) {
    let srcp = 0;
    this.chid = src[srcp++] || 0;
    this.trim = src[srcp++] || 0;
    this.pan = src[srcp++] || 0;
    this.mode = src[srcp++] || 0;
    const paylen = (src[srcp] << 8) | src[srcp+1];
    srcp += 2;
    if (srcp > src.length - paylen) throw new Error(`Unexpected EOF in Channel Header`);
    this.payload = src.slice(srcp, srcp + paylen);
    srcp += paylen;
    const postlen = (src[srcp] << 8) | src[srcp+1];
    srcp += 2;
    if (srcp > src.length - postlen) throw new Error(`Unexpected EOF in Channel Header`);
    this.post = src.slice(srcp, srcp + postlen);
  }
  
  getDisplayName() {
    return `Channel ${this.chid}`;
  }
}

/* Event.
 *******************************************************************************/
 
export class SongEvent {
  constructor(src) {
    if (!SongEvent.nextId) SongEvent.nextId = 1;
    this.id = SongEvent.nextId++;
    if (!src) this._init();
    else if (src instanceof SongEvent) this._copy(src);
    else throw new Error(`Inappropriate input to SongEvent`);
  }
  
  _init() {
    this.time = 0; // Absolute ms.
    this.type = ""; // "n"=note, "w"=wheel, "m"=midi, ""=other
    this.chid = -1; // 0..15, or <0 for channelless events.
    // If (type === "n"):
    //   this.noteid = 0x40; // 0..127
    //   this.velocity = 7; // 0..15
    //   this.durms = 0;
    // If (type === "w"):
    //   this.v = 0x80; // 0..255
    // If (type === "m"):
    //   this.trackId = 0; // Zero-based index of MTrk chunk.
    //   this.opcode = 0x00; // High 4 bits only, for channel voice events. 0xff=Meta, 0xf0=Sysex, 0xf7=Sysex. You'll never see 0x90 or 0xe0.
    //   this.a = 0x00; // 0..127 ; "type" for Meta events. Absent for Sysex.
    //   this.b = 0x00; // 0..127 ; Absent for Program Change, Channel Pressure, Meta, and Sysex.
    //   this.v = new Uint8Array(0); // Meta or Sysex.
    // We're free to add other fields willy-nilly, especially in the (type === "") case.
  }
  
  _copy(src) {
    for (const k of Object.keys(src)) {
      if (k === "id") continue;
      if (k instanceof Uint8Array) {
        this[k] = new Uint8Array(src[k]);
        continue;
      }
      this[k] = src[k];
    }
  }
}
