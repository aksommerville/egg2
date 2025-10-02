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
  
  /* (usage) is one of: "level" "range" "pitch"
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
   * If we're at EOF, or we have the special value [0,0], we return a per-usage default instead.
   */
  env(usage) {
    if (this.srcp > this.src.length - 2) {
      this.srcp = this.src.length;
      return this.defaultEnv(usage);
    }
    if (!this.src[this.srcp] && !this.src[this.srcp + 1]) {
      this.srcp += 2;
      return this.defaultEnv(usage);
    }
    const env = {
      usage,
      susp: 0,
      lo: [],
      isDefault: false,
    };
    const flags = this.src[this.srcp++];
    
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
          usage: "level",
          susp: 2,
          lo: [
            { t: 0x0000, v: 0x0000 },
            { t: 0x0020, v: 0x4000 },
            { t: 0x0050, v: 0x3000 },
            { t: 0x00d0, v: 0x0000 },
          ],
          hi: [
            { t: 0x0000, v: 0x0000 },
            { t: 0x0010, v: 0xffff },
            { t: 0x0040, v: 0x5000 },
            { t: 0x0140, v: 0x0000 },
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
    encoder.u16be(0);
    return;
  }

  let flags = 0;
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
  
  // Sustain point and point count. Fudge sustain point if it would otherwise produce 0,0.
  let susp = env.susp - 1;
  if (!flags && (env.lo.length <= 1) && !susp) susp = 15;
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

/* Given (mode) 0..4 and (modecfg) Uint8Array, return a live model object.
 * Creates all top-level fields whether or not they exist in the serial.
 * All models:
 *   {
 *     mode: per input
 *     extra: Uint8Array
 *   }
 * 1=DRUM: {
 *   drums: {
 *     noteid: u8
 *     trimlo: u8
 *     trimhi: u8
 *     pan: u8
 *     serial: Uint8Array
 *   }[]
 * }
 * 2=FM: {
 *   rate: float, relative or qnotes(absrate)
 *   absrate: boolean
 *   range: float
 *   levelenv: env
 *   rangeenv: env
 *   pitchenv: env
 *   wheelrange: u16
 *   lforate: float
 *   lfodepth: float
 *   lfophase: float
 * }
 * 3=HARSH: {
 *   shape: u8
 *   levelenv: env
 *   pitchenv: env
 *   wheelrange: u16
 * }
 * 4=HARM: {
 *   harmonics: u16[]
 *   levelenv: env
 *   pitchenv: env
 *   wheelrange: u16
 * }
 */
export function decodeModecfg(mode, modecfg) {
  const model = { mode };
  if (!modecfg) modecfg = new Uint8Array(0);
  const decoder = new EauDecoder(modecfg);
  switch (mode) {
  
    case 1: { // DRUM
        model.drums = [];
        while (!decoder.finished()) {
          const drum = {};
          drum.noteid = decoder.u8(0);
          drum.trimlo = decoder.u8(255);
          drum.trimhi = decoder.u8(255);
          drum.pan = decoder.u8(128);
          if (!(drum.serial = decoder.u16len(null))) break;
          model.drums.push(drum);
        }
      } break;
      
    case 2: { // FM
        const rate = decoder.u16(0);
        if (rate & 0x8000) {
          model.rate = (rate & 0x7fff) / 256;
          model.absrate = true;
        } else {
          model.rate = rate / 256;
          model.absrate = false;
        }
        model.range = decoder.u8_8(0);
        model.levelenv = decoder.env("level");
        model.rangeenv = decoder.env("range");
        model.pitchenv = decoder.env("pitch");
        model.wheelrange = decoder.u16(200);
        model.lforate = decoder.u8_8(0);
        model.lfodepth = decoder.u0_8(1);
        model.lfophase = decoder.u0_8(0);
      } break;
      
    case 3: { // HARSH
        model.shape = decoder.u8(0);
        model.levelenv = decoder.env("level");
        model.pitchenv = decoder.env("pitch");
        model.wheelrange = decoder.u16(200);
      } break;
      
    case 4: { // HARM
        model.harmonics = [];
        let harmc = decoder.u8(0);
        if (decoder.srcp <= decoder.src.length - harmc * 2) {
          while (harmc-- > 0) model.harmonics.push(decoder.u16(0));
        }
        model.levelenv = decoder.env("level");
        model.pitchenv = decoder.env("pitch");
        model.wheelrange = decoder.u16(200);
      } break;
      
  }
  model.extra = decoder.remainder();
  return model;
}

export function encodeModecfg(model) {
  const encoder = new Encoder();
  switch (model.mode) {
    case 0: return model.extra;
    case 1: {
        for (const drum of model.drums) {
          encoder.u8(drum.noteid);
          encoder.u8(drum.trimlo);
          encoder.u8(drum.trimhi);
          encoder.u8(drum.pan);
          encoder.pfxlen(2, () => encoder.raw(drum.serial));
        }
      } break;
    case 2: {
        let reqc = 0;
        if (model.extra.length) reqc = 10;
        else if (model.lfophase && model.lfodepth && model.lforate) reqc = 9;
        else if (model.lfodepth !== 1 && model.lforate) reqc = 8;
        else if (model.lforate) reqc = 7;
        else if (model.wheelrange !== 200) reqc = 6;
        else if (!model.pitchenv.isDefault) reqc = 5;
        else if (!model.rangeenv.isDefault) reqc = 4;
        else if (!model.levelenv.isDefault) reqc = 3;
        else if (model.range) reqc = 2;
        else if (model.rate) reqc = 1;
        if (reqc <= 0) break;
        let rate = Math.max(0, Math.min(0xffff, ~~(model.rate * 256)));
        if (model.absrate) rate |= 0x8000; else rate &= 0x7fff;
        encoder.u16be(rate);
        if (reqc <= 1) break;
        encoder.u16be(model.range * 256);
        if (reqc <= 2) break;
        encodeEnv(encoder, "level", model.levelenv);
        if (reqc <= 3) break;
        encodeEnv(encoder, "range", model.rangeenv);
        if (reqc <= 4) break;
        encodeEnv(encoder, "pitch", model.pitchenv);
        if (reqc <= 5) break;
        encoder.u16be(model.wheelrange);
        if (reqc <= 6) break;
        encoder.u16be(model.lforate * 256);
        if (reqc <= 7) break;
        encoder.u8(Math.max(0, Math.min(0xff, ~~(model.lfodepth * 256))));
        if (reqc <= 8) break;
        encoder.u8(Math.max(0, Math.min(0xff, ~~(model.lfophase * 256))));
        if (reqc <= 9) break;
        encoder.raw(model.extra);
      } break;
    case 3: {
        let reqc = 0;
        if (model.extra.length) reqc = 5;
        else if (model.wheelrange !== 200) reqc = 4;
        else if (!model.pitchenv.isDefault) reqc = 3;
        else if (!model.levelenv.isDefault) reqc = 2;
        else if (model.shape) reqc = 1;
        if (reqc <= 0) break;
        encoder.u8(model.shape);
        if (reqc <= 1) break;
        encodeEnv(encoder, "level", model.levelenv);
        if (reqc <= 2) break;
        encodeEnv(encoder, "pitch", model.pitchenv);
        if (reqc <= 3) break;
        encoder.u16be(model.wheelrange);
        if (reqc <= 4) break;
        encoder.raw(model.extra);
      } break;
    case 4: {
        let reqc = 0;
        if (model.extra.length) reqc = 5;
        else if (model.wheelrange !== 200) reqc = 4;
        else if (!model.pitchenv.isDefault) reqc = 3;
        else if (!model.levelenv.isDefault) reqc = 2;
        else if ((model.harmonics.length > 1) || ((model.harmonics.length === 1) && (model.harmonics[0] !== 0xffff))) reqc = 1;
        if (reqc <= 0) break;
        encoder.u8(model.harmonics.length);
        for (const harm of model.harmonics) {
          encoder.u16be(harm);
        }
        if (reqc <= 1) break;
        encodeEnv(encoder, "level", model.levelenv);
        if (reqc <= 2) break;
        encodeEnv(encoder, "pitch", model.pitchenv);
        if (reqc <= 3) break;
        encoder.u16be(model.wheelrange);
        if (reqc <= 4) break;
        encoder.raw(model.extra);
      } break;
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
  
  // To or from 0 (NOOP) or 1 (DRUM), there's nothing we can do.
  if ((newMode === 0) || (newMode === 1) || (oldMode === 0) || (oldMode === 1)) {
    if (stashConfig) return new Uint8Array(stashConfig);
    return new Uint8Array(0);
  }
  
  /* The common voiced modes (2=FM, 3=HARSH, 4=HARM) have three fields in common:
   *   levelenv, pitchenv, wheelrange
   * Do a full decode of each, reassign in the models, and reencode.
   */
  if ((newMode >= 2) && (newMode <= 4) && (oldMode >= 2) && (oldMode <= 4)) {
    const oldModel = decodeModecfg(oldMode, oldConfig);
    const stashModel = decodeModecfg(newMode, stashConfig);
    stashModel.levelenv = oldModel.levelenv;
    stashModel.pitchenv = oldModel.pitchenv;
    stashModel.wheelrange = oldModel.wheelrange;
    return encodeModecfg(stashModel);
  }
  
  // And finally, stash or empty.
  if (stashConfig) return new Uint8Array(stashConfig);
  return new Uint8Array(0);
}
