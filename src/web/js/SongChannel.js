/* SongChannel.js
 * Subservient to a SongPlayer, representing one channel.
 * We're responsible both for its event bus and its signal bus.
 */
 
import { calculateEauDuration, EauDecoder, eauNotev, eauEnvApply } from "./songBits.js";

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
    if (this.pan) {
      const panNode = new StereoPannerNode(this.player.ctx, { pan: this.pan });
      this.postEnd.connect(panNode);
      panNode.connect(player.node);
    } else {
      this.postEnd.connect(player.node);
    }
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
    if (this.mode === 1) return this.playDrum(when, this.notes[noteid], velocity);
    if ((this.mode >= 2) && (this.mode <= 4)) return this.tunedNote(when, noteid, velocity, durs);
  }
  
  // (when) in context seconds, (v) in -512..511.
  wheel(when, v) {
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
    //- `0x01 DELAY`: u8.8 period qnotes=1.0, u0.8 dry=0.5, u0.8 wet=0.5, u0.8 store=0.5, u0.8 feedback=0.5, u8 sparkle(0..128..255)=0x80.
    let qnotes=1, dry=0.5, wet=0.5, sto=0.5, fbk=0.5, sparkle=0;
    if (body.length >= 2) {
      qnotes = ((body[0] << 8) | body[1]) / 256;
      if (body.length >= 3) dry = body[2] / 255;
      if (body.length >= 4) wet = body[3] / 255;
      if (body.length >= 5) sto = body[4] / 255;
      if (body.length >= 6) fbk = body[5] / 255;
      if (body.length >= 7) sparkle = (body[6] - 128) / 128;
    }
    const period = (qnotes * this.player.tempo) / 1000;
    if (period < 0.001) return null;
    let dst;
    if (sparkle) {
      const mlt = Math.pow(1.125, sparkle);
      const splitter = new ChannelSplitterNode(this.player.ctx, { numberOfOutputs: 2 });
      const inl = new GainNode(this.player.ctx, { gain: 1 });
      const inr = new GainNode(this.player.ctx, { gain: 1 });
      input.connect(inl);
      input.connect(inr);
      splitter.connect(inl, 0);
      splitter.connect(inr, 1);
      const dstl = this.makeDelay(inl, period * mlt, dry, wet, sto, fbk);
      const dstr = this.makeDelay(inr, period / mlt, dry, wet, sto, fbk);
      dst = new ChannelMergerNode(this.player.ctx, { numberOfInputs: 2 });
      dstl.connect(dst, 0, 0);
      dstr.connect(dst, 0, 1);
    } else {
      dst = this.makeDelay(input, period, dry, wet, sto, fbk);
    }
    return dst;
  }
  
  makeDelay(input, period, dry, wet, sto, fbk) {
    const delay = new DelayNode(this.player.ctx, { delayTime: period, maxDelayTime: period });
    const dryNode = new GainNode(this.player.ctx, { gain: dry });
    const wetNode = new GainNode(this.player.ctx, { gain: wet });
    const stoNode = new GainNode(this.player.ctx, { gain: sto });
    const fbkNode = new GainNode(this.player.ctx, { gain: fbk });
    const dst = new GainNode(this.player.ctx, { gain: 1 }); // Only because we need a single output node.
    input.connect(dryNode);
    dryNode.connect(dst);
    input.connect(stoNode);
    stoNode.connect(delay);
    delay.connect(fbkNode);
    fbkNode.connect(delay);
    delay.connect(wetNode);
    wetNode.connect(dst);
    return dst;
  }
  
  addPostStageWaveshaper(body, input) {
    const invc = body.length >> 1;
    if (!invc) return null; // As an edge case, the empty waveshaper is noop (not silence).
    const ptc = invc * 2 + 1;
    const coefv = new Float32Array(ptc);
    let pp=invc+1, np=invc-1;
    for (let bodyp=0; bodyp<body.length; bodyp+=2, pp++, np--) {
      coefv[pp] = ((body[bodyp] << 8) | body[bodyp+1]) / 65535.0;
      coefv[np] = -coefv[pp];
    }
    const node = new WaveShaperNode(this.player.ctx, { curve: coefv });
    input.connect(node);
    return node;
  }
  
  addPostStageTremolo(body, input) {
    let qnotes=1, depth=1, phase=0;
    if (body.length >= 2) {
      qnotes = ((body[0] << 8) | body[1]) / 256;
      if (body.length >= 3) {
        depth = body[2] / 255;
        if (body.length >= 4) {
          phase = body[3] / 255;
        }
      }
    }
    const frequency = 1000 / (qnotes * this.player.tempo);
    depth *= 0.5;
    phase *= Math.PI * 2;
    const real = new Float32Array([0, Math.sin(phase)*depth]);
    const imag = new Float32Array([0, Math.cos(phase)*depth]);
    const wave = new PeriodicWave(this.player.ctx, { real, imag, disableNormalization: true });
    const osc = new OscillatorNode(this.player.ctx, { shape: "custom", periodicWave: wave, frequency });
    const gain = new GainNode(this.player.ctx, { gain: 1.0 - depth });
    osc.start();
    osc.connect(gain.gain);
    input.connect(gain);
    return gain;
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
   * These share most plumbing. At init, a member (osc) gets created:
   *   osc(when,hz,velocity,durs): AudioNode
   **************************************************************************/
   
  initFm(src) {
    const decoder = new EauDecoder(src);
    const modrate = decoder.u16(0);
    let modrange = decoder.u16(0);
    this.levelenv = decoder.env("level");
    this.rangeenv = decoder.env("range");
    this.pitchenv = decoder.env("pitch");
    this.wheelrange = decoder.u16(200);
    let lforate = decoder.u16(0);
    let lfodepth = decoder.u8(0xff);
    let lfophase = decoder.u8(0);
    
    this.wheeli = 0;
    this.wheelCents = 0;
    this.tunable = []; // {node,until:seconds}
    
    if (modrate & 0x8000) {
      this.modabs = true;
      this.modrate = (modrate & 0x7fff) / 256; // qnotes
      this.modrate = 256000 / (this.modrate * this.player.tempo); // hz
    } else {
      this.modrate = modrate / 256;
    }
    
    // If rangeenv is not default, apply the master range.
    if (!(this.rangeenv.flags & 0x80)) {
      modrange /= 0x100;
      this.rangeenv.initlo *= modrange;
      this.rangeenv.inithi *= modrange;
      for (const pt of this.rangeenv.points) {
        pt.vlo *= modrange;
        pt.vhi *= modrange;
      }
    } else {
      this.modrange = modrange / 256;
    }
    
    if (lforate && lfodepth) {
      const frequency = 256000 / (lforate * this.player.tempo);
      lfodepth /= 0xff;
      lfophase /= 0xff;
      lfodepth *= 0.5;
      lfophase *= Math.PI * 2;
      this.lfobase = 1.0 - lfodepth; // I can't seem to get a DC offset out of PeriodicWave, but we can fake it at the receiving node.
      const real = new Float32Array([0, Math.sin(lfophase)*lfodepth]);
      const imag = new Float32Array([0, Math.cos(lfophase)*lfodepth]);
      const wave = new PeriodicWave(this.player.ctx, { real, imag, disableNormalization: true });
      this.lfo = new OscillatorNode(this.player.ctx, { shape: "custom", periodicWave: wave, frequency });
      this.lfo.start();
    }

    this.osc = (a,b,c,d) => this.oscillateFm(a, b, c, d);
  }
  
  initHarsh(src) {
    const decoder = new EauDecoder(src);
    let shape = decoder.u8(0);
    this.levelenv = decoder.env("level");
    this.pitchenv = decoder.env("pitch");
    this.wheelrange = decoder.u16(200);
    
    this.wheeli = 0;
    this.wheelCents = 0;
    this.tunable = []; // {node,until:seconds}
    
    switch (shape) {
      case 1: this.shape = "square"; break;
      case 2: this.shape = "sawtooth"; break;
      case 3: this.shape = "triangle"; break;
      default: this.shape = "sine";
    }
    this.osc = (a,b,c,d) => this.oscillateHarsh(a,b,c,d);
  }
  
  initHarm(src) {
    const decoder = new EauDecoder(src);
    let harmc = decoder.u8(0);
    let imag;
    if (harmc) {
      imag = new Float32Array(1 + harmc);
      for (let i=1; i<=harmc; i++) imag[i] = decoder.u16(0) / 0xffff;
    } else {
      imag = new Float32Array([0, 1]);
    }
    this.wave = new PeriodicWave(this.player.ctx, {
      real: new Float32Array(imag.length),
      imag,
      disableNormalization: true,
    });
    this.levelenv = decoder.env("level");
    this.pitchenv = decoder.env("pitch");
    this.wheelrange = decoder.u16(200);
    
    this.wheeli = 0;
    this.wheelCents = 0;
    this.tunable = []; // {node,until:seconds}
    
    this.osc = (a,b,c,d) => this.oscillateHarm(a,b,c,d);
  }
   
  tunedNote(when, noteid, velocity, durs) {
    const osc = this.osc(when, eauNotev[noteid & 0x7f], velocity, durs);
    if (!osc) return;
    const env = new GainNode(this.player.ctx, { gain: 0 });
    const pts = eauEnvApply(this.levelenv, when, velocity, durs);
    env.gain.setValueAtTime(pts[0].v, when);
    for (const pt of pts) env.gain.linearRampToValueAtTime(pt.v, pt.t);
    osc.connect(env);
    env.connect(this.postStart);
    this.player.droppables.push({ node: osc });
    this.player.droppables.push({ node: env });
    const endTime = pts[pts.length - 1].t;
    for (const d of this.player.droppables) if (!d.time) {
      d.time = endTime;
      this.addTunable(d.node, endTime);
    }
  }
  
  tunedWheel(when, v) {
    if (v === this.wheeli) return;
    this.wheeli = v;
    if (this.wheelrange < 1) return;
    this.wheelCents = (v * this.wheelrange) / 512;
    for (let i=this.tunable.length; i-->0; ) {
      if (this.tunable[i].until < this.player.ctx.currentTime) {
        this.tunable.splice(i, 1);
      } else {
        this.tunable[i].node.detune.value = this.wheelCents;
      }
    }
  }
  
  addTunable(node, until) {
    if (!this.wheelrange) return;
    if (until <= this.player.ctx.currentTime) return;
    if (!node.detune) return;
    this.tunable.push({ node, until });
    for (let i=this.tunable.length; i-->0; ) {
      if (this.tunable[i].until < this.player.ctx.currentTime) {
        this.tunable.splice(i, 1);
      }
    }
  }
  
  oscillateFm(when, hz, velocity, durs) {
    
    const car = new OscillatorNode(this.player.ctx, { frequency: hz });
    car.detune.value = this.wheelCents;
    car.start(when);
    
    let mhz = this.modabs ? this.modrate : (this.modrate * hz);
    const mod = new OscillatorNode(this.player.ctx, { frequency: mhz });
    mod.start(when);
    
    const mscale = new GainNode(this.player.ctx, { gain: 0 });
    if (this.rangeenv.flags & 0x80) {
      mscale.gain.setValueAtTime(this.modrange * hz, 0);
    } else {
      const pts = eauEnvApply(this.rangeenv, when, velocity, durs);
      mscale.gain.setValueAtTime(pts[0].v, when);
      for (const pt of pts) mscale.gain.linearRampToValueAtTime(pt.v * hz, pt.t);
    }
    mod.connect(mscale);
    
    if (this.lfo) {
      const lfoscale = new GainNode(this.player.ctx, { gain: this.lfobase });
      this.lfo.connect(lfoscale.gain);
      mscale.connect(lfoscale);
      lfoscale.connect(car.frequency);
    } else {
      mscale.connect(car.frequency);
    }
    
    if (!(this.pitchenv.flags & 0x80)) {
      const pts = eauEnvApply(this.pitchenv, when, velocity, durs);
      car.detune.setValueAtTime(pts[0].v, 0);
      for (const pt of pts) car.detune.linearRampToValueAtTime(pt.v, pt.t);
    }
    
    this.player.droppables.push({ node: mod });
    return car;
  }
  
  oscillateHarsh(when, hz, velocity, durs) {
    const osc = new OscillatorNode(this.player.ctx, { frequency: hz, shape: this.shape, detune: this.wheelCents });
    osc.start(when);
    
    if (!(this.pitchenv.flags & 0x80)) {
      const pts = eauEnvApply(this.pitchenv, when, velocity, durs);
      osc.detune.setValueAtTime(pts[0].v, 0);
      for (const pt of pts) osc.detune.linearRampToValueAtTime(pt.v, pt.t);
    }
    
    return osc;
  }
  
  oscillateHarm(when, hz, velocity, durs) {
    const osc = new OscillatorNode(this.player.ctx, { frequency: hz, shape: "custom", periodicWave: this.wave, detune: this.wheelCents });
    osc.start(when);
    
    if (!(this.pitchenv.flags & 0x80)) {
      const pts = eauEnvApply(this.pitchenv, when, velocity, durs);
      osc.detune.setValueAtTime(pts[0].v, 0);
      for (const pt of pts) osc.detune.linearRampToValueAtTime(pt.v, pt.t);
    }
    
    return osc;
  }
}
