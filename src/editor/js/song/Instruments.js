/* Instruments.js
 * Model for the EAU-Text file containing the SDK's default instruments.
 * In general, the editor only uses EAU, not EAU-Text or MIDI. And we call the server to convert for us.
 * But standard instruments are a special case: We really want to preserve names and such.
 * TODO It would also be great if we can edit the instruments here in the editor. What would that take?
 */
 
import { Encoder } from "../Encoder.js";
 
export class Instruments {
  constructor(src) {
    this.instruments = []; // Packed, in the order we find them.
    this.tempo = 500;
    this.events = [];
    this.eventsBlockCount = 0;

    if (src instanceof Array) {
      for (const statement of src) {
        this.compileStatement(statement);
      }

    } else if (src) {
      const tokenizer = new EautTokenizer(src);
      for (;;) {
        const statement = tokenizer.statement();
        if (!statement) break;
        this.compileStatement(statement);
      }
    }
    delete this.eventsBlockCount;
  }
  
  compileStatement(statement) {
    switch (statement.kw) {
      case "chdr": this.instruments.push(new Instrument(statement)); break;
      case "tempo": this.tempo = +statement.params[0]; break;
      case "events": {
          if (this.eventsBlockCount++ === 1) {
            this.events.push({ type: "loop" });
          }
          this.compileEvents(statement);
        } break;
      default: throw new Error(`Unexpected EAUT statement ${JSON.stringify(statement.kw)}`);
    }
  }
  
  compileEvents(src) {
    for (const statement of src.body) {
      switch (statement.kw) {
        case "delay": this.compileDelay(statement); break;
        case "note": this.compileNote(statement); break;
        case "wheel": this.compileWheel(statement); break;
        default: throw new Error(`Unexpected statement ${JSON.stringify(statement.kw)} in events block.`);
      }
    }
  }
  
  compileDelay(statement) {
    const ms = +statement.params[0];
    this.events.push({ type: "delay", ms });
  }
  
  compileNote(statement) {
    if (statement.params.length !== 4) throw new Error(`Expected four params to 'note' (chid, noteid, velocity, durms), found ${statement.params.length}.`);
    const chid = +statement.params[0];
    const noteid = +statement.params[1];
    const velocity = +statement.params[2];
    const durms = +statement.params[3];
    this.events.push({ type: "note", chid, noteid, velocity, durms });
  }
  
  compileWheel(statement) {
    if (statement.params.length !== 2) throw new Error(`Expected two params to 'wheel' (chid, wheel), found ${statement.params.length}.`);
    const chid = +statement.params[0];
    const wheel = +statement.params[1];
    this.events.push({ type: "wheel", chid, wheel });
  }
  
  encodeEau(encoder) {
    encoder.raw("\0EAU");
    encoder.u32be(4);
    encoder.u16be(this.tempo);
    const looppp = encoder.c;
    encoder.u16be(0); // loopp
    for (const instrument of this.instruments) {
      this.encodeChdr(encoder, instrument);
    }
    if (this.events.length > 0) {
      let eventp = 0;
      encoder.raw("EVTS");
      encoder.pfxlen(4, () => {
        const evtsp = encoder.c;
        while (eventp < this.events.length) {
          const event = this.events[eventp++];
          if (event.type === "delay") {
            let ms = event.ms;
            while ((eventp < this.events.length) && (this.events[eventp].type === "delay")) {
              ms += this.events[eventp++].ms;
            }
            while (ms >= 4096) {
              encoder.u8(0x7f);
              ms -= 4096;
            }
            if (ms >= 0x40) {
              encoder.u8(0x40 | ((ms >> 6) - 1));
              ms &= 0x3f;
            }
            if (ms > 0) {
              encoder.u8(ms);
            }
          } else if (event.type === "note") {
            const dur = event.durms >> 2;
            encoder.u8(0x80 | (event.chid << 2) | (event.noteid >> 5));
            encoder.u8((event.noteid << 3) | (event.velocity >> 4));
            encoder.u8((event.velocity << 4) | (dur >> 8));
            encoder.u8(dur);
          } else if (event.type === "wheel") {
            encoder.u8(0xc0 | (event.chid << 2) | (event.wheel >> 8));
            encoder.u8(event.wheel);
          } else if (event.type === "loop") {
            const offset = encoder.c - evtsp;
            if (offset > 0xffff) throw new Error(`Loop point must be within 64 kB of events start.`);
            encoder.v[looppp] = offset >> 8;
            encoder.v[looppp + 1] = offset;
          }
        }
      });
    }
  }
  
