/* EauDecoder.js
 * Structured support for reading EAU bits like modecfg and post stages.
 */
 
import { Encoder } from "../Encoder.js";
 
export class EauDecoder {
  constructor(src) {
    this.src = src;
    this.srcp = 0;
  }
  
  finished() {
    return (this.srcp >= this.src.length);
  }
  
  u8(fallback) {
    if (this.srcp >= this.src.length) return fallback;
    return this.src[this.srcp++];
  }
  
  u16(fallback) {
    if (this.srcp > this.src.length - 2) { this.srcp = this.src.length; return fallback; }
    const v = (this.src[this.srcp] << 8) | this.src[this.srcp + 1];
    this.srcp += 2;
    return v;
  }
  
  u0_8(fallback) {
    if (this.srcp >= this.src.length) return fallback;
    return this.src[this.srcp++] / 256;
  }
  
  u8_8(fallback) {
    if (this.srcp > this.src.length - 2) { this.srcp = this.src.length; return fallback; }
    const v = this.src[this.srcp] + this.src[this.srcp + 1] / 256;
    this.srcp += 2;
    return v;
  }
  
  u8len(fallback) {
    if (this.srcp >= this.src.length) return fallback || new Uint8Array(0);
    const len = this.src[this.srcp++];
    if (this.srcp > this.src.length - len) { this.srcp = this.src.length; return fallback || new Uint8Array(0); }
    const body = new Uint8Array(this.src.buffer, this.src.byteOffset + this.srcp, len);
    this.srcp += len;
    return body;
  }
  
  u16len(fallback) {
    if (this.srcp > this.src.length - 2) { this.srcp = this.src.length; return fallback; }
    const len = (this.src[this.srcp] << 8) | this.src[this.srcp + 1];
    this.srcp += 2;
    if (this.srcp > this.src.length - len) { this.srcp = this.src.length; return fallback; }
    const body = new Uint8Array(this.src.buffer, this.src.byteOffset + this.srcp, len);
    this.srcp += len;
    return body;
  }
  
  remainder() {
    const body = new Uint8Array(this.src.buffer, this.src.byteOffset + this.srcp, this.src.length - this.srcp);
    this.srcp = this.src.length;
    return body;
  }
  
  /* Wave model is an array of: {
   *   opcode: u8
   *   param: Uint8Array
   * }
   * (opcode) must be known to decode, we'll throw an exception for unknown ones.
   * Once decoded, you can reencode without knowing what they mean.
   * Editor will be responsible for keeping opcodes and param lengths valid.
   */
  wave(fallback) {
  
    // Two fallback scenarios.
    if (this.srcp >= this.src.length) return this.fallbackWave(fallback);
    if (!this.src[this.srcp]) {
      this.srcp++;
      return this.fallbackWave(fallback);
    }
  
    const dst = [];
    for (;;) {
      if (this.srcp >= this.src.length) throw new Error(`Unterminated wave.`);
      const opcode = this.src[this.srcp++];
      if (!opcode) break; // EOF
      let paramlen = this.src.length;
      switch (opcode) {
        case 0x01: paramlen = 0; break; // SINE
        case 0x02: paramlen = 1; break; // SQUARE
        case 0x03: paramlen = 1; break; // SAW
        case 0x04: paramlen = 1; break; // TRIANGLE
        case 0x05: paramlen = 0; break; // NOISE
        case 0x06: paramlen = 1; break; // ROTATE
        case 0x07: paramlen = 2; break; // GAIN
        case 0x08: paramlen = 1; break; // CLIP
        case 0x09: paramlen = 1; break; // NORM
        case 0x0a: paramlen = 1 + (this.src[this.srcp] || 0) * 2; break; // HARMONICS
        case 0x0b: paramlen = 1; break; // HARMFM
        case 0x0c: paramlen = 1; break; // MAVG
      }
      if (this.srcp > this.src.length - paramlen) throw new Error(`Overrun in encoded wave.`);
      const param = new Uint8Array(this.src.buffer, this.src.byteOffset + this.srcp, paramlen);
      this.srcp += paramlen;
      dst.push({ opcode, param });
    }
    return dst;
  }
  
