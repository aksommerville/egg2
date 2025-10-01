/* Song.js
 * Data model for a song or sound resource.
 * We only deal with EAU. Not MIDI or EAU-Text.
 * That's a controversial choice for sure, since MIDI and EAUT can contain data that EAU doesn't.
 * But it makes the editor much more tractable, and hey, it all has to end up EAU eventually.
 * Anyone using Song must arrange to call the server for conversion, if it might be some non-EAU format.
 */
 
import { Encoder } from "../Encoder.js";
import { EauDecoder } from "./EauDecoder.js";
 
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
    this.text = []; // [chid,noteid,name], sorted.
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
        case 0x54455854: this._decodeTEXT(chunk); break;
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
  
  _decodeTEXT(src) {
    for (let srcp=0; srcp<src.length; ) {
      const chid = src[srcp++];
      const noteid = src[srcp++] || 0;
      const len = src[srcp++] || 0;
      if (srcp>src.length - len) break;
      let p = this.textSearch(chid, noteid);
      if (p < 0) {
        p = -p - 1;
        const v = new TextDecoder("utf8").decode(new Uint8Array(src.buffer, src.byteOffset + srcp, len));
        this.text.splice(p, 0, [chid, noteid, v]);
      }
      srcp += len;
    }
  }
  
  textSearch(chid, noteid) {
    let lo=0, hi=this.text.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.text[ck];
           if (chid < q[0]) hi = ck;
      else if (chid > q[0]) lo = ck + 1;
      else if (noteid < q[1]) hi = ck;
      else if (noteid > q[1]) lo = ck + 1;
      else return ck;
    }
    return -lo - 1;
  }
  
  // Never empty, and always contains channel and note id.
  getNameForce(chid, noteid) {
    const p = this.textSearch(chid, noteid);
    if (p >= 0) {
      if (noteid) return `${chid}:${noteid}: ${this.text[p][2]}`;
      return `${chid}: ${this.text[p][2]}`;
    }
    if (noteid) return `Note ${noteid} on channel ${chid}`;
    return `Channel ${chid}`;
  }
  
  // Empty string if not defined.
  getName(chid, noteid) {
    const p = this.textSearch(chid, noteid);
    if (p < 0) return "";
    return this.text[p][2];
  }
  
  setName(chid, noteid, name) {
    let p = this.textSearch(chid, noteid);
    if (name) {
      if (name.length > 0xff) name = name.substring(0, 256);
      if (p < 0) this.text.splice(-p - 1, 0, [chid, noteid, name]);
      else this.text[p][2] = name;
    } else {
      if (p >= 0) this.text.splice(p, 1);
    }
  }
  
  removeNoteNames(chid) {
    let p = this.textSearch(chid, 0);
    if (p < 0) p = -p -1;
    else p++;
    let c = 0;
    while ((p + c < this.text.length) && (this.text[p + c][0] === chid)) c++;
    this.text.splice(p, c);
    return c;
  }
  
  noteInUse(chid, noteid) {
    const channel = this.channelsByChid[chid];
    if (!channel) return false;
    return !!this.events.find(e => ((e.type === "note") && (e.chid === chid) && (e.noteid === noteid)));
  }
  
  encode() {
    const encoder = new Encoder();
    
    encoder.raw("\0EAU");
    let looppp = 0;
    encoder.pfxlen(4, () => {
      encoder.u16be(this.tempo);
      looppp = encoder.c;
      encoder.u16be(0);
    });
    
    if (this.text.length) {
      encoder.raw("TEXT");
      encoder.pfxlen(4, () => {
        for (const [chid, noteid, name] of this.text) {
          encoder.u8(chid);
          encoder.u8(noteid);
          encoder.u8(name.length);
          encoder.raw(name);
        }
      });
    }
    
    const skippedChids = new Set();
    for (const channel of this.channels) {
      if (channel.skipEncode) {
        skippedChids.add(channel.chid);
        continue;
      }
      encoder.raw("CHDR");
      encoder.pfxlen(4, () => {
        encoder.u8(channel.chid);
        encoder.u8(channel.trim);
        encoder.u8(channel.pan);
        encoder.u8(channel.mode);
        if (channel.modecfg.length > 0xffff) throw new Error(`Channel ${channel.chid} modecfg length ${channel.modecfg.length} > 65535`);
        encoder.u16be(channel.modecfg.length);
        encoder.raw(channel.modecfg);
        if (!channel.skipPost) {
          if (channel.post.length > 0xffff) throw new Error(`Channel ${channel.chid} post length ${channel.post.length} > 65535`);
          encoder.u16be(channel.post.length);
          encoder.raw(channel.post);
        }
      });
    }
    
    encoder.raw("EVTS");
    encoder.pfxlen(4, () => {
      const chunkstart = encoder.c;
      let now = 0;
      for (const event of this.events) {
      
        if (skippedChids.has(event.chid)) continue;
      
        // Sync delay at each event. There's never a case where we need to aggregate them.
        let delay = ~~(event.time - now);
        while (delay >= 4096) {
          encoder.u8(0x7f);
          delay -= 4096;
        }
        if (delay >= 0x40) {
          encoder.u8(0x40 | ((delay >> 6) - 1));
          delay &= 0x3f;
        }
        if (delay > 0) {
          encoder.u8(delay);
        }
        now = event.time;
        
        switch (event.type) {
          case "loop": {
              const loopp = encoder.c - chunkstart;
              if ((loopp < 0) || (loopp > 0xffff)) throw new Error(`Invalid loop position ${loopp}`);
              encoder.v[looppp] = loopp >> 8;
              encoder.v[looppp + 1] = loopp;
            } break;
          case "note": {
              // 10ccccnn nnnnnvvv vvvvdddd dddddddd : Note (n) on channel (c), velocity (v), duration (d*4) ms.
              let dur = event.durms >> 2;
              if (dur < 0) dur = 0;
              else if (dur > 0xfff) dur = 0xfff;
              encoder.u8(0x80 | (event.chid << 2) | (event.noteid >> 5));
              encoder.u8((event.noteid << 3) | (event.velocity >> 4));
              encoder.u8((event.velocity << 4) | (dur >> 8));
              encoder.u8(dur);
            } break;
          case "wheel": {
              // 11ccccww wwwwwwww : Wheel on channel (c), (w)=0..512..1023 = -1..0..1
              let v = event.wheel;
              if (v < 0) v = 0;
              else if (v > 1023) v = 1023;
              encoder.u8(0xc0 | (event.chid << 2) | (v >> 8));
              encoder.u8(v);
            } break;
        }
      }
    });
    
    return encoder.finish();
  }
  
  encodeWithChidFilters(mute, solo, noPost) {
    for (const channel of this.channels) {
      if (mute.includes(channel.chid)) channel.skipEncode = true;
      else if (solo.includes(channel.chid)) channel.skipEncode = false;
      else if (solo.length > 0) channel.skipEncode = true;
      else channel.skipEncode = false;
      channel.skipPost = noPost.includes(channel.chid);
    }
    const serial = this.encode();
    for (const channel of this.channels) {
      delete channel.skipEncode;
      delete channel.skipPost;
    }
    return serial;
  }
  
  calculateDuration() {
    if (this.events.length < 1) return 0;
    return this.events[this.events.length - 1].time / 1000;
  }
  
  unusedChid() {
    for (let chid=0; chid<16; chid++) {
      if (!this.channelsByChid[chid]) return chid;
    }
    return -1;
  }
  
  countEventsForChid(chid) {
    let c = 0;
    for (const event of this.events) {
      if (event.chid === chid) c++;
    }
    return c;
  }
  
  insertEvent(event) {
    if (!event) throw new Error("Null event");
    let lo=0, hi=this.events.length;
    while (lo < hi) {
      const ck = (lo + hi) >> 1;
      const q = this.events[ck];
           if (event.time < q.time) hi = ck;
      else if (event.time > q.time) lo = ck + 1;
      else {
        lo = ck;
        break;
      }
    }
    this.events.splice(lo, 0, event);
  }
  
  sortEvents() {
    this.events.sort((a, b) => a.time - b.time);
  }
  
  /* Examine events and channel configs to determine the exact time that sound ends, then tweak our final event to match.
   * This is way more expensive than you think, even in noop cases.
   * We do not account for tails due to Delay stages, because mathematically those tails are infinite.
   * This is appropriate for sound effects, not so much for songs.
   * Returns true if anything changed.
   */
  forceMinimumEndTime() {
    if (this.events.length < 1) return false;
  
    // Determine the tail length for each channel, split by low and high velocity, and before and after sustain.
    const tailByChid = []; // Sparse array of [prelo,prehi,postlo,posthi] ms, per channel. (postlo) zero means no sustain; we clamp to 1 if sustainable.
    for (let chid=0; chid<this.channelsByChid.length; chid++) {
      const channel = this.channelsByChid[chid];
      if (!channel) continue;
      tailByChid[chid] = channel.calculateTailTimes();
    }
    
    // Calculate end time for each note and record the highest.
    let endTime = 0;
    for (const event of this.events) {
      if (event.type === "note") {
        const tail = tailByChid[event.chid];
        if (!tail) continue;
        const channel = this.channelsByChid[event.chid];
        if (!channel) continue;
        const hiv = Math.min(0, Math.max(1, event.velocity / 127));
        const lov = 1 - hiv;
        let noteEndTime = event.time + Math.ceil(tail[0] * lov + tail[1] * hiv);
        if (tail[2]) { // Sustainable.
          noteEndTime += event.durms + Math.ceil(tail[2] * lov + tail[3] * hiv);
        }
        if (noteEndTime > endTime) endTime = noteEndTime;
      }
    }
    
    // If we don't already have a noop event at the end, push one and we're done.
    const final = this.events[this.events.length - 1];
    if (final.type !== "noop") {
      if (endTime < final.time) return false; // oops?
      this.events.push(new SongEvent(endTime, "noop"));
      return true;
    }
    
    // If there's at least two events, confirm that the penultimate is before our new end time.
    // It might not be! eg a bunch of redundant Wheel events at the end. In that case, I'm not sure what to do, so do nothing.
    if (this.events.length >= 2) {
      const penny = this.events[this.events.length - 2];
      if (penny.time > endTime) return false;
    }
    
    // Change the final event if it's different.
    if (final.time === endTime) return false;
    final.time = endTime;
    return true;
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
    this.modecfg = new Uint8Array();
    this.post = new Uint8Array();
    this.stash = []; // Sparse Uint8Array indexed by (mode). Prior configs we can return to when user toggles mode.
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
  
  /* Rewrite this channel in place from some other channel.
   * Does not affect (chid,trim,pan).
   */
  overwrite(src) {
    this.mode = src.mode;
    this.modecfg = new Uint8Array(src.modecfg);
    this.post = new Uint8Array(src.post);
  }
  
  /* [prelo,prehi,postlo,posthi]
   * Calculate parameters for notes' duration based on mode and modecfg.
   * (postlo) zero is special, it means notes are unsustainable and (pre) contains the entire duration.
   */
  calculateTailTimes() {
    const decoder = new EauDecoder(this.modecfg);
    let env = null;
    switch (this.mode) {
      /* Doing for DRUM would mean decoding every drum and returning the longest.
       * That's a ton of work and I just don't think it's worth it -- Drums are a song thing, and auto-end-time is a sound thing.
       */
      case 2: { // FM
          decoder.u16(0); // rate
          decoder.u8_8(0); // range
          env = decoder.env("level");
        } break;
      case 3: { // HARSH
          decoder.u8(0); // shape
          env = decoder.env("level");
        } break;
      case 4: { // HARM
          let harmc = decoder.u8(0);
          if (decoder.srcp > decoder.src.length - harmc * 2) break;
          while (harmc-- > 0) decoder.u16(0);
          env = decoder.env("level");
        } break;
    }
    const tail = [0, 0, 0, 0];
    if (!env) return tail;
    let i;
    if (env.flags & 0x04) { // Sustain in play.
      for (i=1; i<env.susp; i++) tail[0] += env.lo[i].t;
      for (; i<env.lo.length; i++) tail[2] += env.lo[i].t;
      if (!tail[2]) tail[2] = 1; // Can't be zero, it's the "sustain" flag too.
      if (env.hi) {
        for (i=1; i<env.susp; i++) tail[1] += env.hi[i].t;
        for (; i<env.hi.length; i++) tail[3] += env.hi[i].t;
      } else {
        tail[1] = tail[0];
        tail[3] = tail[2];
      }
    } else {
      for (const pt of env.lo) tail[0] += pt.t;
      if (env.hi) {
        for (const pt of env.hi) tail[1] += pt.t;
      } else {
        tail[1] = tail[0];
      }
    }
    return tail;
  }
}

/* SongEvent.
 ***************************************************************************************/
 
export class SongEvent {
  constructor(srcOrTime, type) {
    this._init(srcOrTime, type);
    if (typeof(srcOrTime) === "number") ;
    else if (!srcOrTime) ;
    else if (srcOrTime instanceof SongEvent) this._copy(srcOrTime);
    else throw new Error(`Unexpected input to SongEvent`);
  }
  
  // New event with all the fields, on the assumption that user is going to change type.
  static newFullyPopulated() {
    const event = new SongEvent();
    event.type = "note";
    event.noteid = 0x40;
    event.velocity = 0x40;
    event.chid = 0;
    event.durms = 0;
    event.wheel = 0x100;
    return event;
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
