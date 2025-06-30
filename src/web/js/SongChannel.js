/* SongChannel.js
 * Subservient to a SongPlayer, representing one channel.
 * We're responsible both for its event bus and its signal bus.
 */
 
import { calculateEauDuration } from "./songBits.js";

export class SongChannel {
  
  /* Construction may throw exceptions for malformed data.
   * (modecfg,post) should be Uint8Array if present or null if absent.
   * (trim,pan) are 8 bit, exactly as encoded.
   */
  constructor(player, chid, trim, pan, mode, modecfg, post) {
    this.player = player;
    this.chid = chid;
    this.trim = trim / 255.0;
    this.pan = (pan - 0x80) / 127.0;
    this.mode = mode;
    
    switch (mode) {
      case 1: this.initDrum(modecfg); break;
      case 2: this.initFm(modecfg); break;
      case 3: this.initHarsh(modecfg); break;
      case 4: this.initHarm(modecfg); break;
      default: this.mode = 0;
    }
    if (!this.mode) return;
    
    this.postStart = new GainNode(this.player.ctx, { gain: 1 }); // Voices attach here.
    this.postEnd = new GainNode(this.player.ctx, { gain: this.trim }); // Post attaches here.
    this.postEnd.connect(player.node);
    let postTail = this.postStart; // Last node of post, or postStart.
    if (post) {
      for (let srcp=0; srcp<post.length; ) {
        const stageid = post[srcp++];
        const len = post[srcp++];
        if (srcp > post.length - len) throw new Error("Malformed EAU");
        const body = new Uint8Array(post.buffer, post.byteOffset + srcp, len);
        srcp += len;
        const next = this.addPostStage(stageid, body, postTail);
        if (next) postTail = next;
      }
    }
    postTail.connect(this.postEnd);
  }
  
  /* Events from song.
   **********************************************************************************/
  
  // (when) in context seconds, (velocity) in 0..1; (durs) in seconds.
  note(when, noteid, velocity, durs) {
    //console.log(`SongChannel[${this.uniq}].note chid=${this.chid} when=${when} noteid=${noteid} velocity=${velocity} durs=${durs}`);//TODO
    if (this.mode === 1) return this.playDrum(when, this.notes[noteid], velocity);
    if ((this.mode >= 2) && (this.mode <= 4)) return this.tunedNote(when, noteid, velocity, durs);
  }
  
  // (when) in context seconds, (v) in -512..511.
  wheel(when, v) {
    //console.log(`SongChannel[${this.uniq}].wheel chid=${this.chid} when=${when} v=${v}`);//TODO
    if ((this.mode >= 2) && (this.mode <= 4)) return this.tunedWheel(when, v);
  }
  
  /* Add a stage to post.
   * Added nodes should connect (input) to themselves.
   * Return the last node you add, or null if nothing.
   *********************************************************************************/
   
  addPostStage(stageid, body, input) {
    switch (stageid) {
      case 1: return this.addPostStageDelay(body, input);
      case 2: return this.addPostStageWaveshaper(body, input);
      case 3: return this.addPostStageTremolo(body, input);
    }
    return null;
  }
  
  addPostStageDelay(body, input) {
    //TODO Delay
  }
  
  addPostStageWaveshaper(body, input) {
    //TODO Waveshaper
  }
  
  addPostStageTremolo(body, input) {
    //TODO Tremolo
  }
  
  /* Drum mode.
   ***********************************************************************/
   
  initDrum(src) {
    if (!src) {
      this.mode = 0;
      return;
    }
    this.notes = []; // Sparse, indexed by noteid. { trimlo,trimhi,pan: float, serial: Uint8Array, pcm?: AudioBuffer, pending?: {when,velocity}[] }
    for (let srcp=0; srcp<src.length; ) {
      const noteid = src[srcp++];
      const trimlo = src[srcp++];
      const trimhi = src[srcp++];
      const pan = src[srcp++];
      const len = (src[srcp] << 8) | src[srcp+1];
      srcp += 2;
      if (srcp > src.length - len) throw new Error("Malformed EAU");
      const serial = new Uint8Array(src.buffer, src.byteOffset + srcp, len);
      srcp += len;
      this.notes[noteid] = {
        trimlo: trimlo / 127.0,
        trimhi: trimhi / 127.0,
        pan: (pan - 0x80) / 128.0,
        serial,
      };
    }
  }
  
  playDrum(when, note, velocity) {
    if (!note) return;
    if (!note.pcm) {
      if (note.pending) {
        // We got a request for a note that's currently being printed.
        // This is normal, especially for hi hats at the start of a song.
        // Hold them in (note.pending) until it resolves.
        note.pending.push({ when, velocity });
        return;
      }
      const framec = calculateEauDuration(note.serial, this.player.ctx.sampleRate);
      if (framec < 1) {
        const p = this.notes.indexOf(note);
        if (p >= 0) this.notes[p] = null;
        return;
      }
      const subctx = new OfflineAudioContext(1, framec, this.player.ctx.sampleRate);
      const songPlayer = new SongPlayer(subctx, note.serial, 1.0, 0.0, false, 0);
      songPlayer.play();
      songPlayer.update(subctx.currentTime + framec / subctx.sampleRate + 0.200);
      note.pending = [{ when, velocity }];
      subctx.startRendering().then(result => {
        note.pcm = result;
        const pending = note.pending;
        delete note.pending;
        for (const p of pending) {
          // It's possible for (p.when) to be in the past now. Take it on the chin, and let WebAudio play it right now instead.
          this.playDrum(p.when, note, p.velocity);
        }
      });
      return;
    }
    const node = new AudioBufferSourceNode(this.player.ctx, {
      buffer: note.pcm,
    });
    node.connect(this.postStart);
    node.start(when);
  }
  
  /* Tuned voices: FM, HARSH, HARM.
   **************************************************************************/
   
  initFm(src) {
    //TODO
  }
  
  initHarsh(src) {
    //TODO
  }
  
  initHarm(src) {
    //TODO
  }
   
  tunedNote(when, noteid, velocity, durs) {
    //TODO
    //XXX Making a very simple voice just so I can get something out the speaker, as a sanity check...
    //...Sounds fine as far as timing and repeat. No obvious memory leaks.
    const frequency = 440 * Math.pow(2, (noteid - 0x45) / 12);
    const peak = 0.300;
    if (durs < 0.040) durs = 0.040;
    const endTime = when + durs + 0.200;
    const osc = new OscillatorNode(this.player.ctx, { frequency });
    osc.start();
    const env = new GainNode(this.player.ctx, { gain: 0 });
    env.gain.setValueAtTime(0, when);
    env.gain.linearRampToValueAtTime(peak, when + 0.020);
    env.gain.linearRampToValueAtTime(peak * 0.250, when + 0.040);
    env.gain.linearRampToValueAtTime(peak * 0.250, when + durs);
    env.gain.linearRampToValueAtTime(0, endTime);
    osc.connect(env);
    env.connect(this.postStart);
    setTimeout(() => { // XXX surely there's a better way to remove after completion
      osc.stop();
      osc.disconnect();
      env.disconnect();
    }, (endTime - this.player.ctx.currentTime + 0.100) * 1000);
  }
  
  tunedWheel(when, v) {
    //TODO
  }
}