  fallbackWave(src) {
    if (!src) return [{
      opcode: 0x01, // SINE
      param: new Uint8Array(0),
    }];
    return src.map(op => ({
      opcode: op.opcode,
      param: new Uint8Array(op.param),
    }));
  }
  
  /* (usage) is one of: "level" "range" "pitch" "mix"
   * Returns a live env model:
   * {
   *   usage
   *   susp: 0 if no sustain. NB We do count the fake initial point, but it's not sustainable.
   *   lo: { // minimum one, first point must have (t==0)
   *     t: ms, absolute, must be sorted (NB relative when encoded, absolute here)
   *     v: u16
   *   }[]
   *   hi?: same as lo
   *   isDefault: boolean ; if false we encode explicitly even if it happens to match the default.
   * }
   * If we're at EOF, or we have the special value [0], we return a per-usage default instead.
   */
  env(usage) {
    if (this.srcp >= this.src.length) {
      return this.defaultEnv(usage);
    }
    if (!this.src[this.srcp]) {
      this.srcp += 1;
      return this.defaultEnv(usage);
    }
    const env = {
      usage,
      susp: 0,
      lo: [],
      isDefault: false,
    };
    const flags = this.src[this.srcp++];
    if (flags & 0xf0) throw new Error(`Unexpected env flags 0x${flags.toString(16)}`);
    
    if (flags & 0x02) { // Velocity
      env.hi = [];
    }
    if (flags & 0x01) { // Initials
      const initlo = (this.src[this.srcp] << 8) | this.src[this.srcp + 1];
      this.srcp += 2;
      env.lo.push({ t: 0, v: initlo });
      if (env.hi) {
        const inithi = (this.src[this.srcp] << 8) | this.src[this.srcp + 1];
        this.srcp += 2;
        env.hi.push({ t: 0, v: inithi });
      }
    } else {
      let init = 0;
      switch (usage) {
        case "pitch": init = 0x8000; break;
        case "range": init = 0xffff; break;
      }
      env.lo.push({ t: 0, v: init });
      if (env.hi) env.hi.push({ t: 0, v: init });
    }
    
    const susp_ptc = this.src[this.srcp++] || 0;
    const ptc = susp_ptc & 15;
    const ptlen = env.hi ? 8 : 4;
    if (this.srcp > this.src.length - ptlen * ptc) {
      this.srcp = this.src.length;
      return this.defaultEnv(usage);
    }
    if (flags & 0x04) { // Sustain
      env.susp = (susp_ptc >> 4) + 1;
      if (env.susp > ptc) env.susp = 0;
    }
    
    let nowlo=0, nowhi=0;
    for (let i=0; i<ptc; i++) {
      const tlo = (this.src[this.srcp] << 8) | this.src[this.srcp + 1]; this.srcp += 2;
      const vlo = (this.src[this.srcp] << 8) | this.src[this.srcp + 1]; this.srcp += 2;
      nowlo += tlo;
      env.lo.push({ t: nowlo, v: vlo });
      if (env.hi) {
        const thi = (this.src[this.srcp] << 8) | this.src[this.srcp + 1]; this.srcp += 2;
        const vhi = (this.src[this.srcp] << 8) | this.src[this.srcp + 1]; this.srcp += 2;
        nowhi += thi;
        env.hi.push({ t: nowhi, v: vhi });
      }
    }
    
    return env;
  }
  
  defaultEnv(usage) {
    switch (usage) {
      case "level": return {
          // Try to match src/opt/synth/synth_env.c:synth_env_fallback(). Mind that our (t) are absolute.
          usage: "level",
          susp: 2,
          lo: [
            { t:   0, v: 0x0000 },
            { t:  30, v: 0x8000 },
            { t:  50, v: 0x4000 },
            { t: 200, v: 0x0000 },
          ],
          hi: [
            { t:   0, v: 0x0000 },
            { t:  20, v: 0xffff },
            { t:  40, v: 0x6666 },
            { t: 340, v: 0x0000 },
          ],
          isDefault: true,
        };
      case "range": return {
          usage: "range",
          susp: 0,
          lo: [{ t: 0, v: 0xffff }],
          isDefault: true,
        };
      case "pitch": return {
          usage: "pitch",
          susp: 0,
          lo: [{ t: 0, v: 0x8000 }],
          isDefault: true,
        };
      case "mix": return {
          usage: "mix",
          susp: 0,
          lo: [{ t: 0, v: 0x0000 }],
          isDefault: true,
        };
    }
    return {
      usage,
      susp: 0,
      lo: [{ t: 0, v: 0 }],
      isDefault: true,
    };
  }
}

