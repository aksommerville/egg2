/* songBits.js
 * Little self-contained helpers for audio.
 */
 
export const FREQUENCY_BY_NOTEID = [];
export const WAVE_SIZE_SAMPLES = 1024; // Same as native synthesizer, but doesn't strictly need to be.

export function requireFrequencies() {
  if (FREQUENCY_BY_NOTEID.length) return;
  for (let noteid=0; noteid<0x80; noteid++) {
    FREQUENCY_BY_NOTEID.push(440 * 2 ** ((noteid - 69) / 12));
  }
}

/* For reading fields off an EAU Channel Header.
 * This is not used for posts, event streams, or the outer framing; those are all done inline where needed.
 */
export class EauDecoder {
  constructor(src) {
    this.src = src;
    this.srcp = 0;
  }
  
  /* Return an Envelope instance, or null at EOF.
   * (raw) to return values in 0..0xffff as encoded, default to scale them to 0..1.
   */
  env(raw) {
    if (this.srcp >= this.src.length) return null;
    try {
      const env = new Envelope();
      this.srcp = env.decode(this.src, this.srcp, raw);
      return env;
    } catch (e) {
      this.srcp = this.src.length;
      return null;
    }
  }
  
  /* "sine", "square", "sawtooth", "triangle", or a Float32Array of a single-period wave.
   */
  wave() {
    const shape = this.u8();
    const qual = this.u8();
    const harmc = this.u8();
    if (this.srcp > this.src.length - harmc * 2) return "sine";
    const harmv = [];
    for (let i=harmc; i-->0; this.srcp+=2) {
      harmv.push(((this.src[this.srcp] << 8) | this.src[this.srcp+1]) / 65535);
    }
    // With no qualifier or harmonics, we might be able to use the trivial built-ins.
    if (!qual && !harmc) switch (shape) {
      case 0: return "sine";
      case 1: return "square";
      case 2: return "sawtooth";
      case 3: return "triangle";
      case 4: break; // fixedfm, must synthesize
      default: return "sine"; // unknown shape, default to unmodified sine
    }
    let wave = generatePrimitiveWave(shape, qual);
    if (harmc) {
      wave = applyHarmonics(wave, harmv);
    }
    return wave;
  }
  
  u16() {
    if (this.srcp > this.src.length - 2) return 0;
    const v = (this.src[this.srcp] << 8) | this.src[this.srcp+1];
    this.srcp += 2;
    return v;
  }
  
  u0_16() {
    return this.u16() / 65535;
  }
  
  u8_8() {
    return this.u16() / 256;
  }
  
  u0_8() {
    return this.u8() / 255;
  }
  
  u8() {
    if (this.srcp >= this.src.length) return 0;
    return this.src[this.srcp++];
  }
}

export class Envelope {
  constructor() {
    this.flags = 0; // 1,2,4 = Velocity,Initials,Sustain
    this.initlo = 0; // 0..1
    this.inithi = 0; // 0..1
    this.susp = 0xff;
    this.points = []; // {tlo,thi,vlo,vhi}. Times in relative seconds, values in 0..1
  }
  
  static constant(k) {
    const env = new Envelope();
    env.initlo = env.inithi = k;
    return env;
  }
  
  // Returns the new (srcp). Throws null if misencoded.
  decode(src, srcp, raw) {
    this.flags = src[srcp++];
    if (this.flags & 0x02) { // Initials.
      if (srcp > src.length - 2) throw null;
      this.initlo = ((src[srcp] << 8) | src[srcp+1]);
      srcp += 2;
      if (this.flags & 0x01) { // Velocity.
        if (srcp > src.length - 2) throw null;
        this.inithi = ((src[srcp] << 8) | src[srcp+1]);
        srcp += 2;
      } else {
        this.inithi = this.initlo;
      }
      if (!raw) {
        this.inithi /= 65535;
        this.initlo /= 65535;
      }
    }
    this.susp = 0xff;
    if (this.flags & 0x04) { // Sustain.
      if (srcp >= src.length) throw null;
      this.susp = src[srcp++];
    }
    if (srcp >= src.length) throw null;
    const pointc = src[srcp++];
    if (pointc > 16) throw null;
    const pointlen = (this.flags & 0x01) ? 8 : 4;
    if (srcp > src.length - pointc * pointlen) throw null;
    this.points = [];
    for (let i=pointc; i-->0; ) {
      const tlo = ((src[srcp] << 8) | src[srcp+1]) / 1000;
      srcp += 2;
      let vlo = ((src[srcp] << 8) | src[srcp+1]);
      srcp += 2;
      let thi=tlo, vhi=vlo;
      if (pointlen === 8) {
        thi = ((src[srcp] << 8) | src[srcp+1]) / 1000;
        srcp += 2;
        vhi = ((src[srcp] << 8) | src[srcp+1]);
        srcp += 2;
      }
      if (!raw) {
        vlo /= 65535;
        vhi /= 65535;
      }
      this.points.push({ tlo, thi, vlo, vhi });
    }
    if (this.susp < this.points.length) {
      const p = { ...this.points[this.susp] };
      p.tlo = p.thi = 0;
      this.points.splice(this.susp + 1, 0, p);
    }
    return srcp;
  }
  
  scale(mlt) {
    this.initlo *= mlt;
    this.inithi *= mlt;
    for (const pt of this.points) {
      pt.vlo *= mlt;
      pt.vhi *= mlt;
    }
  }
  
  bias(add) {
    this.initlo += add;
    this.inithi += add;
    for (const pt of this.points) {
      pt.vlo += add;
      pt.vhi += add;
    }
  }
  
