/* SongPlayer.js
 * Given an encoded EAU song and an AudioContext, we manage everything.
 */
 
import { SongChannel } from "./SongChannel.js";

const FORWARD_TIME = 2.000;
 
export class SongPlayer {

  /* The EAU file is decoded but not 100% validated during construction.
   * Allow for exceptions during construction.
   */
  constructor(ctx, src, trim, pan, repeat, id) {
    console.log(`SongPlayer.constructor id=${id}`, { ctx });
    this.ctx = ctx;
    this.serial = src;
    this.trim = trim;
    this.pan = pan;
    this.repeat = repeat;
    this.id = id;
    
    this.eventp = 0;
    this.eventTime = 0; // Context time at (eventp), does not wrap.
    this.startTime = 0; // Context time at start, does not wrap.
    this.eventsFinished = false; // True if we've processed them all, and not repeating.
    this.running = false;
    this.node = new GainNode(ctx, { gain: 1 });
    
    this.tempo = 0; // Serves as flag for "\0EAU" present.
    this.loopp = 0;
    this.events = null; // Uint8Array
    this.channels = []; // Packed, and only contains those with low chid.
    this.channelsByChid = []; // Sparse, weak. Indexed by chid, only low ones.
    for (let srcp=0; srcp<src.length; ) {
      const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
      const chunklen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
      if ((chunklen < 0) || (srcp > src.length - chunklen)) throw new Error("Malformed EAU");
      const chunk = new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen);
      srcp += chunklen;
      switch (chunkid) {
        case 0x00454155: { // "\0EAU"
            if (chunklen >= 2) {
              this.tempo = (chunk[0] << 8) | chunk[1];
              if (!this.tempo) throw new Error("Malformed EAU");
              if (chunklen >= 4) {
                this.loopp = (chunk[2] << 8) | chunk[3];
              }
            } else {
              // There is a default tempo, but the header chunk is required even if empty.
              this.tempo = 500;
            }
          } break;
        case 0x43484452: { // "CHDR"
            if (!this.tempo) throw new Error("Malformed EAU");
            this.receiveChannelHeader(chunk);
          } break;
        case 0x45565453: { // "EVTS"
            if (this.events) throw new Error("Malformed EAU");
            this.events = chunk;
          } break;
      }
    }
    if (!this.tempo) throw new Error("Malformed EAU"); // Missing introducer chunk.
    if (!this.events) {
      // The spec doesn't strictly require events to be present.
      // Create a single long delay event if there's nothing.
      this.events = new Uint8Array([0x7f]);
    }
    if (this.loopp > this.events.length) throw new Error("Malformed EAU");
  }
  
  receiveChannelHeader(src) {
    if (src.length < 1) return; // Grudgingly allow that this is legal, but it's noop.
    const chid = src[0];
    if (chid >= 0x10) return; // High chid are legal but noop (not addressable by events).
    if (this.channelsByChid[chid]) return; // Duplicate chid. Ignore, same as native implementation. Spec leaves this case undefined.
    const trim = src[1] ?? 0x40;
    const pan = src[2] ?? 0x80;
    const mode = src[3] ?? 2;
    let srcp = 4;
    let modecfg=null, post=null;
    if (srcp <= src.length - 2) {
      const modecfglen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (modecfglen) {
        if (srcp > src.length - modecfglen) throw new Error("Malformed EAU");
        modecfg = new Uint8Array(src.buffer, src.byteOffset + srcp, modecfglen);
        srcp += modecfglen;
      }
    }
    if (srcp <= src.length - 2) {
      const postlen = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (postlen) {
        if (srcp > src.length - postlen) throw new Error("Malformed EAU");
        post = new Uint8Array(src.buffer, src.byteOffset + srcp, postlen);
        srcp += postlen;
      }
    }
    const channel = new SongChannel(this, chid, trim, pan, mode, modecfg, post);
    this.channels.push(channel);
    this.channelsByChid[chid] = channel;
  }
  
  /* Public API.
   ********************************************************************************/
  
  play() {
    console.log(`SongPlayer.play ${this.id}`);//TODO
    if (this.running) return;
    this.running = true;
    if (!this.eventTime) this.eventTime = this.startTime = this.ctx.currentTime;
    this.node.connect(this.ctx.destination);
  }
  
  // Hard stop immediately.
  stop() {
    console.log(`SongPlayer.stop ${this.id}`);//TODO
    if (!this.running) return;
    this.running = false;
    this.node.disconnect();
  }
  
  // Schedule fade-out if we're doing that. Some future update must return false.
  stopSoon() {
    console.log(`SongPlayer.stopSoon ${this.id}`);//TODO
    if (!this.running) return;
    const fadeTime = 0.250;
    this.node.gain.setValueAtTime(1, this.ctx.currentTime);
    this.node.gain.linearRampToValueAtTime(0, this.ctx.currentTime + fadeTime);
    setTimeout(() => this.stop(), 300);
  }
  
  getPlayhead() {
    if (!this.running) return 0.0;
    //TODO Need to track repeats and wrap this reported time around. Tricky...
    return Math.max(0.0, this.ctx.currentTime - this.startTime);
  }
  
  setPlayhead(ph) {
    console.log(`SongPlayer.setPlayhead ${this.id} =${ph}`);//TODO
  }
  
  // Return false when finished.
  update(forwardTime) {
    if (!this.running) return false;
    if (!this.eventsFinished) {
      if (!forwardTime) forwardTime = this.ctx.currentTime + FORWARD_TIME;
      while (this.eventTime < forwardTime) {
        this.nextEvent();
        if (this.eventsFinished) break;
      }
    }
    return true;
  }
  
  /* Private.
   *************************************************************************/
  
  /* Process the next event, advance (eventp) and (eventTime) as warranted,
   * and loop or set (eventsFinished) at EOF.
   */
  nextEvent() {
    if (this.eventp >= this.events.length) {
      if (this.repeat) {
        this.eventp = this.loopp;
        this.eventTime += 0.001; // Must advance time by a smidgeon when looping, in case the song is empty.
      } else {
        this.eventsFinished = true;
        const fadeTime = 0.250;
        this.node.gain.setValueAtTime(1, this.eventTime);
        this.node.gain.linearRampToValueAtTime(0, this.eventTime + fadeTime);
        setTimeout(() => this.stop(), (this.eventTime + fadeTime - this.ctx.currentTime + 0.100) * 1000);
      }
      return;
    }
    const lead = this.events[this.eventp++];
    switch (lead & 0xc0) {
      case 0x00: { // Short Delay.
          this.eventTime += lead / 1000;
        } break;
      case 0x40: { // Long Delay.
          this.eventTime += (((lead & 0x3f) + 1) << 6) / 1000;
        } break;
      case 0x80: { // Note.
          const a = this.events[this.eventp++];
          const b = this.events[this.eventp++];
          const c = this.events[this.eventp++];
          const chid = (lead >> 2) & 15;
          const noteid = ((lead & 3) << 5) | (a >> 3);
          const velocity = ((a & 7) << 4) | (b >> 4);
          const durms = ((b & 15) << 8) | c;
          const channel = this.channelsByChid[chid];
          if (!channel) break; //TODO Spec mandates a default.
          channel.note(this.eventTime, noteid, velocity / 127, durms / 1000);
        } break;
      case 0xc0: { // Wheel.
          const a = this.events[this.eventp++];
          const chid = (lead >> 2) & 15;
          const v = (((lead & 3) << 8) | a) - 512;
          const channel = this.channelsByChid[chid];
          if (!channel) break; // TODO Spec mandates a default.
          channel.wheel(this.eventTime, v);
        } break;
    }
  }
}