export function encodeEnv(encoder, usage, env) {

  if (env.isDefault) {
    encoder.u8(0);
    return;
  }

  let flags = 0x08; // "Present" always.
  // 0x02 Velocity if (hi) exists and is different from (lo) anywhere.
  if (env.hi) {
    for (let i=env.hi.length; i-->0; ) {
      if ((env.lo[i].t !== env.hi[i].t) || (env.lo[i].v !== env.hi[i].v)) {
        flags |= 0x02;
        break;
      }
    }
  }
  // 0x01 Initials if either in-use line has a nonzero [0].v.
  if (env.lo[0].v || ((flags & 0x02) && env.hi[0].v)) {
    flags |= 0x01;
  }
  // 0x04 Sustain if the sustain point is in range. NB zero is illegal.
  if ((env.susp > 0) && (env.susp < env.lo.length)) {
    flags |= 0x04;
  }
  encoder.u8(flags);
  
  // Initials.
  if (flags & 0x01) {
    encoder.u16be(env.lo[0].v);
    if (flags & 0x02) {
      encoder.u16be(env.hi[0].v);
    }
  }
  
  // Sustain point and point count.
  let susp = env.susp - 1;
  encoder.u8((susp << 4) | (env.lo.length - 1));
  
  // Points. Skip the first.
  for (let i=1, nowlo=0, nowhi=0; i<env.lo.length; i++) {
    const t = Math.max(0, Math.min(0xffff, ~~(env.lo[i].t - nowlo)));
    encoder.u16be(t);
    encoder.u16be(env.lo[i].v);
    nowlo = env.lo[i].t;
    if (flags & 0x02) {
      const t2 = Math.max(0, Math.min(0xffff, ~~(env.hi[i].t - nowhi)));
      encoder.u16be(t2);
      encoder.u16be(env.hi[i].v);
      nowhi = env.hi[i].t;
    }
  }
}

export function nonDefaultWave(wave) {
  if (!wave.length) return false;
  if ((wave.length === 1) && (wave[0].opcode === 0x01)) return false;
  return true;
}

export function wavesEquivalent(a, b) {
  if (a.length !== b.length) return false;
  for (let i=a.length; i-->0; ) {
    const aop = a[i], bop = b[i];
    if (aop.opcode !== bop.opcode) return false;
    if (aop.param.length !== bop.param.length) return false;
    for (let ii=aop.param.length; ii-->0; ) {
      if (aop.param[ii] !== bop.param[ii]) return false;
    }
  }
  return true;
}

export function encodeWave(dst, wave, defaultIfEquivalent) {
  if (defaultIfEquivalent) {
    if (wavesEquivalent(wave, defaultIfEquivalent)) {
      dst.u8(0);
      return;
    }
  } else {
    if (!nonDefaultWave(wave)) {
      dst.u8(0);
      return;
    }
  }
  for (const { opcode, param } of wave) {
    dst.u8(opcode);
    dst.raw(param);
  }
  if (!wave.length || (wave[wave.length - 1].opcode !== 0x00)) { // We let models omit the trailing EOF.
    dst.u8(0);
  }
}

/* {
 *   wheelrange: u16
 *   minlevel: u16
 *   maxlevel: u16
 *   minhold: u16
 *   rlstime: u16
 * }
 */