  encodeChdr(encoder, instrument) {
    if ((instrument.chid < 0) || (instrument.chid >= 0x10)) return;
    encoder.raw("CHDR");
    encoder.pfxlen(4, () => {
      encoder.u8(instrument.chid);
      encoder.u8(instrument.trim);
      encoder.u8(instrument.pan);
      encoder.u8(instrument.mode);
      encoder.pfxlen(2, () => encoder.raw(instrument.modecfg));
      encoder.pfxlen(2, () => encoder.raw(instrument.post));
    });
  }
}

export class Instrument {
  constructor(statement) {
    if (statement.kw !== "chdr") throw new Error(`Expected 'chdr' statement.`);
    this.chid = -1;
    this.pid = -1;
    this.trim = 0x40;
    this.pan = 0x80;
    this.mode = 2;
    this.modecfg = null; // Shape depends on (mode).
    this.post = null; // {stageid,serial}[]
    for (const sub of statement.body) {
      switch (sub.kw) {
        case "name": this.setName(sub); break;
        case "chid": this.setChid(sub); break;
        case "pid": this.setPid(sub); break;
        case "trim": this.setTrim(sub); break;
        case "pan": this.setPan(sub); break;
        case "mode": this.setMode(sub); break;
        case "modecfg": this.setModecfg(sub); break;
        case "post": this.setPost(sub); break;
        default: throw new Error(`Unexpected keyword ${JSON.stringify(sub.kw)} in 'chdr' block.`);
      }
    }
    if (this.modecfg) {
      this.modecfg = this.compileModecfg(this.modecfg);
    } else this.modecfg = {};
    if (!this.post) this.post = [];
  }
  
