/* AudioCompareModal.js
 * Not part of SongEditor, we are triggered by a global action.
 * Let the user play songs and sounds with both native and web synth, for comparison purposes.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Comm } from "../Comm.js";
import { SongPlayer } from "../SongPlayer.js"; // src/web; not editor
import { eauNotevRequire } from "../songBits.js"; // ''

export class AudioCompareModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data, Comm, Window, "nonce"];
  }
  constructor(element, dom, data, comm, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.comm = comm;
    this.window = window;
    this.nonce = nonce;
    
    this.nativeBuffer = null; // AudioBuffer
    this.webBuffer = null; // AudioBuffer
    this.ctx = null; // AudioContext
    this.renderTimeout = null;
    this.overlayInterval = null;
    this.startTime = 0; // (this.ctx.currentTime) at the start of the current playback
    this.playhead = 0; // PCM playhead in seconds, updates repeatedly while playing
    this.playheadLimit = 0; // Duration of current buffer
    this.node = null; // AudioBufferSourceNode while playing
    this.nativePrintTime = 0;
    this.webPrintTime = 0;
    
    eauNotevRequire();
    
    const row = this.dom.spawn(this.element, "DIV", ["songSelect"]);
    const typeSelect = this.dom.spawn(row, "SELECT", { name: "type", "on-change": e => this.onTypeChange() },
      ...Array.from(new Set(this.data.resv.map(r => r.type))).sort().map(type => {
        return this.dom.spawn(null, "OPTION", { value: type }, type);
      })
    );
    this.dom.spawn(row, "SELECT", { name: "rid", "on-change": e => this.onRidChange() });
    typeSelect.value = "song";
    this.onTypeChange();
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.overlayInterval) {
      this.window.clearInterval(this.overlayInterval);
      this.overlayInterval = null;
    }
    if (this.ctx) {
      try { this.ctx.suspend(); } catch (e) {}
      this.ctx = null;
    }
  }
  
  /* Gross states.
   ********************************************************************************************/
  
  showNone() {
    this.element.querySelector(".loading")?.remove();
    this.element.querySelector(".loaded")?.remove();
  }
  
  showInProgress() {
    this.showNone();
    this.dom.spawn(this.element, "DIV", ["loading"], "Printing song twice...");
  }
  
  showPrinted(native, web) {
    this.showNone();
    this.nativeBuffer = native;
    this.webBuffer = web;
    const container = this.dom.spawn(this.element, "DIV", ["loaded"]);
    const visual = this.dom.spawn(container, "DIV", ["visual"]);
    this.dom.spawn(visual, "CANVAS", ["pcm"]);
    this.dom.spawn(visual, "CANVAS", ["overlay"], { "on-click": e => this.onPlayheadClick(e) });
    
    const radios = this.dom.spawn(container, "DIV", ["radios"], { "on-change": () => this.onPcmSelectChanged() });
    const nativeRow = this.dom.spawn(radios, "DIV", ["row"]);
    const nativeToggle = this.dom.spawn(nativeRow, "INPUT", ["toggle"], { type: "radio", name: "pcmSelect", value: "native", id: `AudioCompareModal-${this.nonce}-pcmSelect-native` });
    this.dom.spawn(nativeRow, "LABEL", ["toggle"], { for: nativeToggle.id }, "Native");
    this.dom.spawn(nativeRow, "DIV", ["stats"], this.summarizeBuffer(native));
    const webRow = this.dom.spawn(radios, "DIV", ["row"]);
    this.dom.spawn(webRow, "INPUT", ["toggle"], { type: "radio", name: "pcmSelect", value: "web", id: `AudioCompareModal-${this.nonce}-pcmSelect-web` });
    this.dom.spawn(webRow, "LABEL", ["toggle"], { for: `AudioCompareModal-${this.nonce}-pcmSelect-web` }, "Web");
    this.dom.spawn(webRow, "DIV", ["stats"], this.summarizeBuffer(web));
    nativeToggle.checked = true;
    
    this.dom.spawn(container, "INPUT", { type: "button", value: "Play/Pause", "on-click": e => this.onPlayPause() });
    this.renderSoon();
  }
  
  summarizeBuffer(buffer) {
    let dst;
    switch (buffer.numberOfChannels) {
      case 1: dst = "mono"; break;
      case 2: dst = "stereo"; break;
      default: dst = `${buffer.numberOfChannels} channels`; break;
    }
    dst += `, ${buffer.length} frames`;
    const src = buffer.getChannelData(0);
    let sqsum=0, lo=src[0], hi=src[0];
    for (let i=src.length; i-->0; ) {
      sqsum += src[i] ** 2;
      if (src[i] < lo) lo = src[i];
      else if (src[i] > hi) hi = src[i];
    }
    const rms = Math.sqrt(sqsum / buffer.length);
    if (buffer.numberOfChannels === 1) dst += `, rms=${rms.toFixed(3)}`;
    else dst += `, rms(ch0)=${rms.toFixed(3)}`;
    const peak = Math.max(-lo, hi);
    dst += `, peak=${peak.toFixed(3)}`;
    if (buffer === this.nativeBuffer) dst += `, print=${this.nativePrintTime / 1000}`;
    else if (buffer === this.webBuffer) dst += `, print=${this.webPrintTime / 1000}`;
    return dst;
  }
  
  onTypeChange() {
    const type = this.element.querySelector("select[name='type']")?.value;
    if (!type) return;
    const ridSelect = this.element.querySelector("select[name='rid']");
    if (!ridSelect) return;
    ridSelect.innerHTML = "";
    this.dom.spawn(ridSelect, "OPTION", { value: "", disabled: true }, "Pick resource...");
    for (const res of this.data.resv.filter(res => res.type === type).sort((a, b) => a.rid - b.rid)) {
      this.dom.spawn(ridSelect, "OPTION", { value: res.path }, `${res.rid}: ${res.name}`);
    }
    ridSelect.value = "";
  }
  
  onRidChange() {
    const path = this.element.querySelector("select[name='rid']")?.value;
    const res = this.data.resv.find(r => r.path === path);
    if (!res) return;
    this.showInProgress();
    if (!this.ctx) {
      this.ctx = new AudioContext({
        sampleRate: 44100, // TODO configurable? For now, match native's default. Not that that's strictly necessary.
      });
    }
    this.eauFromSerial(res.serial).then((buffer) => {
      const serial = new Uint8Array(buffer);
      return Promise.all([
        this.printNative(serial),
        this.printWeb(serial),
      ]);
    }).then(([native, web]) => {
      this.showPrinted(native, web);
    }).catch(e => {
      this.dom.modalError(e);
      this.showNone();
    });
  }
  
  /* PCM acquisition.
   ***********************************************************************************/
  
  eauFromSerial(serial) {
    return this.comm.httpBinary("POST", "/api/convert", { dstfmt: "eau" }, null, serial);
  }
  
  printNative(serial) {
    //TODO Can we request that it print at (this.ctx.sampleRate)?
    this.nativePrintTime = 0;
    const startTime = Date.now();
    return this.comm.httpBinary("POST", "/api/convert", { dstfmt: "wav", srcfmt: "eau" }, null, serial)
      .then((wav) => this.audioBufferFromWav(wav))
      .then((buffer) => {
        this.nativePrintTime = Date.now() - startTime;
        return buffer;
      });
  }
  
  audioBufferFromWav(wav) {
    wav = new Uint8Array(wav);
    // We don't decode WAV generically; we expect the strict subset that eggdev produces.
    if (
      (wav[ 0] !== 0x52) ||
      (wav[ 1] !== 0x49) ||
      (wav[ 2] !== 0x46) ||
      (wav[ 3] !== 0x46) ||
      (wav[ 8] !== 0x57) ||
      (wav[ 9] !== 0x41) ||
      (wav[10] !== 0x56) ||
      (wav[11] !== 0x45)
    ) throw new Error(`Expected a WAV file`);
    const fmt = wav[20] | (wav[21] << 8);
    const numberOfChannels = wav[22] | (wav[23] << 8);
    const sampleRate = wav[24] | (wav[25] << 8) | (wav[26] << 16) | (wav[27] << 24);
    const sampleSize = wav[34] | (wav[35] << 8);
    if (fmt !== 1) throw new Error(`Expected WAV format 1 (LPCM), found ${fmt}`);
    if (sampleSize !== 16) throw new Error(`Expected sampleSize 16, found ${sampleSize}`);
    const samplec = (wav.length - 44) >> 1;
    const framec = samplec / numberOfChannels;
    const buffer = new AudioBuffer({
      length: framec,
      sampleRate,
      numberOfChannels,
    });
    if (numberOfChannels === 1) {
      const dst = buffer.getChannelData(0);
      const src = new Int16Array(wav.buffer, wav.byteOffset + 44, samplec);
      for (let p=0; p<src.length; p++) {
        dst[p] = src[p] / 32767;
      }
    } else if (numberOfChannels === 2) {
      const src = new Int16Array(wav.buffer, wav.byteOffset + 44, samplec);
      const dstl = buffer.getChannelData(0);
      const dstr = buffer.getChannelData(1);
      for (let srcp=0, dstp=0; srcp<src.length; srcp+=2, dstp+=1) {
        dstl[dstp] = src[srcp] / 32767;
        dstr[dstp] = src[srcp+1] / 32767;
      }
    } else {
      throw new Error(`Expected 1 or 2 channels, found ${numberOfChannels}`);
    }
    return buffer;
  }
  
  printWeb(serial) {
    /* No need for the outer Audio class. We'll make the OfflineAudioContext and SongPlayer instances ourselves.
     * This is roughly the same as what Audio.egg_play_sound() does.
     */
    if (!this.ctx) return Promise.reject("AudioContext not initialized");
    this.webPrintTime = 0;
    const startTime = Date.now();
    const sampleRate = this.ctx.sampleRate;
    const durms = Math.max(1, SongPlayer.calculateDuration(serial));
    const framec = Math.ceil((durms * sampleRate) / 1000);
    const chanc = 2;
    const ctx = new OfflineAudioContext(chanc, framec, sampleRate);
    const song = new SongPlayer(ctx, serial, 1.0, 0.0, false, 0);
    song.play();
    song.update(durms / 1000);
    return ctx.startRendering().then(buffer => {
      this.webPrintTime = Date.now() - startTime;
      return buffer;
    });
  }
  
  /* Render PCM and overlay.
   *****************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderPcmNow();
      this.renderOverlayNow();
    }, 20);
  }
  
  renderPcmNow() {
    const canvas = this.element.querySelector("canvas.pcm");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    this.renderPcm(ctx, 0, 0, bounds.width, bounds.height >> 1, this.nativeBuffer);
    this.renderPcm(ctx, 0, bounds.height >> 1, bounds.width, bounds.height >> 1, this.webBuffer);
    ctx.fillText("Native", 0, 10);
    ctx.fillText("Web", 0, (bounds.height >> 1) + 10);
  }
  
  renderPcm(ctx, x, y, w, h, buffer) {
    /* (buffer.length) should be substantially larger than (w).
     * Select a range of (buffer) for each pixel up to (w), and get the low and high for that range.
     */
    if ((w < 1) || (buffer.length < 1)) return;
    const src = buffer.getChannelData(0);
    ctx.beginPath();
    for (let xi=0, srcp=0; xi<w; xi++) {
      const nextp = Math.ceil(((xi + 1) * buffer.length) / w);
      let lo=src[srcp], hi=src[srcp];
      for (; srcp<nextp; srcp++) {
        if (src[srcp] < lo) lo = src[srcp];
        else if (src[srcp] > hi) hi = src[srcp];
      }
      ctx.moveTo(x + xi, y + h - (((lo + 1) * h) / 2));
      ctx.lineTo(x + xi, y + h - (((hi + 1) * h) / 2));
    }
    ctx.strokeStyle = "#000";
    ctx.stroke();
  }
  
  renderOverlayNow() {
    const canvas = this.element.querySelector("canvas.overlay");
    if (!canvas) return;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    
    if (this.node) { // If something's playing, gray out the other half.
      ctx.fillStyle = "#888";
      ctx.globalAlpha = 0.5;
      if (this.node.buffer === this.nativeBuffer) {
        ctx.fillRect(0, bounds.height >> 1, bounds.width, bounds.height >> 1);
      } else if (this.node.buffer === this.webBuffer) {
        ctx.fillRect(0, 0, bounds.width, bounds.height >> 1);
      }
      ctx.globalAlpha = 1;
    }
    
    if ((this.playhead > 0) && (this.playhead < this.playheadLimit)) {
      ctx.beginPath();
      const x = (this.playhead * bounds.width) / this.playheadLimit;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, bounds.height);
      ctx.strokeStyle = "#0a0";
      ctx.stroke();
    }
    
    ctx.beginPath();
    ctx.moveTo(0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, 0.5);
    ctx.strokeStyle = "#000";
    ctx.stroke();
  }
  
  requireOverlayUpdates() {
    if (this.overlayInterval) return;
    this.overlayInterval = this.window.setInterval(() => {
      this.playhead = this.ctx.currentTime - this.startTime;
      if (this.playhead >= this.playheadLimit) {
        this.playhead = 0;
        this.window.clearInterval(this.overlayInterval);
        this.overlayInterval = null;
        if (this.node) {
          this.node.stop();
          this.node.disconnect();
          this.node = null;
        }
      }
      this.renderOverlayNow();
    }, 50);
  }
  
  /* Events.
   *********************************************************************************/
   
  getSelectedBuffer() {
    const selection = this.element.querySelector("input[name='pcmSelect']:checked")?.value;
    switch (selection) {
      case "native": return this.nativeBuffer;
      case "web": return this.webBuffer;
    }
    return null;
  }
  
  startPlay(buffer, when) {
    if (this.node) {
      this.node.stop();
      this.node.disconnect();
      this.node = null;
    }
    this.playheadLimit = buffer.duration;
    if (!when || (when < 0)) when = 0;
    this.startTime = this.ctx.currentTime - when;
    this.playhead = when;
    const node = new AudioBufferSourceNode(this.ctx, { buffer });
    node.connect(this.ctx.destination);
    node.start(this.ctx.currentTime, when);
    this.node = node;
    this.requireOverlayUpdates();
  }
   
  onPlayheadClick(event) {
    if (!this.ctx) return;
    const buffer = this.getSelectedBuffer();
    if (!buffer) return;
    const canvas = this.element.querySelector("canvas.overlay");
    const bounds = canvas.getBoundingClientRect();
    const when = ((event.clientX - bounds.x) * buffer.duration) / bounds.width;
    this.startPlay(buffer, when);
  }
  
  onPlayPause() {
    if (!this.ctx) return;
    if (this.node) {
      this.node.stop();
      this.node.disconnect();
      this.node = null;
      if (this.overlayInterval) {
        this.window.clearInterval(this.overlayInterval);
        this.overlayInterval = null;
      }
      this.renderOverlayNow();
      return;
    }
    const buffer = this.getSelectedBuffer();
    if (!buffer) return;
    this.startPlay(buffer, this.playhead);
  }
  
  onPcmSelectChanged() {
    if (!this.node) return; // Not playing, all good.
    const buffer = this.getSelectedBuffer();
    if (!buffer) return;
    this.startPlay(buffer, this.playhead);
  }
}