export function decodeTrivialModecfg(dst, src) {
  dst.wheelrange = src.u16(200);
  dst.minlevel = src.u16(0x2000);
  dst.maxlevel = src.u16(0xffff);
  dst.minhold = src.u16(100);
  dst.rlstime = src.u16(100);
}
export function encodeTrivialModecfg(dst, src) {
  const fldc =
    (src.rlstime !== 100) ? 5 :
    (src.minhold !== 100) ? 4 :
    (src.maxlevel !== 0xffff) ? 3 :
    (src.minlevel !== 0x2000) ? 2 :
    (src.wheelrange !== 200) ? 1 :
    0;
  if (fldc < 1) return; dst.u16be(src.wheelrange);
  if (fldc < 2) return; dst.u16be(src.minlevel);
  if (fldc < 3) return; dst.u16be(src.maxlevel);
  if (fldc < 4) return; dst.u16be(src.minhold);
  if (fldc < 5) return; dst.u16be(src.rlstime);
}

/* {
 *   levelenv: env
 *   wheelrange: u16
 *   wavea: wave
 *   waveb: wave
 *   mixenv: env
 *   modrate: u16 (NB not float)
 *   modrange: u16 (NB not float)
 *   rangeenv: env
 *   pitchenv: env
 *   modulator: wave
 *   rangelforate: u8.8
 *   rangelfodepth: u8
 *   rangelfowave: wave
 *   mixlforate: u8.8
 *   mixlfodepth: u8
 *   mixlfowave: wave
 * }
 */
export function decodeFmModecfg(dst, src) {
  dst.levelenv = src.env("level");
  dst.wheelrange = src.u16(200);
  dst.wavea = src.wave();
  dst.waveb = src.wave(dst.wavea);
  dst.mixenv = src.env("mix");
  dst.modrate = src.u16(0);
  dst.modrange = src.u16(0x0100);
  dst.rangeenv = src.env("range");
  dst.pitchenv = src.env("pitch");
  dst.modulator = src.wave();
  dst.rangelforate = src.u8_8(0);
  dst.rangelfodepth = src.u8(0xff);
  dst.rangelfowave = src.wave();
  dst.mixlforate = src.u8_8(0);
  dst.mixlfodepth = src.u8(0xff);
  dst.mixlfowave = src.wave();
}
export function encodeFmModecfg(dst, src) {
  const fldc =
    (src.mixlforate && nonDefaultWave(src.mixlfowave)) ? 16 :
    (src.mixlforate && (src.mixlfodepth !== 0xff)) ? 15 :
    src.mixlforate ? 14 :
    (src.rangelforate && nonDefaultWave(src.rangelfowave)) ? 13 :
    (src.rangelforate && (src.rangelfodepth !== 0xff)) ? 12 :
    src.rangelforate ? 11 :
    nonDefaultWave(src.modulator) ? 10 :
    (!src.pitchenv.isDefault) ? 9 :
    (!src.rangeenv.isDefault) ? 8 :
    (src.modrange !== 0x0100) ? 7 :
    src.modrate ? 6 :
    (!src.mixenv.isDefault) ? 5 :
    (!wavesEquivalent(src.wavea, src.waveb)) ? 4 :
    nonDefaultWave(src.wavea) ? 3 :
    (src.wheelrange !== 200) ? 2 :
    (!src.levelenv.isDefault) ? 1 :
    0;
  if (fldc < 1) return; encodeEnv(dst, "level", src.levelenv);
  if (fldc < 2) return; dst.u16be(src.wheelrange);
  if (fldc < 3) return; encodeWave(dst, src.wavea);
  if (fldc < 4) return; encodeWave(dst, src.waveb, src.wavea);
  if (fldc < 5) return; encodeEnv(dst, "mix", src.mixenv);
  if (fldc < 6) return; dst.u16be(src.modrate);
  if (fldc < 7) return; dst.u16be(src.modrange);
  if (fldc < 8) return; encodeEnv(dst, "range", src.rangeenv);
  if (fldc < 9) return; encodeEnv(dst, "pitch", src.pitchenv);
  if (fldc < 10) return; encodeWave(dst, src.modulator);
  if (fldc < 11) return; dst.u8_8(src.rangelforate);
  if (fldc < 12) return; dst.u8(src.rangelfodepth);
  if (fldc < 13) return; encodeWave(dst, src.rangelfowave);
  if (fldc < 14) return; dst.u8_8(src.mixlforate);
  if (fldc < 15) return; dst.u8(src.mixlfodepth);
  if (fldc < 16) return; encodeWave(dst, src.mixlfowave);
}

