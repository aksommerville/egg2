/* Audio.js
 * JS interface to Egg's synthesizer.
 * The actual synth stuff lives in src/opt/synth, it's all written in C.
 * This Audio class coordinates an AudioContext, AudioWorkletNode, and WebAssembly context to glue that together.
 * We implement the audio portion of the Egg Platform API, and some extras for editor support.
 */
 
import { EGG_TID_song, EGG_TID_sound } from "./Rom.js";

/* Our AudioWorkletProcessor.
 * This is plain text that we trickfully load as a worklet.
 ********************************************************************************/
 
const wsrc = 
  "class EggAudio extends AudioWorkletProcessor {" +
    "constructor(args) {" +
      "super(args);" +
      "this.memory = new WebAssembly.Memory({ initial: 100, maximum: 2000 });" + /* TODO Maximum memory size, can we get smarter about it? */
      "this.buffers = [];" + /* Float32Array */
      "this.bufferSize = 128;" + /* frames */
      "this.deferredSongs = [];" + /* m.data */
      "this.port.onmessage = m => {" +
        "switch (m.data.cmd) {" +
          "case 'init': this.init(m.data); break;" +
          "case 'reinit': this.reinit(m.data); break;" +
          "case 'playSong': this.playSong(m.data, true); break;" +
          "case 'playSound': this.playSound(m.data); break;" +
          "case 'setPlayhead': this.setPlayhead(m.data); break;" +
          "case 'printWave': this.printWave(m.data); break;" +
          "case 'set': this.setProp(m.data); break;" +
          "case 'noteOn': this.noteOn(m.data); break;" +
          "case 'noteOff': this.noteOff(m.data); break;" +
          "case 'noteOnce': this.noteOnce(m.data); break;" +
          "case 'wheel': this.wheel(m.data); break;" +
        "}" +
      "};" +
    "}" +
    
    "init(m) {" +
      "if (this.instance) throw new Error(`EggAudio multiple instantiation.`);" +
      "this.rate = m.r;" +
      "this.chanc = m.c;" +
      "WebAssembly.instantiate(m.wasm, {" +
        "env: {" +
          "memory: this.memory," +
          "logint: n => console.log(n)," + /*XXX*/
        "}," +
      "}).then(rsp => {" +
        "this.instance = rsp.instance;" +
        "if (this.instance.exports.synth_init(this.rate, this.chanc, this.bufferSize) < 0) {" +
          "throw new Error(`synth_init failed`);" +
        "}" +
        "this.transferRom(m.rom);" +
        "this.acquireBuffers();" +
        "for (const req of this.deferredSongs) this.playSong(req, false);" +
        "this.deferredSongs = [];" +
      "}).catch(e => {" +
        "console.error(e);" +
      "});" +
    "}" +
    
    "reinit(m) {" +
      "if (!this.instance) return;" +
      "this.transferRom(m.rom);" +
    "}" +
    
    "transferRom(rom) {" +
      "if (rom instanceof ArrayBuffer) rom = new Uint8Array(rom);" +
      "if (rom) {" +
        "const dstp = this.instance.exports.synth_get_rom(rom.byteLength);" +
        "const dst = new Uint8Array(this.memory.buffer, dstp, rom.byteLength);" +
        "dst.set(rom);" +
      "} else {" +
        "this.instance.exports.synth_get_rom(0);" +
      "}" +
    "}" +
  
    "acquireBuffers() {" +
      "for (let c=0; c<this.chanc; c++) {" +
        "const p = this.instance.exports.synth_get_buffer(c);" +
        "if (!p) throw new Error(`Buffer for channel ${c}/${this.chanc} was not provided.`);" +
        "this.buffers[c] = new Float32Array(this.memory.buffer, p, this.bufferSize);" +
      "}" +
    "}" +
    
    "playSong(m, deferrable) {" +
      "if (!this.instance) {" +
        "if (deferrable) this.deferredSongs.push(m);" +
        "return;" +
      "}" +
      "this.instance.exports.synth_play_song(m.songid, m.rid, m.repeat, m.trim, m.pan);" +
    "}" +
    
    "playSound(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_play_sound(m.rid, m.trim, m.pan);" +
    "}" +
    
    "setProp(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_set(m.songid, m.chid, m.prop, m.v);" +
    "}" +
    
    "noteOn(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_event_note_on(m.songid, m.chid, m.noteid, m.velocity);" +
    "}" +
    
    "noteOff(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_event_note_off(m.songid, m.chid, m.noteid);" +
    "}" +
    
    "noteOnce(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_event_note_once(m.songid, m.chid, m.noteid, m.velocity, m.durms);" +
    "}" +
    
    "wheel(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_set(m.songid, m.chid, 6, m.v / 8192);" + // PROP 6 = WHEEL
    "}" +
    
    "setPlayhead(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_set(m.songid, 0xff, 3, m.ph);" + /* 3=SYNTH_PROP_PLAYHEAD */
    "}" +
    
    "printWave(m) {" +
      "const serialp = this.instance.exports.synth_wave_prepare(m.serial.length);" +
      "new Uint8Array(this.memory.buffer, serialp, m.serial.length).set(m.serial);" +
      "const pcmp = this.instance.exports.synth_wave_preview();" +
      "const pcm = new Float32Array(this.memory.buffer, pcmp, 1024);" +
      "this.port.postMessage({ cmd: 'wavePreview', pcm, cookie: m.cookie });" +
    "}" +
    
    "process(i, o, p) {" +
      "if (!this.instance) return true;" +
      "const output = o[0];" +
      "if (output[0].length !== this.bufferSize) throw new Error(`got ${output[0].length}-frame buffer from WebAudio, expected ${this.bufferSize}`);" +
      "this.instance.exports.synth_update(this.bufferSize);" +
      "for (let c=0; c<output.length; c++) {" +
        "output[c].set(this.buffers[c]);" +
      "}" +
      "return true;" +
    "}" +
  "}" +
  "registerProcessor('AWP', EggAudio);" +