  setName(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one string after 'name', found ${statement.params.length} params.`);
    this.name = JSON.parse(statement.params[0]);
  }
  
  setChid(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one integer after 'chid', found ${statement.params.length} params.`);
    this.chid = +statement.params[0];
    if (isNaN(this.chid) || (this.chid < 0) || (this.chid > 0xff)) throw new Error(`Invalid chid ${JSON.stringify(statement.params[0])}.`);
  }
  
  setPid(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one integer after 'pid', found ${statement.params.length} params.`);
    this.pid = +statement.params[0];
    if (isNaN(this.pid) || (this.pid < 0) || (this.pid > 0x1fffff)) throw new Error(`Invalid pid ${JSON.stringify(statement.params[0])}.`);
  }
  
  setTrim(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one integer after 'trim', found ${statement.params.length} params.`);
    this.trim = +statement.params[0];
    if (isNaN(this.trim) || (this.trim < 0) || (this.trim > 0xff)) throw new Error(`Invalid trim ${JSON.stringify(statement.params[0])}.`);
  }
  
  setPan(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one integer after 'pan', found ${statement.params.length} params.`);
    this.pan = +statement.params[0];
    if (isNaN(this.pan) || (this.pan < 0) || (this.pan > 0xff)) throw new Error(`Invalid pan ${JSON.stringify(statement.params[0])}.`);
  }
  
  setMode(statement) {
    if (statement.params.length !== 1) throw new Error(`Expected one identifier or integer after 'mode', found ${statement.params.length} params.`);
    switch (statement.params[0]) {
      case "noop": this.mode = 0; break;
      case "drum": this.mode = 1; break;
      case "fm": this.mode = 2; break;
      case "harsh": this.mode = 3; break;
      case "harm": this.mode = 4; break;
      default: this.mode = +statement.params[0];
    }
    if (isNaN(this.mode) || (this.mode < 0) || (this.mode > 0xff)) throw new Error(`Invalid mode ${JSON.stringify(statement.params[0])}.`);
  }
  
  setModecfg(statement) {
    if (this.modecfg) throw new Error(`Multiple modecfg.`);
    if (statement.params.length) throw new Error(`Unexpected params to 'modecfg'.`);
    this.modecfg = statement.body;
  }
  
  setPost(statement) {
    if (this.post) throw new Error(`Multiple post.`);
    if (statement.params.length) throw new Error(`Unexpected params to 'post'.`);
    this.post = statement.body;
  }
  
  compileModecfg(src) {
    switch (this.mode) {
      case 1: return this.compileModecfgDrum(src);
      case 2: case 3: case 4: return this.compileModecfgVoice(src);
      default: return this.compileModecfgGeneric(src);
    }
  }
  
  compileModecfgDrum(src) {
    const encoder = new Encoder();
    //const drums = [];
    for (const statement of src) {
      if (statement.kw !== "drum") throw new Error(`Unexpected statement ${JSON.stringify(statement.kw)} in drum modecfg. Only 'drum' is allowed.`);
      let name = "";
      let noteid = 0;
      let trimlo = 0x40;
      let trimhi = 0xc0;
      let pan = 0x80;
      let serial = null;
      for (const sub of statement.body) {
        switch (sub.kw) {
          case "name": name = JSON.parse(sub.params[0]); break;
          case "noteid": noteid = +sub.params[0]; break;
          case "trim": trimlo = +sub.params[0]; trimhi = +sub.params[1]; break;
          case "pan": pan = +sub.params[0]; break;
          case "serial": serial = new Instruments(sub.body); break;
          default: throw new Error(`Unexpected field ${JSON.stringify(sub.kw)} in drum body.`);
        }
      }
      encoder.u8(noteid);
      encoder.u8(trimlo);
      encoder.u8(trimhi);
      encoder.u8(pan);
      encoder.pfxlen(2, () => {
        if (serial) serial.encodeEau(encoder);
      });
      //drums.push({ name, noteid, trimlo, trimhi, pan, serial });
    }
    //return { drums };
    return encoder.finish();
  }
  
  // fm, harsh, harm. They have a bunch of fields in common, and no disagreement over the overlapping ones.
  compileModecfgVoice(src) {
    const dst = {};
    for (const statement of src.body) {
      switch (statement.kw) {
        case "rate": dst.rate = +statement.params[0]; break;
        case "absrate": dst.absrate = +statement.params[0]; break;
        case "range": dst.range = +statement.params[0]; break;
        case "levelenv": dst.levelenv = this.decodeEnv(statement.params); break;
        case "rangeenv": dst.rangeenv = this.decodeEnv(statement.params); break;
        case "pitchenv": dst.pitchenv = this.decodeEnv(statement.params); break;
        case "wheelrange": dst.wheelrange = +statement.params[0]; break;
        case "lforate": dst.lforate = +statement.params[0]; break;
        case "lfodepth": dst.lfodepth = +statement.params[0]; break;
        case "lfophase": dst.lfophase = +statement.params[0]; break;
        case "shape": dst.shape = this.compileShape(statement.params); break;
        case "harmonics": dst.harmonics = this.compileHarmonics(statement.params); break;
        default: throw new Error(`Unexpected statement ${JSON.stringify(statement.kw)} in modecfg.`);
      }
    }
    const encoder = new Encoder();
    switch (this.mode) {
      case 2: this.encodeModecfgFm(encoder, dst); break;
      case 3: this.encodeModecfgHarsh(encoder, dst); break;
      case 4: this.encodeModecfgHarm(encoder, dst); break;
    }
    return encoder.finish();
  }
  
  encodeModecfgFm(encoder, loose) {
    let fieldc = 0;
         if (loose.hasOwnProperty("lfophase"))   fieldc =10;
    else if (loose.hasOwnProperty("lfodepth"))   fieldc = 9;
    else if (loose.hasOwnProperty("lforate"))    fieldc = 8;
    else if (loose.hasOwnProperty("wheelrange")) fieldc = 7;
    else if (loose.hasOwnProperty("pitchenv"))   fieldc = 6;
    else if (loose.hasOwnProperty("rangeenv"))   fieldc = 5;
    else if (loose.hasOwnProperty("levelenv"))   fieldc = 4;
    else if (loose.hasOwnProperty("range"))      fieldc = 3;
    else if (loose.hasOwnProperty("absrange"))   fieldc = 2;
    else if (loose.hasOwnProperty("range"))      fieldc = 1;
    if (fieldc < 1) return;
    if (loose.absrate) encoder.u8_8(loose.absrate + 256);
    else encoder.u8_8(loose.rate);
    if (fieldc < 2) return;
    encoder.u8_8(loose.range);
    if (fieldc < 3) return;
    this.encodeEnv(encoder, loose.levelenv);
    if (fieldc < 4) return;
    this.encodeEnv(encoder, loose.rangeenv);
    if (fieldc < 5) return;
    this.encodeEnv(encoder, loose.pitchenv);
    if (fieldc < 6) return;
    encoder.u16(loose.wheelrange);
    if (fieldc < 7) return;
    encoder.u8_8(loose.lforate);
    if (fieldc < 8) return;
    encoder.u8(loose.lfodepth);
    if (fieldc < 9) return;
    encoder.u8(loose.lfophase);
  }
  
  encodeModecfgHarsh(encoder, loose) {
    let fieldc = 0;
         if (loose.hasOwnProperty("wheelrange")) fieldc = 4;
    else if (loose.hasOwnProperty("pitchenv"))   fieldc = 3;
    else if (loose.hasOwnProperty("levelenv"))   fieldc = 2;
    else if (loose.hasOwnProperty("shape"))      fieldc = 1;
    if (fieldc < 1) return;
    encoder.u8(loose.shape);
    if (fieldc < 2) return;
    this.encodeEnv(encoder, loose.levelenv);
    if (fieldc < 3) return;
    this.encodeEnv(encoder, loose.pitchenv);
    if (fieldc < 4) return;
    encoder.u16(loose.wheelrange);
  }
  
  encodeModecfgHarm(encoder, loose) {
    let fieldc = 0;
         if (loose.hasOwnProperty("wheelrange")) fieldc = 4;
    else if (loose.hasOwnProperty("pitchenv"))   fieldc = 3;
    else if (loose.hasOwnProperty("levelenv"))   fieldc = 2;
    else if (loose.hasOwnProperty("shape"))      fieldc = 1;
    if (fieldc < 1) return;
    encoder.u8(loose.shape);
    if (fieldc < 2) return;
    this.encodeEnv(encoder, loose.levelenv);
    if (fieldc < 3) return;
    this.encodeEnv(encoder, loose.pitchenv);
    if (fieldc < 4) return;
    encoder.u16(loose.wheelrange);
  }
  
  encodeEnv(dst, env) {
    let pointp = 0;
    
    // Initial value?
    if (env.points.length && !env.points[0].tlo && !env.points[0].thi) {
      if (env.points[0].vlo !== env.points[0].vhi) {
        ...
    if (pointp < env.points.length) {
      const point = env.points[pointp++];
      
    //TODO
  }
  
  decodeEnv(src) {
    const points = [];
    let srcp = 0;
    
    // Initial level?
    if ((srcp < src.length) && src[srcp].startsWith("=")) {
      points.push({
        vlo: this.envLo(src[srcp]),
        vhi: this.envHi(src[srcp]),
        tlo: 0,
        thi: 0,
      });
      srcp++;
    }
    
    for (;;) {
      if (srcp >= src.length) break;
      const t = src[srcp++];
      if (srcp >= src.length) throw new Error(`Expected envelope value.`);
      const v = src[srcp++];
      if (!t.startsWith("+")) throw new Error(`Expected envelope times with leading '+', found ${JSON.stringify(t)}`);
      if (!v.startsWith("=")) throw new Error(`Expected envelope values with leading '=', found ${JSON.stringify(v)}`);
      points.push({
        vlo: this.envLo(v),
        vhi: this.envHi(v),
        tlo: this.envLo(t),
        thi: this.envHi(t),
      });
    }
    return { points };
  }
  
  envLo(src) {
    const sepp = src.indexOf("..");
    if (sepp < 0) return +src;
    return +src.substring(0, sepp);
  }
  
  envHi(src) {
    const sepp = src.indexOf("..");
    if (sepp < 0) return +src;
    return +src.substring(sepp + 2);
  }
  
  compileShape(src) {
    switch (src[0]) {
      case "sine": return 0;
      case "square": return 1;
      case "saw": return 2;
      case "triangle": return 3;
    }
    return +src[0];
  }
  
  compileHarmonics(src) {
    return src.map(v => +v);
  }
  
  compileModecfgGeneric(src) {
    let digits = "";
    for (const param of src.params) {
      if (param.startsWith("0x") || param.startsWith("0X")) digits += param.substring(2);
      else digits += param;
    }
    if (digits.length & 1) throw new Error(`Expected hex dump of even length for generic modecfg.`);
    const serial = new Uint8Array(digits.length >> 1);
    for (let dstp=0, srcp=0; srcp<digits.length; dstp++, srcp+=2) {
      const v = parseInt(digits.substring(srcp, srcp + 2), 16);
      if (isNaN(v)) throw new Error(`Invalid digits ${JSON.stringify(digits.substring(srcp, srcp + 2))} in hex dump.`);
      serial[dstp] = v;
    }
    return serial;
  }
}

export class EautTokenizer {
  constructor(src) {
    this.src = src;
    this.srcp = 0;
  }
  
  /* Consumes and composes one EAU statement. One of:
   *   KEYWORD [PARAMS...] { ...STATEMENTS... }
   *   KEYWORD [PARAMS...] ;
   * Or null at EOF.
   * (permitTerminator) should be false calling from outside; if true, "}" is a valid statement.
   * Returns Statement: {
   *   kw: string
   *   params: string[]
   *   body: Statement[]
   * }
   */
  statement(permitTerminator) {
  
    const kw = this.next();
    if (!kw) return null;
    if (permitTerminator && (kw === "}")) return kw;
    if (!this.isident(kw.charCodeAt(0))) throw new Error(`${this.lineno()}: Expected keyword before ${JSON.stringify(kw)}`);
    
    const params = [];
    const body = [];
    let haveBody = false;
    for (;;) {
      const token = this.next();
      if (token === "{") {
        haveBody = true;
        break;
      }
      if (token === ";") {
        break;
      }
      params.push(token);
    }
    
    if (haveBody) {
      const lineno0 = this.lineno();
      for (;;) {
        const sub = this.statement(true);
        if (sub === "}") break;
        if (!sub) throw new Error(`${lineno0}: Unclosed block.`);
        body.push(sub);
      }
    }
    
    return { kw, params, body };
  }
  
  /* Skips whitespace and comments.
   * Returns empty string at EOF.
   */
  next() {
    for (;;) {
    
      // EOF?
      if (this.srcp >= this.src.length) return "";
      
      // Peek next char.
      const ch = this.src.charCodeAt(this.srcp);
      
      // Line comment?
      if (ch === 0x23) {
        const nlp = this.src.indexOf("\n", this.srcp);
        if (nlp <= this.srcp) this.srcp = this.src.length;
        else this.srcp = nlp + 1;
        continue;
      }
      
      // Whitespace?
      if (ch <= 0x20) {
        this.srcp++;
        continue;
      }
      
      // Quoted string? It's ok to choke on embedded quotes, our spec says not to do that.
      if (ch === 0x22) {
        const closep = this.src.indexOf('"', this.srcp + 1);
        if (closep < 0) throw new Error(`${this.lineno()}: Unclosed string in EAU-Text`);
        const token = this.src.substring(this.srcp, closep + 1);
        this.srcp = closep + 1;
        return token;
      }
      
      // Identifier or integer?
      if (this.isident(ch)) {
        const startp = this.srcp++;
        while ((this.srcp < this.src.length) && this.isident(this.src.charCodeAt(this.srcp))) this.srcp++;
        return this.src.substring(startp, this.srcp);
      }
      
      // Everything else is one character of punctuation.
      this.srcp++;
      return this.src[this.srcp-1];
    }
  }
  
  isident(ch) {
    if ((ch >= 0x30) && (ch <= 0x39)) return true;
    if ((ch >= 0x41) && (ch <= 0x5a)) return true;
    if ((ch >= 0x61) && (ch <= 0x7a)) return true;
    if (ch === 0x5f) return true;
    return false;
  }
  
  lineno() {
    let lineno = 1;
    for (let i=this.srcp; i-->0; ) if (this.src.charCodeAt(i) === 0x0a) lineno++;
    return lineno;
  }
}
