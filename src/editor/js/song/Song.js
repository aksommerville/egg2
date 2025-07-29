/* Song.js
 * Data model for a song or sound resource.
 * We only deal with EAU. Not MIDI or EAU-Text.
 * That's a controversial choice for sure, since MIDI and EAUT can contain data that EAU doesn't.
 * But it makes the editor much more tractable, and hey, it all has to end up EAU eventually.
 * Anyone using Song must arrange to call the server for conversion, if it might be some non-EAU format.
 */
 
export class Song {

  /* Construction.
   ********************************************************************************/
   
  constructor(src) {
    this._init();
    if (!src) ;
    else if (src instanceof Song) this._copy(src);
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Unexpected input to Song`);
  }
  
  _init() {
    this.tempo = 500; // ms/qnote
    // (loopp) is not recorded as a header field. Instead, if a loop point exists, we insert a fake event for it.
    this.channels = []; // SongChannel, packed, not necessarily sorted.
    this.channelsByChid = []; // WEAK SongChannel, sparse, index is chid.
    this.events = []; // SongEvent, sorted by time. Note that Delay is not an event in this model.
  }
  
  _copy(src) {
    this.tempo = src.tempo;
    this.channels = src.channels.map(c => new SongChannel(c));
    this.channelsByChid = [];
    for (const channel of this.channels) this.channelsByChid[channel.chid] = channel;
    this.events = src.events.map(e => new SongEvent(e));
  }
  
  _decode(src) {
    this.got0EAU = false;
    this.gotEVTS = false;
    this.loopp = 0;
    for (let srcp=0; srcp<src.length; ) {
      if (srcp > src.length - 8) throw new Error(`Unexpected EOF in EAU file around ${srcp}/${src.length}`);
      const id = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
      const len = (src[srcp+4] << 24) | (src[srcp+5] << 16) | (src[srcp+6] << 8) | src[srcp+7];
      srcp += 8;
      if ((len < 0) || (srcp > src.length - len)) throw new Error(`Unexpected EOF in EAU file around ${srcp}/${src.length}`);
      const chunk = new Uint8Array(src.buffer, src.byteOffset + srcp, len);
      srcp += len;
      switch (id) {
        case 0x00454155: this._decode0EAU(chunk); break;
        case 0x43484452: this._decodeCHDR(chunk); break;
        case 0x45565453: this._decodeEVTS(chunk); break;
        default: console.warn(`Ignoring unexpected ${len}-byte chunk 0x${id.toString(16).padStart('0',8)} in EAU file. It will not be preserved at save.`);
      }
    }
    delete this.loopp;
    delete this.got0EAU;
    delete this.gotEVTS;
    
    // Create a default channel for any events that lack a channel.
    for (const event of this.events) {
      if (event.chid < 0) continue;
      if (this.channelsByChid[event.chid]) continue;
      const channel = new SongChannel(event.chid);
      this.channels.push(channel);
      this.channelsByChid[event.chid] = channel;
    }
  }
  
  _decode0EAU(src) {
    if (this.got0EAU) throw new Error(`EAU file contains multiple file-header chunks.`);
    this.got0EAU = true;
    if (src.length >= 2) {
      this.tempo = (src[0] << 8) | src[1];
      if (src.length >= 4) {
        this.loopp = (src[2] << 8) | src[3];
      }
    }
  }
  
  _decodeCHDR(src) {
    if (src.length < 1) return; // Empty CHDR is technically legal.
    const channel = new SongChannel(src);
    this.channels.push(channel);
    if (this.channelsByChid[channel.chid]) {
      console.warn(`Multiple headers for channel ${channel.chid}. Using the last as principal but keeping all.`);
    }
    this.channelsByChid[channel.chid] = channel;
  }
  
  _decodeEVTS(src) {
    if (!this.got0EAU) throw new Error(`"\\0EAU" chunk must appear before "EVTS"`);
    if (this.gotEVTS) throw new Error(`Multiple EVTS chunks in EAU file.`);
    this.gotEVTS = true;
    let now = 0;
    for (let srcp=0; srcp<src.length; ) {
      if (this.loopp && (srcp >= this.loopp)) {
        this.loopp = 0;
        this.events.push(new SongEvent(now, "loop"));
      }
      const lead = src[srcp++];
      switch (lead & 0xc0) {
        case 0x00: now += lead; break;
        case 0x40: now += ((lead & 0x3f) + 1) << 6; break;
        case 0x80: {
            if (srcp > src.length - 3) throw new Error(`Unexpected EOF in EAU note event.`);
            const a = src[srcp++];
            const b = src[srcp++];
            const c = src[srcp++];
            const event = new SongEvent(now, "note");
            event.chid = (lead >> 2) & 15;
            event.noteid = ((lead & 3) << 5) | (a >> 3);
            event.velocity = ((a & 7) << 4) | (b >> 4);
            event.durms = (((b & 15) << 8) | c) << 2;
            this.events.push(event);
          } break;
        case 0xc0: {
            if (srcp > src.length - 1) throw new Error(`Unexpected EOF in EAU wheel event.`);
            const a = src[srcp++];
            const event = new SongEvent(now, "wheel");
            event.chid = (lead >> 2) & 15;
            event.wheel = ((lead & 3) << 8) | a;
            this.events.push(event);
          } break;
      }
    }
    // Since we're not recording delay as events, append a dummy to capture the end time.
    this.events.push(new SongEvent(now, "noop"));
  }
}

/* SongChannel.
 ***********************************************************************************/
 
export class SongChannel {
  constructor(src) {
    this._init();
    if (typeof(src) === "number") this.chid = src;
    else if (!src) ;
    else if (src instanceof SongChannel) this._copy(src);
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Unexpected input to SongChannel`);
  }
  
  _init() {
    this.chid = 0;
    this.trim = 0x40;
    this.pan = 0x80;
    this.mode = 2;
    this.modecfg = []; // Read-only Uint8Array if not empty.
    this.post = []; // ''
  }
  
  _copy(src) {
    this.chid = src.chid;
    this.trim = src.trim;
    this.pan = src.pan;
    this.mode = src.mode;
    this.modecfg = new Uint8Array(src.modecfg);
    this.post = new Uint8Array(src.post);
  }
  
  _decode(src) {
    this.chid = (src.length >= 1) ? src[0] : 0;
    this.trim = (src.length >= 2) ? src[1] : 0x40;
    this.pan = (src.length >= 3) ? src[2] : 0x80;
    this.mode = (src.length >= 4) ? src[3] : 2;
    let srcp = 4;
    if (srcp <= src.length - 2) {
      const modecfglen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - modecfglen) throw new Error(`Unexpected EOF in CHDR ${this.chid}`);
      this.modecfg = new Uint8Array(src.buffer, src.byteOffset + srcp, modecfglen);
      srcp += modecfglen;
    }
    if (srcp <= src.length - 2) {
      const postlen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - postlen) throw new Error(`Unexpected EOF in CHDR ${this.chid}`);
      this.post = new Uint8Array(src.buffer, src.byteOffset + srcp, postlen);
      srcp += postlen;
    }
    if (srcp < src.length) {
      console.warn(`Dropping ${src.length - srcp} extra bytes at end of CHDR for channel ${this.chid}`);
    }
  }
}

/* SongEvent.
 ***************************************************************************************/
 
export class SongEvent {
  constructor(srcOrTime, type) {
    this._init(srcOrTime, type);
    if (typeof(srcOrTime) === "number") ;
    else if (!src) ;
    else if (src instanceof SongEvent) this._copy(src);
    else throw new Error(`Unexpected input to SongEvent`);
  }
  
  _init(time, type) {
    if (!SongEvent.nextId) SongEvent.nextId = 1;
    this.id = SongEvent.nextId++; // Unique across the entire session.
    if (typeof(time) === "number") this.time = time;
    else this.time = 0; // ms
    this.type = type || "noop"; // "noop","note","wheel","loop"
    this.chid = -1;
    /* If (type==="note"):
     *   this.noteid = 0..127
     *   this.velocity = 0..127
     *   this.durms = 0..16380
     * If (type==="wheel"):
     *   this.wheel = 0..512..1023
     */
  }
  
  _copy(src) {
    this.time = src.time;
    this.chid = src.chid;
    switch (this.type = src.type) {
      case "note": {
          this.noteid = src.noteid;
          this.velocity = src.velocity;
          this.durms = src.durms;
        } break;
      case "wheel": {
          this.wheel = src.wheel;
        } break;
    }
  }
}