/* {
 *   levelenv: env
 *   widthlo: u16
 *   widthhi: u16
 *   stagec: u8
 *   gain: u8.8
 * }
 */
export function decodeSubModecfg(dst, src) {
  dst.levelenv = src.env("level");
  dst.widthlo = src.u16(200);
  dst.widthhi = src.u16(dst.widthlo);
  dst.stagec = src.u8(1);
  dst.gain = src.u8_8(1.0);
}
export function encodeSubModecfg(dst, src) {
  const fldc =
    (src.gain !== 1.0) ? 5 :
    (src.stagec !== 1) ? 4 :
    (src.widthhi !== src.widthlo) ? 3 :
    (src.widthlo !== 200) ? 2 :
    (!src.levelenv.isDefault) ? 1 :
    0;
  if (fldc < 1) return; encodeEnv(dst, "level", src.levelenv);
  if (fldc < 2) return; dst.u16be(src.widthlo);
  if (fldc < 3) return; dst.u16be(src.widthhi);
  if (fldc < 4) return; dst.u8(src.stagec);
  if (fldc < 5) return; dst.u8_8(src.gain);
}

/* Decode a DRUM modecfg, just the notes array.
 * We're a little unlike the other modes in that we take Uint8Array and return the array of notes.
 * Most modecfg decoders take an EauDecoder and a fresh model to assign to.
 * {
 *   noteid: u8
 *   trimlo: u8
 *   trimhi: u8
 *   pan: u8
 *   serial: Uint8Array
 * }[]
 */
export function decodeDrumModecfg(src) {
  const dst = []; // {noteid,trimlo,trimhi,pan,serial}
  for (let srcp=0; srcp<src.length; ) {
    const noteid = src[srcp++];
    const trimlo = src[srcp++];
    const trimhi = src[srcp++];
    const pan = src[srcp++];
    const len = (src[srcp] << 8) | src[srcp+1];
    srcp += 2;
    if (srcp > src.length - len) break;
    dst.push({
      noteid, trimlo, trimhi, pan,
      serial: new Uint8Array(src.buffer, src.byteOffset + srcp, len),
    });
    srcp += len;
  }
  return dst;
}
export function encodeDrumModecfg(dst, src) {
  if (src.drums) src = src.drums; // Accept either the full model or just the drums array.
  for (const drum of src) {
    dst.u8(drum.noteid);
    dst.u8(drum.trimlo);
    dst.u8(drum.trimhi);
    dst.u8(drum.pan);
    dst.u16be(drum.serial.length);
    dst.raw(drum.serial);
  }
}

/* Generic modecfg decode.
 * All returned models contain:
 * {
 *   mode: u8
 *   extra: Uint8Array
 * }
 * And typically a bunch of other fields, see above.
 * By convention, every field for the given mode will be created, even if it was absent from the input.
 */
export function decodeModecfg(mode, modecfg) {
  const model = { mode };
  const decoder = new EauDecoder(modecfg || new Uint8Array(0));
  switch (mode) {
    case 0: break; // NOOP
    case 1: decodeTrivialModecfg(model, decoder); break;
    case 2: decodeFmModecfg(model, decoder); break;
    case 3: decodeSubModecfg(model, decoder); break;
    case 4: model.drums = decodeDrumModecfg(modecfg); model.extra = new Uint8Array(0); return model;
  }
  model.extra = decoder.remainder();
  return model;
}

export function encodeModecfg(model) {
  const encoder = new Encoder();
  switch (model.mode) {
    case 0: break; // NOOP
    case 1: encodeTrivialModecfg(encoder, model); break;
    case 2: encodeFmModecfg(encoder, model); break;
    case 3: encodeSubModecfg(encoder, model); break;
    case 4: encodeDrumModecfg(encoder, model); break;
    default: encoder.raw(model.extra); break; // Encode (extra) only for unknown (mode).
  }
  return encoder.finish();
}