"";
 
/* Audio: The main-thread audio interface.
 *********************************************************************************/
 
export class Audio {
  constructor(rt) {
    this.rt = rt; // Will be undefined when loaded in editor.
    this.ctx = null; // AudioContext
    this.ready = false; // True after all the things are instantiated and we've sent the "init" message.
    this.initStatus = 0; // <0=error, >0=ready
    this.pendingWavePrints = []; // {cookie,promise,resolve,reject}
    this.nextWaveCookie = 1;
    this.musicTrim = 99; // 0..99 exactly as in Egg prefs (those read from here blindly).
    this.soundTrim = 99;
    this.songsBySongid = []; // Sparse, indexed by songid. {rid,startTime,duration,repeat}
    this.durationsByRid = []; // Sparse, indexed by rid. Number, seconds.
    this.initPromise = this.init()
      .then(() => { this.initStatus = 1; })
      .catch(e => { console.error(e); this.initStatus = -1; });
  }
  
  init() {
    this.ready = false;
    if (!window.AudioContext) return Promise.reject(`AudioContext not defined`);
    const js = new Blob([wsrc], { type: "text/javascript" });
    const url = URL.createObjectURL(js);
    this.ctx = new AudioContext({ latencyHint: "interactive" });
    if (!this.ctx.audioWorklet) return Promise.reject(`AudioWorklet not defined`);
    let resolve, reject;
    const promise = new Promise((res, rej) => {
      resolve = res;
      reject = rej;
    }).then(() => {
      this.node = new AudioWorkletNode(this.ctx, "AWP", {
        numberOfInputs: 0,
        numberOfOutputs: 1,
        outputChannelCount: [this.ctx.destination.channelCount],
      });
      this.node.connect(this.ctx.destination);
      this.node.port.onmessage = e => this.onWorkerMessage(e.data);
      return this.acquireWasm();
    }).then(wasm => {
      this.node.port.postMessage({ cmd: "init", rom: this.sliceRom(this.rt?.rom?.serial), wasm, r: this.ctx.sampleRate, c: this.ctx.destination.channelCount });
      this.ready = true;
    });
    this.ctx.audioWorklet.addModule(url).then(() => {
      URL.revokeObjectURL(url);
      if (this.ctx.state === "suspended") {
        window.addEventListener("click", () => {
          this.ctx.resume().then(resolve);
        }, { once: true });
      } else {
        resolve();
      }
    }).catch(e => {
      URL.revokeObjectURL(url);
      reject(e);
    });
    return promise;
  }
  
  /* Given a full Egg ROM, return just the song and sound bits.
   * Tid (5,6), deliberately assigned next to each other.
   */
  sliceRom(src) {
    if (!src || (src.length < 4)) return src;
    if ((src[0] !== 0x00) || (src[1] !== 0x45) || (src[2] !== 0x52) || (src[3] !== 0x4d)) return src;
    let startp=4, stopp=src.length, startTid=0;
    for (let srcp=4, tid=1; srcp<src.length; ) {
      const lead = src[srcp++];
      if (!lead) break;
      switch (lead & 0xc0) {
        case 0x00: { // TID
            const nextTid = tid + lead;
            if (!startTid && (nextTid >= 5)) {
              startp = srcp;
              startTid = nextTid;
            }
            if (nextTid > 6) {
              stopp = srcp - 1;
              srcp = src.length;
            }
            tid = nextTid;
          } break;
        case 0x40: srcp++; break; // RID, skip it.
        case 0x80: { // RES, skip it.
            const len = (((lead & 0x3f) << 16) | (src[srcp] << 8) | src[srcp+1]) + 1;
            srcp += 2;
            srcp += len;
          } break;
        case 0xc0: return src; // Illegal. Abort, return the input.
      }
    }
    if (!startTid) return new Uint8Array(0); // Didn't find anything. Give them an empty ROM.
    // If the first tid is 6, ie there's no songs, we need to insert a tid advancement.
    if (startTid === 6) {
      const len = stopp - startp;
      const dst = new Uint8Array(1 + len);
      dst[0] = 0x01; // tid+1
      new Uint8Array(dst.buffer, dst.byteOffset + 1, len).set(new Uint8Array(src.buffer, src.byteOffset + startp, stopp - startp));
      return dst;
    }
    return src.slice(startp, stopp);
  }
  
