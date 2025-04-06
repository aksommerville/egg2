/* eautSong.js
 * Encode and decode Song objects to/from EAU Text.
 * Unlike our peers EAU and MIDI, our serial side is typed string (they use Uint8Array).
 *
 * Decoding is also a little weird, since we mostly just convert to EAU -- channel.payload and channel.post are the interesting parts.
 * It feels weird to do that conversion, as opposed to populating some live model.
 * But that's how we do it. UI controllers work from the EAU serial for channel headers.
 */
 
import { Song, SongChannel, SongEvent } from "./Song.js";
import { eauSongEncode } from "./eauSong.js";
import { Encoder } from "../Encoder.js";

/* Encode.
 ******************************************************************************/

export function eautSongEncode(song) {
  console.log(`TODO eautSongEncode`);
  return new Uint8Array(0); // TODO
}

/* Structured decoder for breaking the text apart generically.
 *******************************************************************************/
 
class EautDecoder {
  constructor(src, srcp, lineno0) {
    if (typeof(src) !== "string") throw new Error(`Inappropriate input to EautDecoder`);
    this.src = src;
    this.srcp = srcp || 0;
    this.lineno = (lineno0 || 0) + 1;
  }
  
  /* Returns one of:
   *   { lineno, comment: string } Includes the leading "#".
   *   { lineno, tokens: string[], body: string, lineno0: number }
   *   null: EOF
   * Comments do return as statements, one line at a time. Because we use those eg for channel names.
   * If there's a true (body), you can make a new EautDecoder using (body) and (lineno0) to read it.
   */
  nextStatement() {
    this.skipWhitespace();
    if (this.srcp >= this.src.length) return null;
    
    // No matter what kind of statement it is, read the first line linewise.
    const lineno = this.lineno;
    let nlp = this.src.indexOf("\n", this.srcp);
    if (nlp < 0) nlp = this.src.length;
    let line = this.src.substring(this.srcp, nlp).trim();
    this.srcp = nlp;
    
    // Comment?
    if (line.startsWith("#")) {
      return { lineno, comment: line };
    }
    
    // Is a body present?
    let body = "";
    let lineno0 = lineno;
    if (line.endsWith("{")) {
      line = line.substring(0, line.length - 1).trim();
      if ((this.srcp < this.src.length) && (this.src[this.srcp] === "\n")) {
        this.srcp++;
        this.lineno++;
      }
      const bodyp = this.srcp;
      for (;;) {
        const beforep = this.srcp;
        const next = this.nextStatement();
        if (!next) throw new Error(`${this.lineno}: Unmatched bracket`);
        if (!next.tokens) continue;
        if (next.tokens.length !== 1) continue;
        if (next.tokens[0] !== "}") continue;
        body = this.src.substring(bodyp, beforep);
        break;
      }
    }
    
    return { lineno, tokens: line.split(/\s+/g).filter(v => v), body, lineno0 };
  }
  
  // Advance (srcp) past whitespace. Comments don't count.
  skipWhitespace() {
    while (this.srcp < this.src.length) {
      const ch = this.src.charCodeAt(this.srcp);
      if (ch > 0x20) return;
      this.srcp++;
      if (ch === 0x0a) this.lineno++;
    }
  }
}

/* Decode.
 *****************************************************************************/
 
