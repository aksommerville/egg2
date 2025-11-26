/* MidiService.js
 * Provides MIDI-In for synth testing.
 */
 
export class MidiService {
  static getDependencies() {
    return [Window];
  }
  constructor(window) {
    this.window = window;
    
    this.midiAccess = null;
    this.midiAccessPromise = null;
    this.pollTimeout = null;
    this.listeners = []; // {id,cb}
    this.nextListenerId = 1;
  }
  
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ id, cb });
    if (!this.midiAccess && !this.midiAccessPromise) this.requireMidiAccess();
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p < 0) return;
    this.listeners.splice(p, 1);
  }
  
  requireMidiAccess() {
    if (this.midiAccess) return Promise.resolve();
    if (this.midiAccessPromise) return this.midiAccessPromise;
    if (!this.window.navigator.requestMIDIAccess) {
      return Promise.reject(`MIDI access not available.`);
    }
    return this.midiAccessPromise = this.window.navigator.requestMIDIAccess().then(access => {
      this.midiAccess = access;
      this.midiAccessPromise = null;
      this.midiAccess.addEventListener("statechange", e => this.pollDevicesSoon());
      this.pollDevicesSoon();
    }).catch(e => {
      this.midiAccessPromise = null;
      throw e;
    });
  }
  
  // Linux + Chrome + MPK225, I get a whole bunch of state changes on connect and disconnect. Debounce.
  pollDevicesSoon() {
    if (this.pollTimeout) return;
    this.pollTimeout = this.window.setTimeout(() => {
      this.pollTimeout = null;
      this.pollDevicesNow();
    }, 100);
  }
  
  pollDevicesNow() {
    for (const [id, device] of this.midiAccess.inputs) {
      // Don't use addEventListener; we might have seen this device already.
      device.onmidimessage = e => this.onMidiMessage(e);
    }
  }
  
  onMidiMessage(event) {
    if (!(event?.data instanceof Uint8Array)) return;
    // We're not going to distinguish input devices. Anything coming in all gets dumped into the same bus.
    // We'll assume that (event.data) is cut on event boundaries. Would be pretty crazy if not.
    for (let srcp=0; srcp<event.data.length; ) {
      const len = this.measureEvent(event.data, srcp);
      if (len < 1) break;
      const mevt = this.readEvent(event.data, srcp, len);
      this.onMidiEvent(mevt);
      srcp += len;
    }
  }
  
  onMidiEvent(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  measureEvent(src, srcp) {
    switch (src[srcp] & 0xf0) {
      case 0x80:
      case 0x90:
      case 0xa0:
      case 0xb0:
      case 0xe0:
        return 3;
      case 0xc0:
      case 0xd0:
        return 2;
      case 0xf0: switch (src[srcp]) {
          case 0xf0: { // Sysex. We should never see these, we didn't request permission.
              const srcp0 = srcp;
              for (;;) {
                if (srcp >= src.length) return srcp - srcp0;
                if (src[srcp++] === 0xf7) return srcp - srcp0;
              }
            }
          case 0xf2: return 3; // Song Position Pointer.
          case 0xf3: return 2; // Song Select.
          case 0xf6: return 1; // Tune Request.
          case 0xf7: return 1; // End Of Sysex.
          case 0xf8: return 1; // Timing Clock.
          case 0xfa: return 1; // Start.
          case 0xfb: return 1; // Continue.
          case 0xfc: return 1; // Stop.
          case 0xfe: return 1; // Active Sensing.
          case 0xff: return 1; // Reset.
          default: return 1; // f1 f4 f5 f9 fd undefined, assume single byte.
        } break;
    }
    return 0;
  }
  
  readEvent(src, srcp, len) {
    const opcode = src[srcp] & 0xf0;
    const chid = src[srcp] & 0x0f;
    const a = src[srcp + 1];
    const b = src[srcp + 2];
    switch (opcode) {
      case 0x80:
      case 0x90:
      case 0xa0:
      case 0xb0:
        return { opcode, chid, a, b };
      case 0xc0:
      case 0xd0:
        return { opcode, chid, a };
      case 0xe0:
        return { opcode: 0, chid, a: 0, b: 0 }; // XXX My MPK225 has a noisy wheel and it's becoming a problem. Need some way to say "ignore wheels" in the UI.
        return { opcode, chid, a, b, v: ((a | (b << 7)) - 8192) / 8192 };
      case 0xf0: switch (src[srcp]) {
          case 0xf2: return { opcode: 0xf2, p: a | (b << 7) };
          case 0xf3: return { opcode: 0xf3, songid: a };
          default: return { opcode: src[srcp] };
        } break;
    }
    throw new Error(`Error reading event at ${srcp}:${len}/${src.length}`, src);
  }
}

MidiService.singleton = true;
