/* Song.js
 * Data model for a song or sound resource.
 * We only deal with EAU. Not MIDI or EAU-Text.
 * That's a controversial choice for sure, since MIDI and EAUT can contain data that EAU doesn't.
 * But it makes the editor much more tractable, and hey, it all has to end up EAU eventually.
 * The main rationale is I don't want multiple implementations of MIDI<~>EAU conversion within the project.
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
    this.channels = []; // SongChannel, packed, not necessarily sorted.
    this.channelsByChid = []; // WEAK SongChannel, sparse, index is chid.
    this.events = []; // SongEvent, sorted by time. Note that Delay is not an event in this model.
    this.text = []; // [chid,noteid,name], sorted.
    this.dropNoopChannels = false; // InstrumentsEditor sets this true, and any channel with mode NOOP will be dropped at encode.
  }
  
  _copy(src) {
    this.tempo = src.tempo;
    this.channels = src.channels.map(c => new SongChannel(c));
    this.channelsByChid = [];
    for (const channel of this.channels) this.channelsByChid[channel.chid] = channel;
    this.events = src.events.map(e => new SongEvent(e));
  }
  
  _decode(src) {
    
    // If the input is empty, call it ok. We're already in a sane default state.
    if (!src?.length) return;
    
    // Split into the three main chunks, and capture (tempo), the only global header field.
    if (src.length < 10) throw new Error(`Invalid length ${src.length} for EAU file.`);
    if ((src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x41) || (src[3] !== 0x55)) {
      throw new Error(`EAU signature mismatch`);
    }
    this.tempo = (src[4] << 8) | src[5];
    const chdrlen = (src[6] << 24) | (src[7] << 16) | (src[8] << 8) | src[9];
    let srcp = 10;
    if ((chdrlen < 0) || (srcp > src.length - chdrlen)) throw new Error(`Unexpected EOF in EAU Channel Headers.`);
    const chdr = new Uint8Array(src.buffer, src.byteOffset + srcp, chdrlen);
    srcp += chdrlen;
    if (srcp > src.length - 4) throw new Error(`EAU file missing Events block.`);
    const evtlen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
    srcp += 4;
    if ((evtlen < 0) || (srcp > src.length - evtlen)) throw new Error(`Unexpected EOF in EAU Events.`);
    const evt = new Uint8Array(src.buffer, src.byteOffset + srcp, evtlen);
    srcp += evtlen;
    let text = null; // Allow text to be missing.
    if (srcp <= src.length - 4) {
      const textlen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
      srcp += 4;
      if ((textlen < 0) || (srcp > src.length - textlen)) throw new Error(`Unexpected EOF in EAU Text.`);
      text = new Uint8Array(src.buffer, src.byteOffset + srcp, textlen);
      srcp += textlen;
    }
    
    // Enter each of those blocks.
    this._decodeChdr(chdr);
    this._decodeEvents(evt);
    this._decodeText(text);
    
    // Warn and create a default channel for any events that lack a channel.
    // The MIDI-to-EAU conversion that should have preceded us should have taken care of this, but let's be sure.
    for (const event of this.events) {
      if (event.chid < 0) continue;
      if (this.channelsByChid[event.chid]) continue;
      const channel = new SongChannel(event.chid);
      this.channels.push(channel);
      this.channelsByChid[event.chid] = channel;
      console.warn(`Channel ${event.chid} was not configured but contains at least one event. Created a default config for it.`, { channel });
    }
  }
  
  _decodeChdr(src) {
    for (let srcp=0; srcp<src.length; ) {
      if (srcp > src.length - 6) throw new Error(`Short Channel Headers`);
      const chid = src[srcp++];
      const trim = src[srcp++];
      const pan = src[srcp++];
      const mode = src[srcp++];
      const modecfglen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - modecfglen) throw new Error(`Short Channel Headers`);
      const modecfg = new Uint8Array(src.buffer, src.byteOffset + srcp, modecfglen);
      srcp += modecfglen;
      if (srcp > src.length - 2) throw new Error(`Short Channel Headers`);
      const postlen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - postlen) throw new Error(`Short Channel Headers`);
      const post = new Uint8Array(src.buffer, src.byteOffset + srcp, postlen);
      srcp += postlen;
      const channel = new SongChannel({ chid, trim, pan, mode, modecfg, post });
      
      // In case of duplicates, log the decoded channel for troubleshooting, but keep the first.
      // Behavior for this situation is explicitly undefined by our spec.
      if (this.channelsByChid[chid]) {
        console.warn(`EAU channel ${chid} configured more than once. Using the first one.`, channel);
        continue;
      }
      this.channels.push(channel);
      this.channelsByChid[chid] = channel;
    }
  }
  
  _decodeEvents(src) {
    let now = 0;
    const holds = []; // SongEvent, all "note" with (durms==0).
    for (let srcp=0; srcp<src.length; ) {
      const lead = src[srcp++];
      
      // High bit unset is Delay, one of two formats.
      if (!(lead & 0x80)) {
        if (lead & 0x40) now += ((lead & 0x3f) + 1) << 6;
        else now += lead;
        continue;
      }
      
      // All other events are distinguished by the top four bits.
      switch (lead & 0xf0) {
      
        case 0x80: { // Note Off
            const chid = lead & 0x0f;
            const noteid = src[srcp++];
            const velocity = src[srcp++];
            // The oldest matching Note On wins. Not sure what one is supposed to do when On/Off pairs are nested.
            const holdp = holds.findIndex(h => ((h.chid === chid) && (h.noteid === noteid)));
            if (holdp < 0) {
              console.warn(`Dropping Note Off chid=${chid} noteid=${noteid} at time ${now}ms; Note On not found.`);
              break;
            }
            const event = holds[holdp];
            holds.splice(holdp, 1);
            event.durms = now - event.time;
          } break;
          
        case 0x90: { // Note On
            const chid = lead & 0x0f;
            const noteid = src[srcp++];
            const velocity = src[srcp++];
            const event = new SongEvent(now, "note");
            event.chid = chid;
            event.noteid = noteid;
            event.velocity = velocity;
            holds.push(event); // Expect Note Off.
            this.events.push(event);
          } break;
          
        case 0xa0: { // Note Once
            const chid = lead & 0x0f;
            const a = src[srcp++];
            const b = src[srcp++];
            const c = src[srcp++];
            const noteid = a >> 1;
            const velocity = ((a & 1) << 6) | (b >> 2);
            const durms = (((b & 3) << 8) | c) << 4;
            const event = new SongEvent(now, "note");
            event.chid = chid;
            event.noteid = noteid;
            event.velocity = velocity;
            event.durms = durms;
            this.events.push(event);
          } break;
          
        case 0xe0: { // Wheel
            const chid = lead & 0x0f;
            const a = src[srcp++];
            const b = src[srcp++];
            const v = a | (b << 7);
            const event = new SongEvent(now, "wheel");
            event.chid = chid;
            event.wheel = v;
            this.events.push(event);
          } break;
          
        case 0xf0: { // Marker
            const event = new SongEvent(now, "marker");
            event.mark = lead & 0x0f;
            this.events.push(event);
          } break;
          
        default: { // Undefined event (0xb0,0xc0,0xd0). Error.
            throw new Error(`Unexpected leading byte 0x${lead.toString(16)} in EAU Events at ${srcp-1}/${src.length}.`);
          }
      }
    }
    // Issue a warning for missing Note Off.
    if (holds.length) {
      console.warn(`${holds.length} Note On events had no Note Off and will be interpretted as zero sustain.`);
    }
    // Insert a dummy event to capture any trailing delay.
    const event = new SongEvent(now, "marker");
    event.mark = -1;
    this.events.push(event);
  }
  
  _decodeText(src) {
    if (!src?.length) return;
    const textDecoder = new TextDecoder("utf8");
    for (let srcp=0; srcp<src.length; ) {
      const chid = src[srcp++];
      const noteid = src[srcp++] || 0;
      const len = src[srcp++] || 0;
      if (srcp>src.length - len) break;
      let p = this.textSearch(chid, noteid);
      if (p < 0) {
        p = -p - 1;
        const v = textDecoder.decode(new Uint8Array(src.buffer, src.byteOffset + srcp, len));
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
      if (noteid < 0x80) return `${chid}:${noteid}: ${this.text[p][2]}`;
      return `${chid}: ${this.text[p][2]}`;
    }
    if (noteid < 0x80) return `Note ${noteid} on channel ${chid}`;
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
      if (name.length > 0xff) name = name.substring(0, 255);
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
    
    // Header.
    encoder.raw("\0EAU");
    encoder.u16be(this.tempo);
    
    // Channel Headers.
    const skippedChids = new Set();
    encoder.pfxlen(4, () => {
      for (const channel of this.channels) {
        if (channel.skipEncode) {
          skippedChids.add(channel.chid);
          continue;
        }
        if (this.dropNoopChannels && !channel.mode) continue;
        encoder.u8(channel.chid);
        encoder.u8(channel.trim);
        encoder.u8(channel.pan);
        encoder.u8(channel.mode);
        if (channel.modecfg.length > 0xffff) throw new Error(`Channel ${channel.chid} modecfg length ${channel.modecfg.length} > 65535`);
        encoder.u16be(channel.modecfg.length);
        encoder.raw(channel.modecfg);
        if (channel.skipPost) {
          encoder.u16be(0);
        } else {
          if (channel.post.length > 0xffff) throw new Error(`Channel ${channel.chid} post length ${channel.post.length} > 65535`);
          encoder.u16be(channel.post.length);
          encoder.raw(channel.post);
        }
      }
    });
    
    // Events.
    encoder.pfxlen(4, () => {
      this._encodeEvents(encoder, skippedChids);
    });
    
    // Text.
    encoder.pfxlen(4, () => {
      for (const [chid, noteid, name] of this.text) {
        encoder.u8(chid);
        encoder.u8(noteid);
        encoder.u8(name.length);
        encoder.raw(name);
      }
    });

    return encoder.finish();
  }
  
  _encodeEvents(encoder, skippedChids) {
    /* We're responsible for detecting "note" events with excessive (durms), and splitting those into On/Off pairs.
     * Plenty of opportunity for subtle error here, so I'm going to err on the side of caution and compose a single-use event list first, which matches what we'll encode.
     * Do not include Delay events tho; we'll manage that during the encode pass.
     */
    const events = []; // SongEvent or lookalike. (type) can also be "Note On" and "Note Off".
    let longnotec = 0; // If nonzero, (events) is probably not sorted.
    let endTime = this.events[this.events.length - 1]?.time || 0; // Our EOF marker is otherwise ignored.
    for (let event of this.events) {
      if (skippedChids.has(event.chid)) continue;
      switch (event.type) {
        case "note": {
            if ((event.chid < 0) || (event.chid > 0xf)) break;
            if (event.durms > 16368) {
              events.push({
                time: event.time,
                type: "Note On",
                chid: event.chid,
                noteid: event.noteid,
                velocity: event.velocity,
              });
              events.push({
                time: event.time + event.durms,
                type: "Note Off",
                chid: event.chid,
                noteid: event.noteid,
                velocity: event.velocity,
              });
              longnotec++;
            } else {
              events.push(event);
            }
          } break;
        case "wheel": {
            if ((event.chid < 0) || (event.chid > 0xf)) break;
            events.push(event);
          } break;
        case "marker": {
            if ((event.mark < 0) || (event.mark > 0xf)) break;
            events.push(event);
          } break;
      }
    }
    /* If there are any long notes, (events) is probably out of order, so re-sort it.
     * We depend on Array.sort being stable. This is guaranteed as of ES2019, but might break in old browsers.
     * Long notes can also change our end time, if a note sustains beyond the end and also longer than 16368 ms.
     */
    if (longnotec) {
      events.sort((a, b) => a.time - b.time);
      const nendtime = events[events.length - 1].time;
      if (nendtime > endTime) endTime = nendtime;
    }
    /* OK now we have the real set of encodable events.
     * Iterate those, and insert delays JITly.
     */
    let wtime = 0; // Sum of emitted delays.
    const FLUSH_DELAY = (toTime) => {
      let delay = toTime - wtime;
      if (delay <= 0) return;
      while (delay >= 4096) {
        encoder.u8(0x7f);
        delay -= 4096;
      }
      if (delay >= 64) {
        encoder.u8(0x40 | ((delay >> 6) - 1));
        delay &= 0x3f;
      }
      if (delay > 0) {
        encoder.u8(delay);
      }
      wtime = toTime;
    };
    for (const event of events) {
      FLUSH_DELAY(event.time);
      switch (event.type) {
        case "Note Off": {
            encoder.u8(0x80 | event.chid);
            encoder.u8(event.noteid);
            encoder.u8(event.velocity);
          } break;
        case "Note On": {
            encoder.u8(0x90 | event.chid);
            encoder.u8(event.noteid);
            encoder.u8(event.velocity);
          } break;
        case "note": {
            const dur = Math.max(0, Math.min(0x3ff, (event.durms + 8) >> 4)); // +8 to round
            encoder.u8(0xa0 | event.chid);
            encoder.u8((event.noteid << 1) | (event.velocity >> 6));
            encoder.u8((event.velocity << 2) | (dur >> 8));
            encoder.u8(dur & 0xff);
          } break;
        case "wheel": {
            encoder.u8(0xe0 | event.chid);
            encoder.u8(event.wheel & 0x7f);
            encoder.u8(event.wheel >> 7);
          } break;
        case "marker": {
            encoder.u8(0xf0 | event.mark);
          } break;
      }
    }
    // A final delay, if we're below (endTime), which is very likely.
    FLUSH_DELAY(endTime);
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
    if (final.type !== "marker") {
      if (endTime < final.time) return false; // oops?
      this.events.push(new SongEvent(endTime, "marker"));
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
    else if (src.hasOwnProperty("chid")) this._copy(src); // SongChannel lookalike, for the initial decode.
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
  
  // (src) may be a SongChannel or a lookalike with all 6 fields.
  _copy(src) {
    this.chid = src.chid;
    this.trim = src.trim;
    this.pan = src.pan;
    this.mode = src.mode;
    this.modecfg = new Uint8Array(src.modecfg);
    this.post = new Uint8Array(src.post);
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
      case 1: { // TRIVIAL. No envelope and no velocity.
          decoder.u16(0); // wheelrange
          decoder.u16(0); // minlevel
          decoder.u16(0); // maxlevel
          const minhold = decoder.u16(0);
          const rlstime = decoder.u16(0);
          return [minhold, minhold, rlstime, rlstime];
        }
      case 2: { // FM
          env = decoder.env("level");
        } break;
      case 3: { // SUB
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
    event.type = "marker";
    event.noteid = 0x40;
    event.velocity = 0x40;
    event.chid = 0;
    event.durms = 0;
    event.wheel = 0x100;
    event.mark = -1;
    return event;
  }
  
  _init(time, type) {
    if (!SongEvent.nextId) SongEvent.nextId = 1;
    this.id = SongEvent.nextId++; // Unique across the entire session.
    if (typeof(time) === "number") this.time = time;
    else this.time = 0; // ms
    this.type = type || "marker"; // "note","wheel","marker"
    this.chid = -1;
    switch (this.type) {
      case "note": {
          this.noteid = 0x40; // 0..127
          this.velocity = 0x40; // 0..127
          this.durms = 0; // >=0. No upper bound -- that's the encoder's problem.
        } break;
      case "wheel": {
          this.wheel = 0x2000; // 0..0x2000..0x3fff
        } break;
      case "marker": {
          this.mark = -1; // 0..15, or <0 for temporary editor-only markers like EOF.
        } break;
      default: throw new Error(`Unknown song event type ${JSON.stringify(type)}`);
    }
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
      case "marker": {
          this.mark = src.mark;
        } break;
    }
  }
}