export function eautSongDecode(song, src, lineno0) {
  song.format = "eaut";
  const decoder = new EautDecoder(src, 0, lineno0);
  let recentComment = "";
  let now = 0;
  for (let statement; statement=decoder.nextStatement(); ) {
  
    if (statement.comment) {
      const match = statement.comment.match(/^#\s*---\s+(\d+)\.\.(\d+):\s+([a-zA-Z_+(), -]+)\s+----*$/);
      if (match) {
        song.rangeNames.push({
          lo: +match[1],
          hi: +match[2],
          name: match[3],
        });
        const r=song.rangeNames[song.rangeNames.length-1];//XXX
        console.log(`${statement.lineno}:RANGE: ${r.lo} .. ${r.hi} = ${JSON.stringify(r.name)}`);
        recentComment = "";
      } else {
        recentComment = statement.comment;
      }
      continue;
    }
    
    const kw = statement.tokens?.[0] || "";
    if (kw === "globals") {
      eautSongDecodeGlobals(song, statement);
    } else if (kw === "events") {
      now = eautSongDecodeEvents(song, statement, now);
    } else {
      const name = channelNameFromEauTextComment(recentComment);
      eautSongDecodeChannel(song, name, statement);
    }
    recentComment = "";
  }
}

function eautSongDecodeGlobals(song, statement) {
  const decoder = new EautDecoder(statement.body, 0, statement.lineno0);
  for (let st; st=decoder.nextStatement(); ) {
    if (st.comment) continue;
    switch (st.tokens[0]) {
      case "tempo": {
          song.tempo = +st.tokens[1];
          if (isNaN(song.tempo) || (song.tempo < 1) || (song.tempo > 0xffff)) {
            throw new Error(`${st.lineno}: Invalid tempo in EAUT song.`);
          }
        } break;
      default: throw new Error(`${st.lineno}: Unexpected command ${JSON.stringify(st.tokens[0])} in EAUT globals block.`);
    }
  }
}

// Returns new value for (now), ms.
function eautSongDecodeEvents(song, statement, now) {
  const decoder = new EautDecoder(statement.body, 0, statement.lineno0);
  for (let st; st=decoder.nextStatement(); ) {
    if (st.comment) continue;
    switch (st.tokens[0]) {
    
      case "delay": {
          const ms = +st.tokens[1];
          if (isNaN(ms) || (ms < 1)) throw new Error(`${st.lineno}: Invalid delay`);
          now += ms;
        } break;
        
      case "note": {
          const event = new SongEvent();
          event.time = now;
          event.type = "n";
          event.chid = +st.tokens[1];
          // chid>15 are allowed for Channel Headers (we depend on it), but they are very illegal for events.
          if (isNaN(event.chid) || (event.chid < 0) || (event.chid > 15)) throw new Error(`${st.lineno}: Invalid chid`);
          event.noteid = +st.tokens[2];
          if (isNaN(event.noteid) || (event.noteid < 0) || (event.noteid > 0x7f)) throw new Error(`${st.lineno}: Invalid noteid`);
          event.velocity = +st.tokens[3];
          if (isNaN(event.velocity) || (event.velocity < 0) || (event.velocity > 15)) throw new Error(`${st.lineno}: Invalid velocity`);
          event.durms = +st.tokens[4];
          if (isNaN(event.durms) || (event.durms < 0)) throw new Error(`${st.lineno}: Invalid duration`);
          song.events.push(event);
        } break;
        
      case "wheel": {
          const event = new SongEvent();
          event.time = now;
          event.type = "w";
          event.chid = +st.tokens[1];
          if (isNaN(event.chid) || (event.chid < 0) || (event.chid > 15)) throw new Error(`${st.lineno}: Invalid chid`);
          event.v = +st.tokens[2];
          if (isNaN(event.v) || (event.v < 0) || (event.v > 0xff)) throw new Error(`${st.lineno}: Invalid wheel value`);
          song.events.push(event);
        } break;
        
      default: throw new Error(`${st.lineno}: Unexpected command ${JSON.stringify(st.tokens[0])} in EAUT events block.`);
    }
  }
  return now;
}

function channelNameFromEauTextComment(src) {
  return src.match(/^# *([0-9a-zA-Z ,+()-]+)$/)?.[1] || "";
}

/* Decode channel header.
 ************************************************************************************************/

function eautSongDecodeChannel(song, name, statement) {

  const channel = new SongChannel();
  channel.name = name || "";
  channel.chid = +statement.tokens[0];
  if (isNaN(channel.chid) || (channel.chid < 0)) throw new Error(`${statement.lineno}: Invalid channel id`);
  channel.trim = +statement.tokens[1];
  if (isNaN(channel.trim) || (channel.trim < 0) || (channel.trim > 0xff)) throw new Error(`${statement.lineno}: Invalid trim`);
  channel.pan = +statement.tokens[2];
  if (isNaN(channel.pan) || (channel.pan < 0) || (channel.pan > 0xff)) throw new Error(`${statement.lineno}: Invalid pan`);
  switch (statement.tokens[3]?.toLowerCase()) {
    case "noop": channel.mode = 0; break;
    case "drum": channel.mode = 1; break;
    case "fm": channel.mode = 2; break;
    case "sub": channel.mode = 3; break;
    default: {
        channel.mode = +statement.tokens[3];
        if (isNaN(channel.mode) || (channel.mode < 0) || (channel.mode > 0xff)) throw new Error(`${statement.lineno}: Invalid channel mode`);
      }
  }
  if (song.channels[channel.chid]) {
    throw new Error(`${statement.lineno}: Duplicate channel ${channel.chid} (${channel.getDisplayName()} and ${song.channels[channel.chid].getDisplayName()})`);
  }
  song.channels[channel.chid] = channel;

  let fldseq=0, gotpost=false, gotmodecfg=false;
  const encoder = new Encoder();
  const decoder = new EautDecoder(statement.body, 0, statement.lineno0);
  for (let st; st=decoder.nextStatement(); ) {
    if (st.comment) continue;
    
    if (gotpost) {
      throw new Error(`${st.lineno}: Nothing allowed after 'post'`);
    }
    
    if (st.tokens[0] === "post") {
      eautSongDecodePost(channel, st);
      gotpost = true;
      continue;
    }
    
    if (gotmodecfg) {
      throw new Error(`${st.lineno}: Nothing but 'post' allowed after 'modecfg'`);
    }
    
    if (st.tokens[0] === "modecfg") {
      if (fldseq) throw new Error(`${st.lineno}: Can't mix "modecfg" with mode-specific fields.`);
      channel.payload = decodeHexdump(st.tokens.slice(1).join(""));
      gotmodecfg = true;
      continue;
    }
    
    switch (channel.mode) {
      case 0: eautSongDecodeChannelField_noop(encoder, st, fldseq++); break;
      case 1: eautSongDecodeChannelField_drum(encoder, st, fldseq++); break;
      case 2: eautSongDecodeChannelField_fm(encoder, st, fldseq++); break;
      case 3: eautSongDecodeChannelField_sub(encoder, st, fldseq++); break;
      default: eautSongDecodeChannelField_generic(encoder, st, fldseq++); break;
    }
  }
  if (fldseq) {
    channel.payload = encoder.finish();
  }
}

function eautSongDecodeChannelField_generic(encoder, statement, fldseq) {
  throw new Error(`${statmeent.lineno}: Unknown channel mode may only use "modecfg"`);
}

function eautSongDecodeChannelField_noop(encoder, statement, fldseq) {
  // Allow anything, don't even examine it. And emit nothing.
}

function eautSongDecodeChannelField_drum(encoder, statement, fldseq) {
  if (statement.tokens[0] === "note") {
    const noteid = +statement.tokens[1];
    if (isNaN(noteid) || (noteid < 0) || (noteid > 0xff)) throw new Error(`${statement.lineno}: Invalid note id`);
    let trimlo=0x80, trimhi=0xff, pan=0x80;
    if (statement.tokens.length >= 3) {
      trimlo = +statement.tokens[2];
      if (isNaN(trimlo) || (trimlo < 0) || (trimlo > 0xff)) throw new Error(`${statement.lineno}: Invalid trim`);
      trimhi = trimlo;
      if (statement.tokens.length >= 4) {
        trimhi = +statement.tokens[3];
        if (isNaN(trimhi) || (trimhi < 0) || (trimhi > 0xff)) throw new Error(`${statement.lineno}: Invalid trim`);
        if (statement.tokens.length >= 5) {
          pan = +statement.tokens[4];
          if (isNaN(pan) || (pan < 0) || (pan > 0xff)) throw new Error(`${statement.lineno}: Invalid pan`);
        }
      }
    }
    encoder.u8(noteid);
    encoder.u8(trimlo);
    encoder.u8(trimhi);
    encoder.u8(pan);
    const subsong = new Song();
    eautSongDecode(subsong, statement.body, statement.lineno0);
    const eau = eauSongEncode(subsong);
    if (eau.length > 0xffff) throw new Error(`${statement.lineno}: Inner song must be <64k, found ${eau.length} bytes`);
    encoder.u16be(eau.length);
    encoder.raw(eau);
    return;
  }
  
  throw new Error(`${statement.lineno}: Unexpected command ${JSON.stringify(statement.tokens[0])} in drum config.`);
}

function eautSongDecodeChannelField_fm(encoder, statement, fldseq) {
  switch (fldseq) {
    case 0: return eautChhdrfld_env(encoder, statement, "level");
    case 1: return eautChhdrfld_wave(encoder, statement, "wave");
    case 2: return eautChhdrfld_env(encoder, statement, "pitchenv");
    case 3: return eautChhdrfld_u16(encoder, statement, "wheel");
    case 4: return eautChhdrfld_u8_8(encoder, statement, "rate");
    case 5: return eautChhdrfld_u8_8(encoder, statement, "range");
    case 6: return eautChhdrfld_env(encoder, statement, "rangeenv");
    case 7: return eautChhdrfld_u8_8(encoder, statement, "rangelforate");
    case 8: return eautChhdrfld_u0_8(encoder, statement, "rangelfodepth");
    default: throw new Error(`${statement.lineno}: Unexpected command ${JSON.stringify(statement.tokens[0])} in fm config.`);
  }
}

function eautSongDecodeChannelField_sub(encoder, statement, fldseq) {
  switch (fldseq) {
    case 0: return this.eautChhdrfld_env(encoder, statement, "level");
    case 1: return this.eautChhdrfld_u16_optu16(encoder, statement, "width");
    case 2: return this.eautChhdrfld_u8(encoder, statement, "stagec");
    case 3: return this.eautChhdrfld_u8_8(encoder, statement, "gain");
    default: throw new Error(`${statement.lineno}: Unexpected command ${JSON.stringify(statement.tokens[0])} in sub config.`);
  }
}

function eautChhdrfld_env(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  const v = []; // {lo,hi,sustain}, alternating LEVEL, TIME, LEVEL, ...
  for (let i=1; i<statement.tokens.length; i++) {
    let src = statement.tokens[i];
    let sustain = false;
    if (src.endsWith("*")) {
      sustain = true;
      src = src.substring(0, src.length - 1);
    }
    const sepp = src.indexOf("..");
    let lo, hi;
    if (sepp >= 0) {
      lo = +src.substring(0, sepp);
      hi = +src.substring(sepp + 2);
    } else {
      lo = hi = +src;
    }
    if (isNaN(lo) || isNaN(hi) || (lo < 0) || (lo > 0xffff) || (hi < 0) || (hi > 0xffff)) {
      throw new Error(`${statement.lineno}: Invalid token in envelope`);
    }
    v.push({ lo, hi, sustain });
  }
  if (!(v.length & 1)) throw new Error(`${statement.lineno}: Envelope requires an odd count of values (LEVEL[, TIME, LEVEL[, ...]])`);
  
  let susp = -1;
  let velocity = false;
  let initials = false;
  if (v[0].lo || v[0].hi) initials = true;
  for (let i=v.length; i-->0; ) {
    if (v[i].sustain) {
      if (susp >= 0) throw new Error(`${statement.lineno}: Multiple sustain points`);
      if (i & 1) throw new Error(`${statement.lineno}: '*' can only appear on levels, you have it on a time.`);
      if (!i) throw new Error(`${statement.lineno}: Envelope initial value is not sustainable.`);
      susp = (i - 1) >> 1;
    }
    if (v[i].lo != v[i].hi) velocity = true;
  }
  const ptc = (v.length - 1) >> 1;
  if ((ptc > 16) || ((susp >= 0) && (ptc > 15))) throw new Error(`${statement.lineno}: Too many points, limit 16 without sustain or 15 with.`);
  
  encoder.u8(
    (velocity ? 0x01 : 0) |
    (initials ? 0x02 : 0) |
    ((susp >= 0) ? 0x04 : 0) |
  0);
  if (initials) {
    encoder.u16be(v[0].lo);
    if (velocity) {
      encoder.u16be(v[0].hi);
    }
  }
  if (susp >= 0) {
    encoder.u8(susp);
  }
  encoder.u8(ptc);
  for (let i=1; i<v.length; i+=2) {
    encoder.u16be(v[i].lo);
    encoder.u16be(v[i+1].lo);
    if (velocity) {
      encoder.u16be(v[i].hi);
      encoder.u16be(v[i+1].hi);
    }
  }
}

function eautChhdrfld_wave(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  let tp = 1;
  let shape = statement.tokens[tp++] || "0";
  switch (shape) {
    case "sine": shape = 0; break;
    case "square": shape = 1; break;
    case "saw": shape = 2; break;
    case "triangle": shape = 3; break;
    case "fixedfm": shape = 4; break;
    default: {
        shape = +shape;
        if (isNaN(shape) || (shape < 0) || (shape > 0xff)) throw new Error(`${statement.lineno}: Invalid shape`);
      }
  }
  let qualifier = 0;
  if ((tp < statement.tokens.length) && statement.tokens[tp].startsWith("+")) {
    qualifier = +statement.tokens[tp++].substring(1);
    if (isNaN(qualifier) || (qualifier < 0) || (qualifier > 0xff)) throw new Error(`${statement.lineno}: Invalid qualifier`);
  }
  encoder.u8(shape);
  encoder.u8(qualifier);
  const harmc = statement.tokens.length - tp;
  if (harmc > 0xff) throw new Error(`${statement.lineno}: Too many wave harmonics`);
  encoder.u8(harmc);
  for (; tp<statement.tokens.length; tp++) {
    const v = +statement.tokens[tp];
    if (isNaN(v) || (v < 0) || (v > 0xffff)) throw new Error(`${statement.lineno}: Invalid harmonic`);
    encoder.u16be(v);
  }
}

function eautChhdrfld_u8(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  if (statement.tokens.length !== 2) throw new Error(`${statement.lineno}: Expected ${name}`);
  let v = +statement.tokens[1];
  if (isNaN(v) || (v < 0) || (v > 0xff)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
  encoder.u8(v);
}

function eautChhdrfld_u16(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  if (statement.tokens.length !== 2) throw new Error(`${statement.lineno}: Expected ${name}`);
  let v = +statement.tokens[1];
  if (isNaN(v) || (v < 0) || (v > 0xffff)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
  encoder.u16be(v);
}

function eautChhdrfld_u16_optu16(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  const lo = +statement.tokens[1];
  if (isNaN(lo) || (lo < 0) || (lo > 0xffff)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
  encoder.u16be(lo);
  if (statement.tokens.length >= 3) {
    const hi = +statement.tokens[2];
    if (isNaN(hi) || (hi < 0) || (hi > 0xffff)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
    encoder.u16be(hi);
  } else {
    encoder.u16be(lo);
  }
}

function eautChhdrfld_u0_8(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  if (statement.tokens.length !== 2) throw new Error(`${statement.lineno}: Expected ${name}`);
  let v = +statement.tokens[1];
  if (isNaN(v) || (v < 0) || (v > 1)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
  v = Math.floor(v * 255);
  if (v > 0xff) v = 0xff;
  encoder.u8(v);
}

function eautChhdrfld_u8_8(encoder, statement, name) {
  if (statement.tokens[0] !== name) throw new Error(`${statement.lineno}: ${JSON.stringify(statement.tokens[0])} not allowed here, expected ${JSON.stringify(name)}`);
  if (statement.tokens.length !== 2) throw new Error(`${statement.lineno}: Expected ${name}`);
  let v = +statement.tokens[1];
  if (isNaN(v) || (v < 0) || (v > 256)) throw new Error(`${statement.lineno}: Illegal value for ${name}`);
  v = Math.floor(v * 256);
  if (v > 0xffff) v = 0xffff;
  encoder.u16be(v);
}

/* Decode post.
 *************************************************************************************/

function eautSongDecodePost(channel, statement) {
  const encoder = new Encoder();
  const decoder = new EautDecoder(statement.body, 0, statement.lineno0);
  for (let st; st=decoder.nextStatement(); ) {
    if (st.comment) continue;
    switch (st.tokens[0]) {
      case "gain": eautSongDecodePost_gain(encoder, st); break;
      case "delay": eautSongDecodePost_delay(encoder, st); break;
      case "lopass": eautSongDecodePost_lopass(encoder, st); break;
      case "hipass": eautSongDecodePost_hipass(encoder, st); break;
      case "bpass": eautSongDecodePost_bpass(encoder, st); break;
      case "notch": eautSongDecodePost_notch(encoder, st); break;
      case "waveshaper": eautSongDecodePost_waveshaper(encoder, st); break;
      case "tremolo": eautSongDecodePost_tremolo(encoder, st); break;
      default: eautSongDecodePost_generic(encoder, st); break;
    }
  }
  channel.post = encoder.finish();
}

function eautSongDecodePost_gain(encoder, statement) {
  encoder.u8(0x01);
  let gain = +statement.tokens[1];
  if (isNaN(gain) || (gain < 0) || (gain > 256)) throw new Error(`${statement.lineno}: Invalid gain`);
  gain = Math.floor(gain * 256);
  if (gain > 0xffff) gain = 0xffff;
  if (statement.tokens.length >= 3) {
    let clip = +statement.tokens[2];
    if (isNaN(clip) || (clip < 0) || (clip > 1)) throw new Error(`${statement.lineno}: Invalid clip`);
    clip = Math.floor(clip * 255);
    if (clip > 0xff) clip = 0xff;
    encoder.u8(3);
    encoder.u16be(gain);
    encoder.u8(clip);
  } else {
    encoder.u8(2);
    encoder.u16be(gain);
  }
}

function eautSongDecodePost_delay(encoder, statement) {
  encoder.u8(0x02);
  encoder.u8(6);
  encFixed(encoder, statement.tokens[1], "u8.8", "period", statement.lineno);
  encFixed(encoder, statement.tokens[2], "u0.8", "dry", statement.lineno);
  encFixed(encoder, statement.tokens[3], "u0.8", "wet", statement.lineno);
  encFixed(encoder, statement.tokens[4], "u0.8", "store", statement.lineno);
  encFixed(encoder, statement.tokens[5], "u0.8", "feedback", statement.lineno);
}

function eautSongDecodePost_lopass(encoder, statement) {
  encoder.u8(0x03);
  encoder.u8(2);
  encFixed(encoder, statement.tokens[1], "u16", "hz", statement.lineno);
}

function eautSongDecodePost_hipass(encoder, statement) {
  encoder.u8(0x04);
  encoder.u8(2);
  encFixed(encoder, statement.tokens[1], "u16", "hz", statement.lineno);
}

function eautSongDecodePost_bpass(encoder, statement) {
  encoder.u8(0x05);
  encoder.u8(4);
  encFixed(encoder, statement.tokens[1], "u16", "hz", statement.lineno);
  encFixed(encoder, statement.tokens[2], "u16", "width", statement.lineno);
}

function eautSongDecodePost_notch(encoder, statement) {
  encoder.u8(0x06);
  encoder.u8(4);
  encFixed(encoder, statement.tokens[1], "u16", "hz", statement.lineno);
  encFixed(encoder, statement.tokens[2], "u16", "width", statement.lineno);
}

function eautSongDecodePost_waveshaper(encoder, statement) {
  encoder.u8(0x07);
  const lenp = encoder.c;
  encoder.u8(0);
  for (let i=1; i<statement.tokens.length; i++) {
    encFixed(encoder, statement.tokens[i], "u16", "coef", statement.lineno);
  }
  const len = encoder.c - lenp - 1;
  if (len > 0xff) throw new Error(`${statement.lineno}: Too many waveshaper coefficients`);
  encoder.v[lenp] = len;
}

function eautSongDecodePost_tremolo(encoder, statement) {
  encoder.u8(0x08);
  encoder.u8(4);
  encFixed(encoder, statement.tokens[1], "u8.8", "period", statement.lineno);
  encFixed(encoder, statement.tokens[2], "u0.8", "depth", statement.lineno);
  encFixed(encoder, statement.tokens[3], "u0.8", "phase", statement.lineno);
}

function eautSongDecodePost_generic(encoder, statement) {
  const stageid = +statement.tokens[0];
  if (isNaN(stageid) || (stageid < 0) || (stageid > 0xff)) throw new Error(`${statement.lineno}: Unknown post stage ${JSON.stringify(statement.tokens[0])}`);
  encoder.u8(stageid);
  const src = decodeHexdump(statement.tokens.slice(1).join(""));
  if (src.length > 0xff) throw new Error(`${statement.lineno}: Stage payload too long (${src.length}, limit 255)`);
  encoder.u8(src.length);
  encoder.raw(src);
}

/* Helpers.
 ***************************************************************************/

function encFixed(encoder, token, fmt, name, lineno) {
  let v = +token;
  if (isNaN(v) || (v < 0)) throw new Error(`${lineno}: Illegal value for ${name}`);
  switch (fmt) {
    case "u8": if (v > 0xff) throw new Error(`${lineno}: Illegal value for ${name}`); encoder.u8(v); break;
    case "u16": if (v > 0xffff) throw new Error(`${lineno}: Illegal value for ${name}`); encoder.u16be(v); break;
    case "u24": if (v > 0xffffff) throw new Error(`${lineno}: Illegal value for ${name}`); encoder.u24be(v); break;
    case "u32": if (v > 0xffffffff) throw new Error(`${lineno}: Illegal value for ${name}`); encoder.u32be(v); break;
    case "u0.8": if (v > 1) throw new Error(`${lineno}: Illegal value for ${name}`); v = Math.floor(v * 255); if (v > 0xff) v = 0xff; encoder.u8(v); break;
    case "u8.8": if (v > 256) throw new Error(`${lineno}: Illegal value for ${name}`); v = Math.floor(v * 256); if (v > 0xffff) v = 0xffff; encoder.u16be(v); break;
    default: throw new Error(`${lineno}: fixed format ${JSON.stringify(fmt)} not defined`);
  }
}

function decodeHexdump(src) {
  const dst = new Uint8Array(src.length >> 1);
  let dstp=0, srcp=0, hi=-1;
  for (; srcp<src.length; srcp++) {
    let ch = src.charCodeAt(srcp);
    if (ch <= 0x20) continue;
         if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30;
    else if ((ch >= 0x41) && (ch <= 0x46)) ch = ch - 0x41 + 10;
    else if ((ch >= 0x61) && (ch <= 0x66)) ch = ch - 0x61 + 10;
    else throw new Error(`Unexpected character '${src[srcp]}' in hex dump`);
    if (hi < 0) {
      hi = ch;
    } else {
      dst[dstp++] = (hi << 4) | ch;
      hi = -1;
    }
  }
  if (dstp >= dst.length) return dst;
  const nv = new Uint8Array(dstp);
  const srcview = new Uint8Array(dst.buffer, 0, dstp);
  nv.set(srcview);
  return nv;
}