  acquireWasm() {
    return fetch("./synth.wasm").then(rsp => {
      if (!rsp.ok) throw rsp;
      return rsp.arrayBuffer();
    });
  }
  
  pause() {
    if (!this.ctx) return;
    this.ctx.suspend();
  }
  
  resume() {
    if (!this.ctx) return;
    this.ctx.resume();
  }
  
  start() {
    if (this.initStatus < 0) return;
    if (this.initStatus > 0) this.startNow();
    else this.initPromise.then(() => this.startNow());
  }
  
  startNow() {
    if (this.ctx.state === "suspended") {
      this.ctx.resume();
    }
    this.ready = true;
  }
  
  stop() {
    //TODO Tell node to stop, if it exists.
    if (this.ctx) {
      this.ctx.suspend();
    }
    this.ready = false;
  }
  
  update() {
    // No longer needed. But keep the plumbing in place just in case.
  }
  
  /* For editor.
   */
  playEauSong(serial, repeat) {
    if (serial instanceof ArrayBuffer) serial = new Uint8Array(serial);
    if (serial && (serial.length > 0x3fffff)) return;
    if (!this.ready) return;
    let rom = null;
    if (serial) {
      rom = new Uint8Array(serial.length + 3);
      rom[0] = 0x80 | ((serial.length - 1) >> 16);
      rom[1] = (serial.length - 1) >> 8;
      rom[2] = (serial.length - 1);
      new Uint8Array(rom.buffer, rom.byteOffset + 3, serial.length).set(serial);
    }
    this.node.port.postMessage({ cmd: "reinit", rom });
    this.node.port.postMessage({ cmd: "playSong", songid: 1, rid: 1, repeat, trim: 1, pan: 0 });
    this.songsBySongid[1] = {
      rid: 1,
      startTime: this.ctx.currentTime,
      duration: serial ? this.calculateSongDuration(serial) : 0,
      repeat,
    };
  }
  
  /* For editor.
   * (event) is what editor's MidiService produces: { opcode, chid, a, b }
   */
  sendEvent(event) {
    if (!this.node) return;
    switch (event.opcode) {
      case 0x80: this.node.port.postMessage({ cmd: "noteOff", songid: 1, chid: event.chid, noteid: event.a, velocity: event.b }); break;
      case 0x90: this.node.port.postMessage({ cmd: "noteOn", songid: 1, chid: event.chid, noteid: event.a, velocity: event.b }); break;
      case 0xe0: this.node.port.postMessage({ cmd: "wheel", songid: 1, chid: event.chid, v: (event.a | (event.b << 7)) - 8192 }); break;
    }
  }
  
  /* (serial) is a Uint8Array containing an EAU file.
   * Returns sum of delays in seconds, but never zero.
   */
  calculateSongDuration(serial) {
    let total = 0;
    if (serial.length >= 10) {
      const chdrlen = (serial[6] << 24) | (serial[7] << 16) | (serial[8] << 8) | serial[9];
      let srcp = 10;
      if ((chdrlen >= 0) && (srcp <= serial.length - chdrlen - 4)) {
        srcp += chdrlen;
        const evtlen = (serial[srcp] << 24) | (serial[srcp+1] << 16) | (serial[srcp+2] << 8) | serial[srcp+3];
        srcp += 4;
        if ((evtlen >= 0) && (srcp <= serial.length - evtlen)) {
          const stopp = srcp + evtlen;
          while (srcp < stopp) {
            const lead = serial[srcp++];
            if (!(lead & 0x80)) {
              if (lead & 0x40) total += ((lead & 0x3f) + 1) << 6;
              else total += lead;
            } else switch (lead & 0xf0) {
              case 0x80: srcp += 2; break;
              case 0x90: srcp += 2; break;
              case 0xa0: srcp += 3; break;
              case 0xe0: srcp += 2; break;
              case 0xf0: break;
              default: srcp = stopp; // misencoded, stop reading
            }
          }
        }
      }
    }
    if (total < 1) total = 1;
    return total / 1000;
  }
  