/* Generate a new modecfg, when user changes mode.
 *  - (newMode) 0..4, what we're generating for.
 *  - (stashConfig) Uint8Array of a previous modecfg for (newMode), if you have one. null is fine.
 *  - (oldMode) mode you're coming from.
 *  - (oldConfig) config you're coming from, must match (oldMode).
 * In general:
 *  - If a field for (newMode) also exists in (oldMode), you'll get its value from (oldConfig).
 *  - The remainder is taken from (stashConfig) if provided.
 *  - We'll fill in defaults where necessary.
 *  - Empty modecfg are always legal, and we'll land there as a final default.
 * It works out simpler than you'd think.
 */
export function mergeModecfg(newMode, stashConfig, oldMode, oldConfig) {
  
  // Current mode not configured at all? Return the stash or empty.
  if (!oldConfig?.length) {
    if (stashConfig) return new Uint8Array(stashConfig);
    return new Uint8Array(0);
  }
  
  // To or from 0 (NOOP) or 4 (DRUM), there's nothing we can do.
  if ((newMode === 0) || (newMode === 4) || (oldMode === 0) || (oldMode === 4)) {
    if (stashConfig) return new Uint8Array(stashConfig);
    return new Uint8Array(0);
  }
  
  /* Any other case, decode stashConfig and oldConfig.
   * Any field that exists in both, copy from oldConfig to stashConfig.
   * Then reencode stashConfig and that's our answer.
   */
  const oldModel = decodeModecfg(oldMode, oldConfig);
  const stashModel = decodeModecfg(newMode, stashConfig);
  for (const k of Object.keys(stashModel)) {
    if (oldModel.hasOwnProperty(k)) {
      stashModel[k] = oldModel[k];
    }
  }
  stashModel.mode = newMode; // The loop above might overwrite it.
  return encodeModecfg(stashModel);
}

/* Decode one post stage, starting from (stageid).
 * The full post block is a packed set of these, no other data.
 * You provide a prepared EauDecoder.
 * All stageid: {
 *   stageid: u8
 *   extra: Uint8Array
 * }
 * 1=GAIN: {
 *   gain: float (u8.8)
 *   clip: u8
 *   gate: u8
 * }
 * 2=DELAY: {
 *   period: float, qnotes (u8.8)
 *   dry: u8
 *   wet: u8
 *   store: u8
 *   feedback: u8
 *   sparkle: u8 (0..128..255)
 * }
 * 3=TREMOLO: {
 *   period: float, qnotes (u8.8)
 *   depth: u8
 *   phase: u8
 *   sparkle: u8 (0..128..255)
 * }
 * 4=DETUNE: {
 *   period: float, qnotes (u8.8)
 *   mix: u8
 *   depth: u8
 *   phase: u8
 *   rightphase: u8
 * }
 * 5=WAVESHAPER: {
 *   // u16 levels in (extra)
 * }
 * 6=LOPASS: { NOT IMPLEMENTED
 *   mid: u16
 * }
 * 7=HIPASS: { NOT IMPLEMENTED
 *   mid: u16
 * }
 * 8=BPASS: { NOT IMPLEMENTED
 *   mid: u16
 *   width: u16
 * }
 * 9=NOTCH: { NOT IMPLEMENTED
 *   mid: u16
 *   width: u16
 * }
 */
export function decodePostStage(decoder) {
  const stage = {};
  stage.stageid = decoder.u8();
  const serial = decoder.u8len();
  decoder = new EauDecoder(serial);
  switch (stage.stageid) {
    case 1: { // GAIN
        stage.gain = decoder.u8_8(1);
        stage.clip = decoder.u8(0xff);
        stage.gate = decoder.u8(0);
      } break;
    case 2: { // DELAY
        stage.period = decoder.u8_8(1);
        stage.dry = decoder.u8(0x80);
        stage.wet = decoder.u8(0x80);
        stage.store = decoder.u8(0x80);
        stage.feedback = decoder.u8(0x80);
        stage.sparkle = decoder.u8(0x80);
      } break;
    case 3: { // TREMOLO
        stage.period = decoder.u8_8(1);
        stage.depth = decoder.u8(0xff);
        stage.phase = decoder.u8(0);
        stage.sparkle = decoder.u8(0x80);
      } break;
    case 4: { // DETUNE
        stage.period = decoder.u8_8(1);
        stage.mix = decoder.u8(0x80);
        stage.depth = decoder.u8(0x80);
        stage.phase = decoder.u8(0);
        stage.rightphase = decoder.u8(0);
      } break;
    case 5: { // WAVESHAPER
        // u16[]; caller should read out of (extra)
      } break;
    case 6: { // LOPASS
        stage.mid = decoder.u16(0);
      } break;
    case 7: { // HIPASS
        stage.mid = decoder.u16(0);
      } break;
    case 8: { // BPASS
        stage.mid = decoder.u16(0);
        stage.width = decoder.u16(200);
      } break;
    case 9: { // NOTCH
        stage.mid = decoder.u16(0);
        stage.width = decoder.u16(200);
      } break;
  }
  stage.extra = decoder.remainder();
  return stage;
}

