/* Song.js
 * EAU song player.
 * Our owner creates the AudioContext and gives us the encoded song resource.
 * Our main responsibility is decoding the song and paying out events in real time.
 * Production of the real signal nodes belongs to Channel.
 */
 
import { SongChannel } from "./SongChannel.js";

/* How far in the future we will schedule events.
 * This needn't be any more than the longest frame length, but I feel safer having a few seconds of slack.
 */
const FORWARD_TIME = 2.000;
 
export class Song {
  constructor(serial, trim, pan, repeat) {
    this.serial = serial;
    this.trim = trim;
    this.pan = pan;
    this.repeat = repeat;
    this.ctx = null;
    this.tempo = 250; // ms/qnote
    this.tempos = 0.250; // s/qnote
    this.loopp = 0; // bytes into event stream
    this.channels = []; // SongChannel, sparse, indexed by chid.
    this.events = []; // Uint8Array, encoded events.
    this.eventp = 0; // Byte offset in (events).
    this.eventTime = 0; // s, relative time corresponding to (eventp). Usually ahead of real time.
    this.startTime = 0; // s, context time at start.
    this.loopTime = 0; // s, relative time at the most recent loop.
    this.draining = false; // If true, no more events, just wait for playback to stop.
    this.decode(serial);
  }
  
  play(ctx) {
    console.log(`TODO Song.play`);
    this.ctx = ctx;
    this.songStartTime = ctx.currentTime;
    for (const channel of this.channels) {
      channel?.play(ctx);
    }
  }
  
  stop() {
    for (const channel of this.channels) {
      channel?.stop();
    }
    this.ctx = null;
  }
  
  // Returns true if still running.
  update() {
    if (!this.ctx) return false;
    for (const channel of this.channels) {
      if (!channel) continue;
      channel.update();
    }
    if (this.draining) {
      for (const channel of this.channels) {
        if (!channel) continue;
        if (!channel.isFinished()) return true;
      }
      return false;
    }
    const untilTime = this.ctx.currentTime + FORWARD_TIME - this.startTime;
    while (this.eventTime < untilTime) {
      if (this.eventp >= this.events.length) return this.loopOrTerminate();
      const lead = this.events[this.eventp++];
      if (!lead) return this.loopOrTerminate(); // Explicit EOF.
      if (lead < 0x80) { // Short Delay.
        this.eventTime += lead / 1000;
        continue;
      }
      switch (lead & 0xf0) { // All other events have a 4-bit opcode similar to MIDI.
        case 0x80: { // Long Delay.
            const ms = ((lead & 0x0f) + 1) << 7;
            this.eventTime += ms / 1000;
          } break;
        case 0x90: // Short Note.
        case 0xa0: // Medium Note.
        case 0xb0: { // Long Note.
            if (this.eventp > this.events.length - 2) return this.terminate();
            const a = this.events[this.eventp++];
            const b = this.events[this.eventp++];
            const chid = lead & 0x0f;
            const channel = this.channels[chid];
            if (!channel) break;
            const noteid = a >> 1;
            const velocity = (((a & 1) << 3) | (b >> 5)) / 15.0; // 0..1
            let durms = b & 0x1f;
            switch (lead & 0xf0) {
              case 0x90: break;
              case 0xa0: durms = (durms + 1) << 5; break;
              case 0xb0: durms = (durms + 1) << 10; break;
            }
            channel.note(noteid, velocity, this.startTime + this.eventTime, durms / 1000);
          } break;
        case 0xc0: { // Wheel.
            if (this.eventp > this.events.length - 1) return this.terminate();
            const chid = lead & 0x0f;
            const v = this.events[this.eventp++];
            const channel = this.channels[chid];
            if (!channel) break;
            channel.wheel((v - 0x80) / 128, this.startTime + this.eventTime);
          } break;
        default: return this.terminate(); // Anything else is illegal.
      }
    }
    return true;
  }
  
  getPlayhead() {
    if (!this.ctx) return 0;
    let ph = this.ctx.currentTime - this.startTime;
    if (ph > this.loopTime) ph -= this.loopTime;
    return ph;
  }
  
  setPlayhead(ph) {
    console.log(`TODO Song.setPlayhead ${ph}`);//TODO This is going to be complicated.
  }
  
  /* Private.
   ******************************************************************************************/
  
  /* Explicit termination, eg for misencoded events.
   * This will cause output to cut off abruptly.
   */
  terminate() {
    this.eventp = this.events.length;
    this.repeat = false;
    return false;
  }
  
  /* Called at the end of the event stream.
   * Return false only if we're ready to end playback.
   */
  loopOrTerminate() {
    if (!this.repeat) {
      this.draining = true;
      return true;
    }
    this.eventp = this.loopp;
    this.loopTime = this.eventTime;
    return true;
  }
   
  decode(src) {
    
    if (src.length < 8) return;
    if ((src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x41) || (src[3] !== 0x55)) return;
    this.tempo = (src[4] << 8) | src[5];
    this.loopp = (src[6] << 8) | src[7];
    this.tempos = this.tempo / 1000;
    
    let srcp = 8;
    while (srcp < src.length) {
      const chid = src[srcp++];
      if (chid === 0xff) break; // Channel Headers Terminator
      const trim = src[srcp++];
      const pan = src[srcp++];
      const mode = src[srcp++];
      const paylen = (src[srcp] << 8) | src[srcp+1]; srcp += 2;
      if (srcp > src.length - paylen) break;
      const payload = new Uint8Array(src.buffer, src.byteOffset + srcp, paylen);
      srcp += paylen;
      const postlen = (src[srcp] << 8) | src[srcp+1]; srcp += 2;
      if (srcp > src.length - postlen) break;
      const post = new Uint8Array(src.buffer, src.byteOffset + srcp, postlen);
      srcp += postlen;
      if (!mode) continue; // Mode zero is explicitly noop.
      if (!trim) continue; // Ditto for trim zero, no sense producing a signal if it's going to be attenuated to silence.
      if (chid >= 0x10) continue; // Chid 16..254 are technically legal, eg the built-in instrument definitions does that. But events can't be encoded for these high channels, so they are noop.
      const channel = new SongChannel(chid, trim / 255, (pan - 0x80) / 128, mode, payload, post, this.tempos);
      if (!channel.mode) continue; // Channel may neuter itself if misencoded or whatever. That's cool, just drop it.
      this.channels[chid] = channel;
    }
    
    this.events = new Uint8Array(src.buffer, src.byteOffset + srcp, src.length - srcp);
    if (this.loopp >= this.events.length) this.loopp = 0;
  }
}