  /* (param) should be GainNode.gain.
   * Returns the absolute end time (>when).
   */
  apply(param, velocity, when, dur) {
    let hiw = Math.max(0, Math.min(1, velocity));
    let low = 1 - hiw;

    const initValue = this.initlo * low + this.inithi * hiw;
    param.setValueAtTime(0, initValue);
    param.setValueAtTime(when, initValue);

    const rlstmin = when + dur;
    let susp = this.susp;
    for (const p of this.points) {
      let t = p.tlo * low + p.thi * hiw;
      if (!susp--) {
        const extra = rlstmin - when;
        if (extra > t) t = extra;
      }
      const v = p.vlo * low + p.vhi * hiw;
      when += t;
      param.linearRampToValueAtTime(v, when);
    }
    return when;
  }
  
  /* Same as (apply) but also multiply each value by (mlt).
   */
  mltapply(param, mlt, velocity, when, dur) {
    let hiw = Math.max(0, Math.min(1, velocity));
    let low = 1 - hiw;

    const initValue = (this.initlo * low + this.inithi * hiw) * mlt;
    param.setValueAtTime(0, initValue);
    param.setValueAtTime(when, initValue);

    const rlstmin = when + dur;
    let susp = this.susp;
    for (const p of this.points) {
      let t = p.tlo * low + p.thi * hiw;
      if (!susp--) {
        const extra = rlstmin - when;
        if (extra > t) t = extra;
      }
      const v = (p.vlo * low + p.vhi * hiw) * mlt;
      when += t;
      param.linearRampToValueAtTime(v, when);
    }
    return when;
  }
}

//TODO Generated waves are read-only and rate-independent. We can cache them globally.

function generateSine() {
  const wave = new Float32Array(WAVE_SIZE_SAMPLES);
  for (let i=0, t=0, dt=Math.PI*2/WAVE_SIZE_SAMPLES; i<WAVE_SIZE_SAMPLES; i++, t+=dt) {
    wave[i] = Math.sin(t);
  }
  return wave;
}

function generateSquare(qual) {
  const wave = new Float32Array(WAVE_SIZE_SAMPLES);
  const halflen = WAVE_SIZE_SAMPLES >> 1;
  const quarter = halflen >> 1;
  const curvelen = (qual * quarter) >> 8;
  // Draw the first quarter of the wave: sine 0..curvelen, then 1 the rest of the way.
  for (let i=0; i<curvelen; i++) wave[i] = Math.sin((i * Math.PI / 2) / curvelen);
  for (let i=curvelen; i<quarter; i++) wave[i] = 1;
  // Second quarter is the first one in reverse.
  for (let from=quarter,i=quarter; i<halflen; i++) wave[i] = wave[--from];
  // Second half is the first half negative (forward or backward, doesn't matter).
  for (let from=0, i=halflen; i<WAVE_SIZE_SAMPLES; from++, i++) wave[i] = -wave[from];
  return wave;
}

function generateRamp(dst, p, c, a, z) {
  if (c < 1) return;
  let v = a;
  let d = (z - a) / c;
  for (; c-->0; p++, v+=d) dst[p] = v;
}

function generateSaw(qual) {
  const wave = new Float32Array(WAVE_SIZE_SAMPLES);
  const halflen = WAVE_SIZE_SAMPLES >> 1;
  const quarter = halflen >> 1;
  const curvelen = (qual * quarter) >> 8;
  // We sacrifice a little continuity for simplicity's sake. Native does exactly the same thing.
  // To wit: First we do a sigmoid from -1 to 1 over (curvelen), then a ramp down to the end.
  // So the endpoints of the ramp don't blend nicely into the sigmoid. But I bet it will sound OK.
  for (let i=0; i<curvelen; i++) wave[i] = Math.sin(Math.PI / 4 + (i * Math.PI) / curvelen);
  generateRamp(wave, curvelen, WAVE_SIZE_SAMPLES - curvelen, 1, -1);
  return wave;
}

function generateTriangle() {
  const wave = new Float32Array(WAVE_SIZE_SAMPLES);
  const halflen = WAVE_SIZE_SAMPLES >> 1;
  generateRamp(wave, 0, halflen, -1, 1);
  generateRamp(wave, halflen, halflen, 1, -1);
  return wave;
}

function generateFixedfm(qual) {
  const wave = new Float32Array(WAVE_SIZE_SAMPLES);
  const rate = qual >> 4;
  const range = qual & 15;
  const cardp = (Math.PI * 2) / WAVE_SIZE_SAMPLES;
  const moddp = cardp * rate;
  let carp=0, modp=0;
  for (let i=0; i<WAVE_SIZE_SAMPLES; i++) {
    wave[i] = Math.sin(carp);
    const mod = Math.sin(modp) * range;
    modp += moddp;
    if (modp > Math.PI) modp -= Math.PI * 2;
    carp += cardp + cardp * mod;
  }
  return wave;
}

export function generatePrimitiveWave(shape, qual) {
  switch (shape) {
    case 0: return generateSine();
    case 1: return generateSquare(qual);
    case 2: return generateSaw(qual);
    case 3: return generateTriangle(qual);
    case 4: return generateFixedfm(qual);
  }
  return generateSine();
}

export function applyHarmonics(src, coefv) {
  const dst = new Float32Array(src.length);
  for (let i=0; i<coefv.length; i++) {
    const coef = coefv[i];
    if (!coef) continue;
    const step = i + 1;
    for (let srcp=0, dstp=0; dstp<dst.length; dstp++) {
      if (srcp >= src.length) srcp -= src.length;
      dst[dstp] += src[srcp] * coef;
      srcp += step;
    }
  }
  return dst;
}