/* Encode one post stage to your Encoder.
 */
export function encodePostStage(encoder, stage) {
  encoder.u8(stage.stageid);
  encoder.pfxlen(1, () => {
    switch (stage.stageid) {
    
      case 1: { // GAIN
          const fldc = 
            stage.extra.length ? 3 :
            (stage.gate !== 0x00) ? 3 :
            (stage.clip !== 0xff) ? 2 :
            (stage.gain !== 1.0) ? 1 :
            0;
          if (fldc < 1) break; encoder.u8_8(stage.gain);
          if (fldc < 2) break; encoder.u8(stage.clip);
          if (fldc < 3) break; encoder.u8(stage.gate);
        } break;
    
      case 2: { // DELAY
          const fldc =
            stage.extra.length ? 6 :
            (stage.sparkle !== 0x80) ? 6 :
            (stage.feedback !== 0x80) ? 5 :
            (stage.store !== 0x80) ? 4 :
            (stage.wet !== 0x80) ? 3 :
            (stage.dry !== 0x80) ? 2 :
            (stage.period !== 1) ? 1 :
            0;
          if (fldc < 1) break; encoder.u8_8(stage.period);
          if (fldc < 2) break; encoder.u8(stage.dry);
          if (fldc < 3) break; encoder.u8(stage.wet);
          if (fldc < 4) break; encoder.u8(stage.store);
          if (fldc < 5) break; encoder.u8(stage.feedback);
          if (fldc < 6) break; encoder.u8(stage.sparkle);
        } break;
        
      case 3: { // TREMOLO
          const fldc = 
            stage.extra.length ? 4 :
            (stage.sparkle !== 0x80) ? 4 :
            (stage.phase !== 0x00) ? 3 :
            (stage.depth !== 0xff) ? 2 :
            (stage.period !== 1.0) ? 1 :
            0;
          if (fldc < 1) break; encoder.u8_8(stage.period);
          if (fldc < 2) break; encoder.u8(stage.depth);
          if (fldc < 3) break; encoder.u8(stage.phase);
          if (fldc < 4) break; encoder.u8(stage.sparkle);
        } break;
        
      case 4: { // DETUNE
          const fldc =
            stage.extra.length ? 5 :
            (stage.rightphase !== 0x00) ? 5 :
            (stage.phase !== 0x00) ? 4 :
            (stage.depth !== 0x80) ? 3 :
            (stage.mix !== 0x80) ? 2 :
            (stage.period !== 1.0) ? 1 :
            0;
          if (fldc < 1) break; encoder.u8_8(stage.period);
          if (fldc < 2) break; encoder.u8(stage.mix);
          if (fldc < 3) break; encoder.u8(stage.depth);
          if (fldc < 4) break; encoder.u8(stage.phase);
          if (fldc < 5) break; encoder.u8(stage.rightphase);
        } break;
        
      case 5: { // WAVESHAPER
          // All content in (extra)
        } break;
        
      case 6: case 7: { // LOPASS,HIPASS
          encoder.u16be(stage.mid);
        } break;
        
      case 8: case 9: { // BPASS,NOTCH
          encoder.u16be(stage.mid);
          encoder.u16be(stage.width);
        } break;
        
    }
    encoder.raw(stage.extra);
  });
}
