/* EauDecoder.js
 * Structured support for reading EAU bits like modecfg and post stages.
 */
 
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
   *   susp: <0 if no sustain
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
      susp: -1,
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
    if (this.srcp > this.src.length - ptlen) {
      this.srcp = this.src.length;
      return this.defaultEnv(usage);
    }
    if (flags & 0x04) { // Sustain
      env.susp = susp_ptc >> 4;
      if (env.susp >= ptc) env.susp = -1;
      else env.susp++; // Models have an initial point; the encoded format does not.
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
          susp: -1,
          lo: [{ t: 0, v: 0xffff }],
          isDefault: true,
        };
      case "pitch": return {
          usage: "pitch",
          susp: -1,
          lo: [{ t: 0, v: 0x8000 }],
          isDefault: true,
        };
    }
    return {
      usage,
      susp: -1,
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
