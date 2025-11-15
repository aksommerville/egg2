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
    "constructor() {" +
      "super();" +
      "console.log(`EggAudio.ctor`);" +
      "this.memory = new WebAssembly.Memory({ initial: 100, maximum: 1000 });" + /* TODO Maximum memory size, can we get smarter about it? */
      "this.buffers = [];" + /* Float32Array */
      "this.bufferSize = 128;" + /* frames */
      "this.port.onmessage = m => {" +
        "console.log(`EggAudio message`, m);" +
        "switch (m.data.cmd) {" +
          "case 'init': this.init(m.data); break;" +
          "case 'reinit': this.reinit(m.data); break;" +
          "case 'playSong': this.playSong(m.data); break;" +
          /*TODO playSong, playSound, etc */
        "}" +
      "};" +
    "}" +
    
    "init(m) {" +
      "console.log(`EggAudio.init`, m);" +
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
        "console.log(`EggAudio.init ok`);" +
      "}).catch(e => {" +
        "console.error(e);" +
      "});" +
    "}" +
    
    "reinit(m) {" +
      "console.log(`EggAudio.reinit`, m);" +
      "if (!this.instance) throw new Error(`EggAudio not initialized`);" +
      "this.transferRom(m.rom);" +
    "}" +
    
    "transferRom(rom) {" +
      "if (rom instanceof ArrayBuffer) rom = new Uint8Array(rom);" +
      "console.log(`TODO EggAudio.transferRom`, rom);" +
      "if (rom) {" +
        "const dstp = this.instance.exports.synth_get_rom(rom.byteLength);" +
        "console.log(`transferRom`, { dstp, rom, bl:rom.byteLength, memory: this.memory });" +
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
      "console.log(`EggAudio.playSong songid=${m.songid} rid=${m.rid} repeat=${m.repeat} trim=${m.trim} pan=${m.pan}`);" +
      "this.instance.exports.synth_play_song(m.songid, m.rid, m.repeat, m.trim, m.pan);" +
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
    console.log(`Audio.ctor`);
    this.initStatus = 0; // <0=error, >0=ready
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
      this.node = new AudioWorkletNode(this.ctx, "AWP");
      this.node.connect(this.ctx.destination);
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
    console.log(`Audio.pause`);
    if (!this.ctx) return;
    this.ctx.suspend();
  }
  
  resume() {
    console.log(`Audio.resume`);
    if (!this.ctx) return;
    this.ctx.resume();
  }
  
  start() {
    console.log(`Audio.start`);
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
    console.log(`Audio.stop`);
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
    console.log(`Audio.playEauSong`, { serial, songid, repeat });
    if (!this.node) return;
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
    /*TODO
    let playhead = 0;
    if (this.song) {
      if (songid && (songid === this.song.id)) playhead = this.song.getPlayhead();
      this.song.stop();
      this.song = null;
    }
    if (serial) {
      if (!this.ctx) this.start();
      //this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid, this.noise);
      this.song.play();
      if (playhead > 0) this.song.setPlayhead(playhead);
      this.update(); // Editor updates on a long period; ensure we get one initial priming update.
    }
    */
  }
  
  playSoundBuffer(buffer, trim, pan) {
    /*TODO
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
    if (!this.soundEnabled) return;
    
    const buffer = this.sounds[soundid];
    if (buffer) {
      if (buffer === "pending") return; // Drop it; the printer will also play it, once ready.
      return this.playSoundBuffer(buffer, trim, pan);
    }
    this.sounds[soundid] = "pending";
    
    const serial = this.rt.rom.getRes(EGG_TID_sound, soundid);
    if (!serial) return;
    /*TODO
    const durms = Math.min(5000, Math.max(1, SongPlayer.calculateDuration(serial)));
    const framec = Math.ceil((durms * this.ctx.sampleRate) / 1000);
    const ctx = new OfflineAudioContext(1, framec, this.ctx.sampleRate);
    const song = new SongPlayer(ctx, serial, 1.0, 0.0, false, 0, this.noise);
    song.play();
    song.update(5.0);
    ctx.startRendering().then(buffer => {
      this.sounds[soundid] = buffer;
      this.playSoundBuffer(buffer, trim, pan);
    });
    /**/
  }
  
  egg_play_song(songid, force, repeat) {
    if (!this.ctx) return;
    if (!this.musicEnabled) return;
    if (!force && (songid === this.song?.id)) return;
    this.songParams = [songid, force, repeat, 0]; // To restore when prefs change.
    const serial = this.rt.rom.getRes(EGG_TID_song, songid);
    if (!serial) songid = 0;
    if (this.song) {
      if (this.pvsong) this.pvsong.stop();
      this.pvsong = this.song;
      this.song.stopSoon();
      this.song = null;
    }
    if (!serial) return;
    //this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid, this.noise);
    this.song.play();
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
    if (this.song) return this.song.getPlayhead();
    return 0.0;
  }
  
  egg_song_set_playhead(ph) {
    if (this.song) this.song.setPlayhead(ph);
  }
}

Audio.singleton = true; // Not required by Egg Runtime, but necessary for the editor.
