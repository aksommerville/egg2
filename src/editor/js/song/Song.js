/* Song.js
 * Data model for SongEditor.
 * We can work with any of EAU, EAU-Text, and MIDI.
 * The live model will be very close to EAU. So conversion to and from MIDI is expensive and potentially lossy.
 * Because there's three separate formats, and encode/decode for all of them is non-trivial, that logic lives in separate files.
 * Supporting all three is important, but the expectation is that we'll usually see MIDI.
 */
 
import { Encoder } from "../Encoder.js";
import { eauSongEncode, eauSongDecode } from "./songEau.js";
import { eautSongEncode, eautSongDecode } from "./songEaut.js";
import { midiSongEncode, midiSongDecode } from "./songMidi.js";
 
export class Song {
  constructor(src) {
    this._init();
    if (!src) ;
    else if (src instanceof Song) this._copy(src);
    else this._decode(src);
  }
  
  _init() {
    this.format = "mid"; // "mid", "eau", "eaut". It's fine to change this on a live object, to encode as a different format.
    this.tempo = 500; // ms/qnote
    this.channels = []; // SongChannel. Packed.
    this.channelsByChid = []; // Weak SongChannel. Sparse, indexed by chid.
    this.events = []; // SongEvent.
    this.rangeNames = []; // {lo,hi,name}, only relevant for the built-in instruments song.
  }
  
  _copy(src) {
    this.format = src.format;
    this.tempo = src.tempo;
    this.channels = src.channels.map(c => new SongChannel(c));
    this.rebuildChannelsByChid();
    this.events = src.events.map(e => new SongEvent(e));
    this.rangeNames = src.rangeNames.map(r => ({...r}));
  }
  
  _decode(src) {
    if (!src?.length) return;
    if (typeof(src) === "string") return eautSongDecode(this, src);
    if (src instanceof Uint8Array) {
      if (Encoder.checkSignature(src, "\u0000EAU")) return eauSongDecode(this, src);
      if (Encoder.checkSignature(src, "MThd\u0000\u0000\u0000\u0006")) return midiSongDecode(this, src);
      return eautSongDecode(this, new TextDecoder("utf8").decode(src));
    }
    throw new Error("Inappropriate input to Song");
  }
  
  encode() {
    switch (this.format) {
      case "eau": return eauSongEncode(this);
      case "eaut": return new TextEncoder("utf8").encode(eautSongEncode(this));
      case "mid": return midiSongEncode(this);
    }
    return eauSongEncode(this);
  }
  
  rebuildChannelsByChid() {
    this.channelsByChid = [];
    for (const channel of this.channels) this.channelsByChid[channel.chid] = channel;
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
    for (let chid=0; chid<this.channelsByChid.length; chid++) {
      if (this.channelsByChid[chid]) chids.push(chid);
    }
    return chids;
  }
  
  unusedChid() {
    for (let chid=0; chid<16; chid++) {
      if (!this.channelsByChid[chid]) return chid;
    }
    return -1;
  }
  
  /* Special accessors.
   ****************************************************************************/
  
  /* If you've just changed a channel's "name" property, call this to force events to match it.
   * Songs sourced from EAU or EAU-Text won't change anything here, but MIDI will.
   * Returns true if anything changed.
   */
  replaceEventsForChannelName(chid) {
    const channel = this.channels.find(c => c?.chid === chid);
    if (!channel) return false;
    const ep = this.events.findIndex(e => ((e.type === "m") && (e.opcode === 0xff) && (e.chid === chid) && ((e.a === 0x03) || (e.a === 0x04))));
    if (ep >= 0) {
      const event = this.events[ep];
      if (channel.name) {
        event.v = new TextEncoder("utf8").encode(channel.name);
      } else {
        this.events.splice(ep, 1);
      }
      return true;
    } else if (channel.name) { // Add a MIDI Meta 0x03 Track Name event, regardless of our source mode. EAUs will drop it at encode.
      const event = new SongEvent();
      event.time = 0;
      event.type = "m";
      event.opcode = 0xff;
      event.chid = chid;
      event.a = 0x03;
      event.v = new TextEncoder("utf8").encode(channel.name);
      this.events.splice(0, 0, event);
      return true;
    }
    return false;
  }
  
  /* Call if you change any event's (time).
   */
  sortEvents() {
    this.events.sort((a, b) => a.time - b.time);
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
  
  // Returns the new (srcp).
  static measure(src, srcp) {
    if (srcp > src.length - 8) return 0;
    const paylen = (src[srcp + 4] << 8) | src[srcp + 5];
    let postp = srcp + 6 + paylen;
    let postlen = src[postp++] << 8;
    postlen |= src[postp++];
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
    this.mode = 0; // (0,1,2,3,4) = (NOOP,DRUM,FM,HARSH,HARM)
    this.payload = []; // Uint8Array if not empty.
    this.post = []; // Uint8Array if not empty.
    this.name = "";
    this.stashedPayloads = []; // Sparse Uint8Array keyed by mode, so we can preserve config when you toggle mode around.
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
  
  // From one EAU "CHDR" chunk.
  _decode(src) {
    let srcp = 0;
    if (srcp < src.length) this.chid = src[srcp++]; else this.chid = 0;
    if (srcp < src.length) this.trim = src[srcp++]; else this.trim = 0x80;
    if (srcp < src.length) this.pan = src[srcp++]; else this.pan = 0x80;
    if (srcp < src.length) this.mode = src[srcp++]; else this.mode = 2;
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
  
  encode() {
    const encoder = new Encoder();
    encoder.u8(this.chid);
    encoder.u8(this.trim);
    encoder.u8(this.pan);
    encoder.u8(this.mode);
    encoder.u16be(this.payload.length);
    encoder.raw(this.payload);
    encoder.u16be(this.post.length);
    encoder.raw(this.post);
    return encoder.finish();
  }
  
  getDisplayName() {
    if (this.name) return `${this.chid}: ${this.name}`;
    return `Channel ${this.chid}`;
  }
  
  changeMode(nmode) {
    const fromMode = this.mode;
    const fromSerial = this.payload;
    const toSerial = this.stashedPayloads[nmode];
    this.stashedPayloads[fromMode] = fromSerial;
    this.mode = nmode;
    this.payload = eauGuessInitialChannelConfig(nmode, toSerial, fromMode, fromSerial);
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
    this.trackId = 0; // Only meaningful when sourced from MIDI. Zero-based index of MTrk.
    // If (type === "n"):
    //   this.noteid = 0x40; // 0..127
    //   this.velocity = 0x80; // 0..127
    //   this.durms = 0;
    // If (type === "w"):
    //   this.v = 0x2000; // 0..0x3fff
    // If (type === "m"):
    //   this.opcode = 0x00; // High 4 bits only, for channel voice events. 0xff=Meta, 0xf0=Sysex, 0xf7=Sysex. You'll never see 0x90 or 0xe0.
    //   this.a = 0x00; // 0..127 ; "type" for Meta events. Absent for Sysex.
    //   this.b = 0x00; // 0..127 ; Absent for Program Change, Channel Pressure, Meta, and Sysex.
    //   this.v = new Uint8Array(0); // Meta or Sysex.
    // We're free to add other fields willy-nilly, especially in the (type === "") case.
    // Songs sourced from MIDI will contain 'n' and 'w' events, not their 'm' counterparts.
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
