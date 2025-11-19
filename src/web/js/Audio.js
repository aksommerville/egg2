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
      "this.memory = new WebAssembly.Memory({ initial: 100, maximum: 1000 });" + /* TODO Maximum memory size, can we get smarter about it? */
      "this.buffers = [];" + /* Float32Array */
      "this.bufferSize = 128;" + /* frames */
      "this.port.onmessage = m => {" +
        "switch (m.data.cmd) {" +
          "case 'init': this.init(m.data); break;" +
          "case 'reinit': this.reinit(m.data); break;" +
          "case 'playSong': this.playSong(m.data); break;" +
          "case 'playSound': this.playSound(m.data); break;" +
          "case 'setPlayhead': this.setPlayhead(m.data); break;" +
          "case 'printWave': this.printWave(m.data); break;" +
          /*TODO playSong, playSound, etc */
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
    
    "playSong(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_play_song(m.songid, m.rid, m.repeat, m.trim, m.pan);" +
    "}" +
    
    "playSound(m) {" +
      "if (!this.instance) return;" +
      "this.instance.exports.synth_play_sound(m.rid, m.trim, m.pan);" +
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
    this.initStatus = 0; // <0=error, >0=ready
    this.songStartTime = 0; // AudioContext.currentTime when song starts, approximate at best.
    this.songPlaying = false; // May need to abstract these song things to run multiple. Getting it working for one first.
    this.songDuration = 0;
    this.pendingWavePrints = []; // {cookie,promise,resolve,reject}
    this.nextWaveCookie = 1;
    this.initPromise = this.init()
      .then(() => { this.initStatus = 1; })
      .catch(e => { console.error(e); this.initStatus = -1; });
  }
  
  init() {
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
      this.node.port.postMessage({ cmd: "init", rom: this.rt?.rom?.serial, wasm, r: this.ctx.sampleRate, c: this.ctx.destination.channelCount });
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
  }
  
  stop() {
    //TODO Tell node to stop, if it exists.
    if (this.ctx) {
      this.ctx.suspend();
    }
  }
  
  update() {//XXX no longer needed
  }
  
  /* For editor.
   * If (songid) nonzero and matches the current song, we'll start at the prior playhead.
   */
  playEauSong(serial, songid, repeat) {
    if (serial instanceof ArrayBuffer) serial = new Uint8Array(serial);
    if (serial && (serial.length > 0x3fffff)) return;
    if (!this.node) return;
    let rom = null;
    if (serial) {
      rom = new Uint8Array(serial.length + 3);
      rom[0] = 0x80 | ((serial.length - 1) >> 16);
      rom[1] = (serial.length - 1) >> 8;
      rom[2] = (serial.length - 1);
      new Uint8Array(rom.buffer, rom.byteOffset + 3, serial.length).set(serial);
      this.songPlaying = true;
      this.songDuration = this.calculateSongDuration(serial);
    } else {
      this.songPlaying = false;
      this.songDuration = 0;
    }
    this.node.port.postMessage({ cmd: "reinit", rom });
    this.node.port.postMessage({ cmd: "playSong", songid: 1, rid: 1, repeat, trim: 1, pan: 0 });
    this.songStartTime = this.ctx.currentTime;
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
  
  playSoundBuffer(buffer, trim, pan) {
    /*TODO Do we still need this? I think we can eliminate the compare-the-synthesizers modal.
    if (!this.ctx) return;
    if (this.ctx.state === "suspended") return;
    if (trim <= 0) return;
    const node = new AudioBufferSourceNode(this.ctx, { buffer });
    let tail = node;
    if (trim < 1) {
      const gain = new GainNode(this.ctx, { gain: trim });
      tail.connect(gain);
      tail = gain;
    }
    if (pan) {
      const panner = new StereoPannerNode(this.ctx, { pan });
      tail.connect(panner);
      tail = panner;
    }
    tail.connect(this.ctx.destination);
    const endTime = this.ctx.currentTime + buffer.duration + 0.010;
    this.soundPlayers.push({ node, endTime });
    if (tail !== node) this.soundPlayers.push({ node: tail, endTime });
    node.start();
    */
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
  
  /* Egg Platform API.
   ********************************************************************************/
   
  enableMusic(enable) {
    if (enable) {
      if (this.musicEnabled) return;
      this.musicEnabled = true;
      if (this.songParams) {
        const ph = this.songParams[3];
        this.egg_play_song(this.songParams[0], this.songParams[1], this.songParams[2]);
        if (this.song) this.song.setPlayhead(ph);
      }
    } else {
      if (!this.musicEnabled) return;
      this.musicEnabled = false;
      if (this.song) {
        if (this.songParams) this.songParams[3] = this.song.getPlayhead();
        if (this.pvsong) this.pvsong.stop();
        this.pvsong = this.song;
        this.song.stopSoon();
        this.song = null;
      }
    }
  }
  
  enableSound(enable) {
    if (enable) {
      if (this.soundEnabled) return;
      this.soundEnabled = true;
    } else {
      if (!this.soundEnabled) return;
      this.soundEnabled = false;
      for (const sound of this.soundPlayers) {
        sound.node.stop?.();
        sound.node.disconnect();
      }
      this.soundPlayers = [];
    }
  }
   
  egg_play_sound(soundid, trim, pan) {
    if (!this.ctx) return;
    this.node.port.postMessage({ cmd: "playSound", rid: soundid, trim, pan });
  }
  
  egg_play_song(songid, force, repeat) {
    if (!this.ctx) return;
    if (!force && (songid === this.song?.id)) return;
    this.songParams = [songid, force, repeat, 0]; // To restore when prefs change.
    this.node.port.postMessage({ cmd: "playSong", songid: 1, rid: songid, repeat, trim: 1, pan: 0 });
    this.songStartTime = this.ctx.currentTime;
  }
  
  egg_play_note(chid, noteid, velocity, durms) {
    if (!this.song) return 0;
    return this.song.playNote(chid, noteid, velocity, durms);
  }
  
  egg_release_note(holdid) {
    if (!this.song) return;
    this.song.releaseNote(holdid);
  }
  
  egg_adjust_wheel(chid, v) {
    if (!this.song) return;
    this.song.adjustWheel(chid, v);
  }
  
  egg_song_get_id() {
    return this.song?.id || 0;
  }
  
  egg_song_get_playhead() {
    if (this.ctx && this.songPlaying) {
      return (this.ctx.currentTime - this.songStartTime) % this.songDuration;
    }
    return 0.0;
  }
  
  getNormalizedPlayhead() {
    if (this.ctx && this.songPlaying) {
      return ((this.ctx.currentTime - this.songStartTime) % this.songDuration) / this.songDuration;
    }
    return 0.0;
  }
  
  egg_song_set_playhead(ph) {
    this.node.port.postMessage({ cmd: "setPlayhead", songid: 1, ph });
    this.songStartTime = this.ctx.currentTime - ph;
  }
  
  setNormalizedPlayhead(p) {
    if (this.ctx && this.songPlaying) {
      const ph = p * this.songDuration;
      this.node.port.postMessage({ cmd: "setPlayhead", songid: 1, ph });
      this.songStartTime = this.ctx.currentTime - ph;
    }
  }
}

Audio.singleton = true; // Not required by Egg Runtime, but necessary for the editor.