  durationByRid(rid) {
    let s = this.durationsByRid[rid];
    if (typeof(s) === "number") return s;
    const serial = this.rt.rom.getRes(5, rid);
    if (serial) {
      s = this.calculateSongDuration(serial);
    } else {
      s = 0.0;
    }
    this.durationsByRid[rid] = s;
    return s;
  }
  
  printWave(serial) {
    const cookie = this.nextWaveCookie++;
    let resolve, reject;
    const promise = new Promise((res, rej) => { resolve = res; reject = rej; });
    this.pendingWavePrints.push({ cookie, promise, resolve, reject });
    this.node.port.postMessage({ cmd: "printWave", serial, cookie });
    return promise;
  }
  
  onWorkerMessage(data) {
    switch (data.cmd) {
      case "wavePreview": {
          const p = this.pendingWavePrints.findIndex(w => w.cookie === data.cookie);
          if (p < 0) break;
          const pend = this.pendingWavePrints[p];
          this.pendingWavePrints.splice(p, 1);
          pend.resolve(data.pcm);
        } break;
    }
  }
  
  getNormalizedPlayhead() {
    if (!this.ctx) return 0.0;
    const song = this.songsBySongid[1];
    if (song?.duration) {
      let elapsed = this.ctx.currentTime - song.startTime;
      if (song.repeat) elapsed %= song.duration;
      else if (elapsed >= song.duration) return 0.0;
      if (elapsed <= 0.0) return 0.001;
      return elapsed / song.duration;
    }
    return 0.0;
  }
  
  setNormalizedPlayhead(p) {
    if (!this.ctx) return;
    const song = this.songsBySongid[1];
    if (song?.duration) {
      const ph = p * song.duration;
      this.node.port.postMessage({ cmd: "setPlayhead", songid: 1, ph });
      song.startTime = this.ctx.currentTime - ph;
    }
  }
  
  /* Egg Platform API.
   ********************************************************************************/
   
  setMusicTrim(trim) {
    if (isNaN(trim) || (trim < 0) || (trim > 99)) return;
    if (trim === this.musicTrim) return;
    this.musicTrim = trim;
    this.node.port.postMessage({ cmd: "set", songid: 0, chid: 0, prop: 7, v: trim / 99 });
  }
  
  setSoundTrim(trim) {
    if (isNaN(trim) || (trim < 0) || (trim > 99)) return;
    if (trim === this.soundTrim) return;
    this.soundTrim = trim;
    this.node.port.postMessage({ cmd: "set", songid: 0, chid: 0, prop: 8, v: trim / 99 });
  }
  
  egg_play_sound(rid, trim, pan) {
    if (rid < 1) return;
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "playSound", rid, trim, pan });
  }
  
  egg_play_song(songid, rid, repeat, trim, pan) {
    if (songid < 1)return;
    if (!this.ready) {
      this.initPromise.then(() => {
        if (this.ready) this.egg_play_song(songid, rid, repeat, trim, pan);
      });
      return;
    }
    this.node.port.postMessage({ cmd: "playSong", songid, rid, repeat, trim, pan });
    this.songsBySongid[songid] = { rid, startTime: this.ctx.currentTime, repeat };
  }
  
  egg_song_set(songid, chid, prop, v) {
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "set", songid, chid, prop, v });
    if (prop === 3) { // PLAYHEAD
      const song = this.songsBySongid[songid];
      if (song) {
        song.startTime = v - this.ctx.currentTime;
      }
    }
  }
  
  egg_song_event_note_on(songid, chid, noteid, velocity) {
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "noteOn", songid, chid, noteid, velocity });
  }
  
  egg_song_event_note_off(songid, chid, noteid) {
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "noteOff", songid, chid, noteid });
  }
  
  egg_song_event_note_once(songid, chid, noteid, velocity, durms) {
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "noteOnce", songid, chid, noteid, velocity, durms });
  }
  
  egg_song_event_wheel(songid, chid, v) {
    if (!this.ready) return;
    this.node.port.postMessage({ cmd: "wheel", songid, chid, v });
  }
  
  egg_song_get_playhead(songid) {
    const song = this.songsBySongid[songid];
    if (song) {
      if (!song.duration) song.duration = this.durationByRid(song.rid);
      let elapsed = this.ctx.currentTime - song.startTime;
      if (song.repeat) elapsed %= song.duration;
      else if (elapsed >= song.duration) return 0.0;
      if (elapsed <= 0.0) return 0.001;
      return elapsed;
    }
    return 0.0;
  }
}

Audio.singleton = true; // Not required by Egg Runtime, but necessary for the editor.
