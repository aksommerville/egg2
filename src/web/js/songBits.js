/* songBits.js
 * Standalone utilities shared around the synthesizer.
 */
 
/* Rate table, Hz by MIDI note id.
 */
export const eauNotev = [];
export function eauNotevRequire() {
  if (eauNotev.length) return;
  for (let i=0; i<128; i++) eauNotev.push(440 * Math.pow(2, (i-0x45) / 12));
}
 
/* Duration of EAU file, based only on delays events.
 */
export function calculateEauDuration(src, rate) {
  for (let srcp=0; srcp<src.length; ) {
    const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
    const chunklen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
    if ((chunklen < 0) || (srcp > src.length - chunklen)) return 0;
    if (chunkid === 0x45565453) return calculateEauEventsDuration(new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen), rate);
    srcp += chunklen;
  }
  return 0;
}
export function calculateEauEventsDuration(src, rate) {
  let durms = 0;
  for (let srcp=0; srcp<src.length; ) {
    const lead = src[srcp++];
    switch (lead & 0xc0) {
      case 0x00: durms += lead; break;
      case 0x40: durms += ((lead & 0x3f) + 1) << 6; break;
      case 0x80: srcp += 3; break;
      case 0xc0: srcp += 1; break;
    }
  }
  return (durms * rate) / 1000;
}

/* Structured decoder for EAU bits.
 * In C I do this kind of ad-hoc with macros, but JS calls for a more structured solution.
 */
export class EauDecoder {
  constructor(v) {
    this.v = v || [];
    this.p = 0;
  }
  u8(fallback) {
    if (this.p >= this.v.length) return fallback;
    return this.v[this.p++];
  }
  u16(fallback) {
    if (this.p >= this.v.length) return fallback;
    if (this.p > this.v.length - 2) throw new Error("Malformed EAU");
    const v = (this.v[this.p] << 8) | this.v[this.p+1];
    this.p += 2;
    return v;
  }
  // (name) is "level", "pitch", or "range", for generating the default and applying bias and scale.
  // Defaulted envelopes will have (flag&0x80).
  env(name) {
    if (this.p >= this.v.length) return this.defaultEnv(name);
    if ((this.p <= this.v.length - 2) && !this.v[this.p] && !this.v[this.p+1]) {
      this.p += 2;
      return this.defaultEnv(name);
    }
    const flags = this.v[this.p++];
    let initlo=0, inithi=0;
    if (flags & 0x01) { // Initials.
      if (this.p > this.v.length - 2) throw new Error("Malformed EAU");
      initlo = (this.v[this.p] << 8) | this.v[this.p+1];
      this.p += 2;
      if (flags & 0x02) { // Velocity.
        if (this.p > this.v.length - 2) throw new Error("Malformed EAU");
        inithi = (this.v[this.p] << 8) | this.v[this.p+1];
        this.p += 2;
      } else {
        inithi = initlo;
      }
    }
    if (this.p >= this.v.length) throw new Error("Malformed EAU");
    const susp_ptc = this.v[this.p++];
    let susp = susp_ptc >> 4;
    let pointc = susp_ptc & 15;
    const ptlen = (flags & 0x02) ? 8 : 4;
    if (this.p > this.v.length - ptlen * pointc) throw new Error("Malformed EAU");
    const points = []; // {tlo,vlo,thi,vhi}
    for (let i=0; i<pointc; i++) {
      const tlo = ((this.v[this.p] << 8) | this.v[this.p+1]) / 1000;
      this.p += 2;
      const vlo = (this.v[this.p] << 8) | this.v[this.p+1];
      this.p += 2;
      let thi=tlo, vhi=vlo;
      if (flags & 0x02) { // Velocity.
        thi = ((this.v[this.p] << 8) | this.v[this.p+1]) / 1000;
        this.p += 2;
        vhi = (this.v[this.p] << 8) | this.v[this.p+1];
        this.p += 2;
      }
      points.push({ tlo, vlo, thi, vhi });
      if ((flags & 0x04) && (i === susp)) {
        points.push({ tlo: 0, vlo, thi: 0, vhi });
      }
    }
    susp++;
    switch (name) {
      case "level": initlo /= 65535; inithi /= 65535; for (const pt of points) { pt.vlo /= 65535; pt.vhi /= 65535; } break;
      case "range": initlo /= 65535; inithi /= 65535; for (const pt of points) { pt.vlo /= 65535; pt.vhi /= 65535; } break; // will be modified further by caller, with the range scale.
      case "pitch": {
          if (flags & 0x01) {
            initlo = (initlo - 0x8000);
            inithi = (inithi - 0x8000);
          } else {
            initlo = inithi = 0;
          }
          for (const pt of points) {
            pt.vlo = (pt.vlo - 0x8000);
            pt.vhi = (pt.vhi - 0x8000);
          }
        } break;
    }
    return { flags, initlo, inithi, susp, points };
  }
  defaultEnv(name) {
    let init = 0; // For a constant, just set this and pass thru.
    switch (name) {
      case "level": return { // The default level envelope is complex and opinionated. Match the native implementation (synth_env.c:synth_env_default_level()).
          flags: 0x86, // Default|Velocity|Sustain.
          susp: 1,
          initlo: 0,
          inithi: 0,
          points: [
            { tlo:0.032, vlo:0.250, thi:0.016, vhi:1.000 },
            { tlo:0.048, vlo:0.188, thi:0.048, vhi:0.313 },
            { tlo:0.000, vlo:0.188, thi:0.000, vhi:0.313 }, // sustain
            { tlo:0.128, vlo:0.000, thi:0.256, vhi:0.000 },
          ],
        };
      case "range": init = 1; break;
      case "pitch": init = 0; break;
    }
    return { flags: 0x80, initlo: init, inithi: init, points: [] };
  }
}

/* Apply envelope.
 * Returns [{t,v}...] at least one.
 */
export function eauEnvApply(env, when, velocity, durs) {
  const durs0=durs;
  const dst = [];
  let sus = (env.flags & 0x04) ? env.susp : 99;
  if (!(env.flags & 0x02) || (velocity <= 0)) {
    dst.push({ t: when, v: env.initlo });
    for (const pt of env.points) {
      let t = pt.tlo;
      if (!sus--) t = Math.max(0, durs);
      t += when;
      dst.push({ t, v: pt.vlo });
      when = t;
    }
    
  } else if (velocity >= 1) {
    dst.push({ t: when, v: env.inithi });
    for (const pt of env.points) {
      let t = pt.thi;
      if (!sus--) t = Math.max(0, durs);
      t += when;
      dst.push({ t, v: pt.vhi });
      when = t;
    }
    
  } else {
    const whi = velocity;
    const wlo = 1 - whi;
    dst.push({ t: when, v: env.initlo * wlo + env.inithi * whi });
    for (const pt of env.points) {
      let t = pt.tlo * wlo + pt.thi * whi;
      durs -= t;
      if (!sus--) t = Math.max(0, durs);
      t += when;
      dst.push({ t, v: pt.vlo * wlo + pt.vhi * whi });
      when = t;
    }
  }
  return dst;
}
