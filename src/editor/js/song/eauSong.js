/* eauSong.js
 * Encode and decode Song objects to/from EAU Binary.
 */
 
import { SongChannel, SongEvent } from "./Song.js";
import { Encoder } from "../Encoder.js";

/* Convenience to generate a default channel header payload, possibly in the context
 * of a previous config and a different mode we're coming from.
 * This is what happens when you change mode in the editor's channel headers.
 * SongChannel stashes config for the old mode as it changes, and provides that here.
 ******************************************************************************/
 
export function eauGuessInitialChannelConfig(toMode, proposal, fromMode, fromSerial) {

  // Same mode, either generate a default (fromSerial empty) or return it verbatim.
  if (toMode === fromMode) {
    if (fromSerial?.length) return new Uint8Array(fromSerial);
    return eauDefaultChannelConfigForMode(toMode);
  }
  
  // (fromSerial) not provided, use the proposal or default.
  if (!fromSerial?.length) {
    if (proposal?.length) return new Uint8Array(proposal);
    return eauDefaultChannelConfigForMode(toMode);
  }
  
  // If we don't have (proposal), generate the default.
  if (!proposal?.length) proposal = eauDefaultChannelConfigForMode(toMode);
  
  // FM<=SUB or SUB<=FM, take the level envelope from (fromSerial) and the rest from (proposal).
  if ((toMode === 2) && (fromMode === 3)) {
    const proposalModel = eauModecfgDecodeFm(proposal);
    const fromModel = eauModecfgDecodeSub(fromSerial);
    proposalModel.level = fromModel.level;
    return eauModecfgEncodeFm(proposalModel);
  }
  if ((toMode === 3) && (fromMode === 2)) {
    const proposalModel = eauModecfgDecodeSub(proposal);
    const fromModel = eauModecfgDecodeFm(fromSerial);
    proposalModel.level = fromModel.level;
    return eauModecfgEncodeSub(proposalModel);
  }
  
  // Finally, give them the proposal.
  return new proposal;
}

export function eauDefaultChannelConfigForMode(mode) {
  // Since these are static, it would be more efficient to write them out encoded.
  // That would be cumbersome to read, so we're generating a model first and then encoding it.
  // Don't worry! Encode and decode of channel header payloads is cheap.
  switch (mode) {
    
    case 1: { // DRUM
        const model = {
          notes: [],
        };
        return eauModecfgEncodeDrum(model);
      }
      
    case 2: { // FM
        const model = {
          level: eauDefaultLevel(),
          wave: {
            shape: 0, // sine
            qual: 0,
            harmonics: [],
          },
          pitchEnv: eauNoopEnv(),
          wheelRange: 200,
          modRate: 0,
          modRange: 0,
          rangeEnv: eauNoopEnv(),
          rangeLfoRate: 0,
          rangeLfoDepth: 0,
          remainder: new Uint8Array(0),
        };
        return eauModecfgEncodeFm(model);
      }
      
    case 3: { // SUB
        const model = {
          level: eauDefaultLevel(),
          widthLo: 150,
          widthHi: 150,
          stageCount: 2,
          gain: 5.0,
          remainder: new Uint8Array(0),
        };
        return eauModecfgEncodeSub(model);
      }
  }
  // Empty is always legal.
  return new Uint8Array(0);
}

// Envelope model.
function eauDefaultLevel() {
  return {
    flags: 0x05, // velocity|sustain
    initlo: 0,
    inithi: 0,
    susp: 1,
    points: [
      { tlo: 20, thi: 30, vlo:0x3000, vhi:0xffff },
      { tlo: 40, thi: 40, vlo:0x2000, vhi:0x4000 },
      { tlo:150, thi:300, vlo:0x0000, vhi:0x0000 },
    ],
  };
}

function eauNoopEnv() {
  return {
    flags: 0,
    initlo: 0,
    inithi: 0,
    susp: -1,
    points: [],
  };
}

/* Decoder specific to EAU Channel Header payloads.
 ******************************************************************************/
 
class EauModecfgDecoder {
  constructor(src) {
    this.src = src;
    this.p = 0;
  }
  
  finished() {
    return (this.p >= this.src.length);
  }
  
  u8(fallback) {
    if (this.p >= this.src.length) return fallback || 0;
    return this.src[this.p++];
  }
  
  u16(fallback) {
    if (this.p > this.src.length - 2) {
      this.p = this.src.length;
      return fallback || 0;
    }
    const v = (this.src[this.p] << 8) | this.src[this.p+1];
    this.p += 2;
    return v;
  }
  
  u08(fallback) {
    if (this.p >= this.src.length) return fallback || 0;
    return this.src[this.p++] / 255;
  }
  
  u88(fallback) {
    if (this.p > this.src.length - 2) {
      this.p = this.src.length;
      return fallback || 0;
    }
    const v = ((this.src[this.p] << 8) | this.src[this.p+1]) / 256;
    this.p += 2;
    return v;
  }
  
  u16len() {
    const len = this.u16();
    if (this.p > this.src.length - len) {
      this.p = this.src.length;
      return new Uint8Array(0);
    }
    const v = new Uint8Array(this.src.buffer, this.src.byteOffset + this.p, len);
    this.p += len;
    return v;
  }
  
  remainder() {
    if (this.p >= this.src.length) return new Uint8Array(0);
    const v = new Uint8Array(this.src.buffer, this.src.byteOffset + this.p, this.src.length - this.p);
    this.p = this.src.length;
    return v;
  }
  
  env() {
    const model = {
      flags: 0,
      initlo: 0, // u16
      inithi: 0, // u16
      susp: -1,
      points: [], // {tlo,thi,vlo,vhi} All u16.
    };
    model.flags = this.u8();
    if (model.flags & 0x02) { // Initials
      model.initlo = this.u16();
      if (model.flags & 0x01) { // Velocity
        model.inithi = this.u16(model.initlo);
      } else {
        model.inithi = model.initlo;
      }
    }
    if (model.flags & 0x04) { // Sustain
      model.susp = this.u8();
    }
    const ptc = this.u8();
    const ptlen = (model.flags & 0x01) ? 8 : 4;
    if (this.p > this.src.length - ptc * ptlen) {
      this.p = this.src.length;
      return model;
    }
    for (let i=0; i<ptc; i++) {
      const point = {};
      point.tlo = this.u16();
      point.vlo = this.u16();
      if (model.flags & 0x01) {
        point.thi = this.u16();
        point.vhi = this.u16();
      } else {
        point.thi = point.tlo;
        point.vhi = point.vlo;
      }
      model.points.push(point);
    }
    return model;
  }
  
  wave() {
    const model = {
      shape: 0, // u8 (0,1,2,3,4)=(sine,square,saw,triangle,fixedfm)
      qual: 0, // u8
      harmonics: [], // u16
    };
    model.shape = this.u8();
    model.qual = this.u8();
    const harmc = this.u8();
    if (this.p > this.src.length - harmc * 2) {
      this.p = this.src.length;
      return model;
    }
    for (let i=0; i<harmc; i++) {
      model.harmonics.push(this.u16());
    }
    return model;
  }
}


function eauEnvIsDefault(env) {
  if (env.flags) return false;
  if (env.initlo) return false;
  if (env.points.length) return false;
  return true;
}

function eauEncodeEnv(encoder, env) {
  if (!env) env = { flags: 0, points: [] };
  encoder.u8(env.flags);
  if (env.flags & 0x02) { // Initials.
    encoder.u16be(env.initlo);
    if (env.flags & 0x01) { // Velocity.
      encoder.u16be(env.inithi);
    }
  }
  if (env.flags & 0x04) { // Sustain.
    encoder.u8(env.susp);
  }
  encoder.u8(env.points.length);
  for (const pt of env.points) {
    encoder.u16be(pt.tlo);
    encoder.u16be(pt.vlo);
    if (env.flags & 0x01) {
      encoder.u16be(pt.thi);
      encoder.u16be(pt.vhi);
    }
  }
}

function eauWaveIsDefault(wave) {
  if (wave.shape) return false;
  if (wave.qual) return false;
  if (wave.harmonics.length) return false;
  return true;
}

export function eauEncodeWave(encoder, wave) {
  if (!wave) wave = { shape: 0, qual: 0, harmonics: [] };
  encoder.u8(wave.shape);
  encoder.u8(wave.qual);
  encoder.u8(wave.harmonics.length);
  for (const harm of wave.harmonics) {
    encoder.u16be(harm);
  }
}

/* Helpers for dissecting and assembling channel config blocks.
 * These stay encoded most of the time, only get decoded when you open a channel's config modal.
 ********************************************************************************/

/* Model: {
 *   notes: {
 *     noteid: u8
 *     trimLo: u8
 *     trimHi: u8
 *     pan: u8
 *     serial: Uint8Array (full EAU file)
 *   }[]
 * }
 */
export function eauModecfgDecodeDrum(src) {
  const model = {
    notes: [],
  };
  const decoder = new EauModecfgDecoder(src);
  while (!decoder.finished()) {
    const note = {};
    note.noteid = decoder.u8();
    note.trimLo = decoder.u8() || 0;
    note.trimHi = decoder.u8() || 0;
    note.pan = decoder.u8() || 0;
    note.serial = decoder.u16len();
    model.notes.push(note);
  }
  return model;
}
export function eauModecfgEncodeDrum(src) {
  const encoder = new Encoder();
  for (const note of src.notes || []) {
    encoder.u8(note.noteid);
    encoder.u8(note.trimLo);
    encoder.u8(note.trimHi);
    encoder.u8(note.pan);
    encoder.u16be(note.serial.length);
    encoder.raw(note.serial);
  }
  return encoder.finish();
}

/* Model matches the serial layout: {
 *   level: Envelope
 *   wave: Wave
 *   pitchEnv: Envelope
 *   wheelRange: u16 cents
 *   modRate: float (u8.8)
 *   modRange: float (u8.8)
 *   rangeEnv: Envelope
 *   rangeLfoRate: float (u8.8) qnotes
 *   rangeLfoDepth: float (u0.8)
 *   remainder: Uint8Array, should be empty.
 * }
 */
export function eauModecfgDecodeFm(src) {
  const model = {};
  const decoder = new EauModecfgDecoder(src);
  model.level = decoder.env();
  model.wave = decoder.wave();
  model.pitchEnv = decoder.env();
  model.wheelRange = decoder.u16();
  model.modRate = decoder.u88();
  model.modRange = decoder.u88();
  model.rangeEnv = decoder.env();
  model.rangeLfoRate = decoder.u88();
  model.rangeLfoDepth = decoder.u08();
  model.extra = decoder.remainder();
  return model;
}
export function eauModecfgEncodeFm(src) {
  const encoder = new Encoder();
  let fieldCount = 0;
       if (src.remainder?.length) fieldCount = 10;
  else if (src.rangeLfoDepth) fieldCount = 9;
  else if (src.rangeLfoRate) fieldCount = 8;
  else if (!eauEnvIsDefault(src.rangeEnv)) fieldCount = 7;
  else if (src.modRange) fieldCount = 6;
  else if (src.modRate) fieldCount = 5;
  else if (src.wheelRange) fieldCount = 4;
  else if (!eauEnvIsDefault(src.pitchEnv)) fieldCount = 3;
  else if (!eauWaveIsDefault(src.wave)) fieldCount = 2;
  else fieldCount = 1; // Always emit (level). It has its own non-obvious defaulting rules, plus like yeah you always want it.
  eauEncodeEnv(encoder, src.level);
  if (fieldCount >= 2) {
    eauEncodeWave(encoder, src.wave);
    if (fieldCount >= 3) {
      eauEncodeEnv(encoder, src.pitchEnv);
      if (fieldCount >= 4) {
        encoder.u16be(src.wheelRange);
        if (fieldCount >= 5) { // Drop modRate if modRange unset.
          encoder.u8_8(src.modRate);
          encoder.u8_8(src.modRange);
          if (fieldCount >= 7) {
            eauEncodeEnv(encoder, src.rangeEnv);
            if (fieldCount >= 8) { // Drop rangeLfoRate if rangeLfoDepth unset.
              encoder.u8_8(src.rangeLfoRate);
              encoder.u0_8(src.rangeLfoDepth);
              if (fieldCount >= 10) {
                encoder.raw(src.remainder);
              }
            }
          }
        }
      }
    }
  }
  return encoder.finish();
}

/* Model matches the serial layout: {
 *   level: Envelope
 *   widthLo: u16 hz
 *   widthHi: u16 hz
 *   stageCount: u8
 *   gain: float (u8.8)
 *   remainder: Uint8Array, should be empty.
 * }
 */
export function eauModecfgDecodeSub(src) {
  const model = {};
  const decoder = new EauModecfgDecoder(src);
  model.level = decoder.env();
  model.widthLo = decoder.u16();
  model.widthHi = decoder.u16(model.widthLo);
  model.stageCount = decoder.u8(1);
  model.gain = decoder.u88(1.0);
  model.remainder = decoder.remainder();
  return model;
}
export function eauModecfgEncodeSub(src) {
  const encoder = new Encoder();
  let fieldCount;
       if (src.remainder?.length) fieldCount = 6;
  else if (src.gain !== 1) fieldCount = 5;
  else if (src.stageCount !== 1) fieldCount = 4;
  else if (src.widthHi !== src.widthLo) fieldCount = 3;
  else fieldCount = 2; // Always emit (level) and (widthLo).
  eauEncodeEnv(encoder, src.level);
  encoder.u16be(src.widthLo);
  if (fieldCount >= 3) {
    encoder.u16be(src.widthHi);
    if (fieldCount >= 4) {
      encoder.u8(src.stageCount);
      if (fieldCount >= 5) {
        encoder.u8_8(src.gain);
        if (fieldCount >= 6) {
          encoder.raw(src.remainder);
        }
      }
    }
  }
  return encoder.finish();
}

/* Helpers for encoded post pipes.
 ******************************************************************************/

/* Call (cb(stageid, body, p)) for each stage in order.
 * (p) is the offset to the start of the stage, ie its "stageid" byte.
 */
export function eauPostForEach(serial, cb) {
  if (!serial) return;
  for (let srcp=0; srcp<serial.length; ) {
    const startp = srcp;
    const stageid = serial[srcp++];
    const len = serial[srcp++] || 0;
    if (srcp > serial.length - len) break;
    const body = serial.slice(srcp, srcp + len);
    srcp += len;
    cb(stageid, body, startp);
  }
}

/* Returns [{stageid, body, p}], just a flattened version of eauPostForEach.
 */
export function eauPostDecode(serial) {
  const stages = [];
  eauPostForEach(serial, (stageid, body, p) => stages.push({ stageid, body, p }));
  return stages;
}

/* Reverse of eauPostDecode; (stages) is [{stageid, body}], returns Uint8Array.
 */
export function eauPostEncode(stages) {
  const encoder = new Encoder();
  for (const { stageid, body } of stages) {
    if ((typeof(stageid) !== "number") || (stageid < 0) || (stageid > 0xff)) throw new Error(`Invalid stageid: ${JSON.stringify(stageid)}`);
    if (body.length > 0xff) throw new Error(`Invalid length ${body.length} for post stage ${stageid}`);
    encoder.u8(stageid);
    encoder.u8(body.length);
    encoder.raw(body);
  }
  return encoder.finish();
}

export const EAU_POST_STAGE_NAMES = [
  "noop",
  "gain",
  "delay",
  "lopass",
  "hipass",
  "bpass",
  "notch",
  "waveshaper",
  "tremolo",
];

export function eauPostDefaultBody(stageid) {
  switch (stageid) {
    case 1: return new Uint8Array([ 0x01,0x00, 0xff ]); // gain (gain,clip)
    case 2: return new Uint8Array([ 0x01,0x00, 0x80,0x80,0x80,0x80 ]); // delay (period,dry,wet,store,feedback)
    case 3: return new Uint8Array([ 0x02,0x00 ]); // lopass (hz)
    case 4: return new Uint8Array([ 0x02,0x00 ]); // hipass (hz)
    case 5: return new Uint8Array([ 0x02,0x00, 0x01,0x00 ]); // bpass (mid,width)
    case 6: return new Uint8Array([ 0x02,0x00, 0x01,0x00 ]); // notch (mid,width)
    case 7: return new Uint8Array([ 0x80,0x00, 0xff,0xff ]); // waveshaper (level...)
    case 8: return new Uint8Array([ 0x01,0x00, 0x80, 0x00 ]); // tremolo (period,depth,phase)
  }
  return new Uint8Array(0);
}

// We're not providing models for the specific post stages. All of that knowledge is hard-coded in SongPostBodyModal.js.

/* Encode.
 ******************************************************************************/

export function eauSongEncode(song) {
  const encoder = new Encoder();
  
  let loopp = 0;//TODO
  
  encoder.u8(0x00);
  encoder.u8(0x45);
  encoder.u8(0x41);
  encoder.u8(0x55);
  encoder.u16be(song.tempo);
  encoder.u16be(loopp);
  
  for (const channel of song.channels) {
    if (!channel) continue;
    if ((channel.chid < 0) || (channel.chid >= 0xff)) continue;
    if ((channel.payload.length > 0xffff) || (channel.post.length > 0xffff)) {
      throw new Error(`${channel.getDisplayName()} payload or post exceeds 64k.`);
    }
    encoder.u8(channel.chid);
    encoder.u8(channel.trim);
    encoder.u8(channel.pan);
    encoder.u8(channel.mode);
    encoder.u16be(channel.payload.length);
    encoder.raw(channel.payload);
    encoder.u16be(channel.post.length);
    encoder.raw(channel.post);
  }
  encoder.u8(0xff);
  
  let now = 0;
  const delayTo = (nextTime) => {
    let delay = nextTime - now;
    now = nextTime;
    while (delay >= 2048) {
      encoder.u8(0x8f);
      delay -= 2048;
    }
    if (delay >= 128) {
      encoder.u8(0x80 | ((delay >> 7) - 1));
      delay &= 0x7f;
    }
    if (delay) {
      encoder.u8(delay);
    }
  };
  for (const event of song.events) {
    switch (event.type) {
      case "n": {
          delayTo(event.time);
          let dur = event.durms || 0;
          if (dur >= 1024) {
            encoder.u8(0xb0 | event.chid);
            dur = (dur >> 10) - 1;
          } else if (dur >= 32) {
            encoder.u8(0xa0 | event.chid);
            dur = (dur >> 5) - 1;
          } else {
            encoder.u8(0x90 | event.chid);
          }
          if (dur < 0) dur = 0; else if (dur > 0x1f) dur = 0x1f;
          encoder.u8((event.noteid << 1) | (event.velocity >> 3));
          encoder.u8((event.velocity << 5) | dur);
        } break;
      case "w": {
          delayTo(event.time);
          encoder.u8(0xc0 | event.chid);
          encoder.u8(event.v);
        } break;
    }
  }
  if (song.events.length && (song.events[song.events.length-1].time > now)) {
    delayTo(song.events[song.events.length-1].time);
  }
  
  return encoder.finish();
}

/* Decode.
 *****************************************************************************/
 
export function eauSongDecode(song, src) {
  song.format = "eau";
  
  // Fixed header.
  if (src.length < 8) throw new Error(`Invalid length ${src.length} for EAU`);
  if (
    (src[0] !== 0x00) ||
    (src[1] !== 0x45) ||
    (src[2] !== 0x41) ||
    (src[3] !== 0x55)
  ) throw new Error(`EAU signature mismatch`);
  song.tempo = (src[4] << 8) | src[5];
  if (song.tempo < 1) throw new Error(`Invalid tempo in EAU file`);
  const loopp = (src[6] << 8) | src[7];//TODO
  let srcp = 8;
  
  // Channel Headers.
  while (srcp < src.length) {
    if (src[srcp] === 0xff) break;
    const channel = new SongChannel();
    channel.chid = src[srcp++];
    channel.trim = src[srcp++];
    channel.pan = src[srcp++];
    channel.mode = src[srcp++];
    const payloadc = (src[srcp] << 8) | src[srcp+1]; srcp += 2;
    if (srcp > src.length - payloadc) throw new Error(`EAU overrun`);
    channel.payload = src.slice(srcp, srcp + payloadc);
    srcp += payloadc;
    const postc = (src[srcp] << 8) | src[srcp+1]; srcp += 2;
    if (srcp > src.length - postc) throw new Error(`EAU overrun`);
    channel.post = src.slice(srcp, srcp + postc);
    srcp += postc;
  }
  if (srcp++ >= src.length) return;
  
  // Events.
  let now = 0;
  while (srcp < src.length) {
    const lead = src[srcp++];
    if (!lead) break; // EOF.
    if (!(lead & 0x80)) { // SHORT DELAY.
      now += lead;
      continue;
    }
    switch (lead & 0xf0) {
      case 0x80: { // LONG DELAY.
          now += ((lead & 0x0f) + 1) << 7;
        } break;
        
      case 0x90: // SHORT NOTE.
      case 0xa0: // MEDIUM NOTE.
      case 0xb0: { // LONG NOTE.
          if (srcp > src.length - 2) throw new Error(`EAU overrun`);
          const a = src[srcp++];
          const b = src[srcp++];
          const event = new SongEvent();
          event.time = now;
          event.type = "n";
          event.chid = lead & 0x0f;
          event.noteid = a >> 1;
          event.velocity = ((a & 1) << 3) | (b >> 5);
          event.durms = b & 0x1f;
          switch (lead & 0xf0) { // The only difference between the 3 note events is scale of duration.
            case 0x90: break;
            case 0xa0: event.durms = (event.durms + 1) << 5; break;
            case 0xb0: event.durms = (event.durms + 1) << 10; break;
          }
        } break;
        
      case 0xc0: { // WHEEL.
          if (srcp > src.length - 1) throw new Error(`EAU overrun`);
          const v = src[srcp++];
          const event = new SongEvent();
          event.time = now;
          event.type = "w";
          event.chid = lead & 0x0f;
          event.v = v;
        } break;
        
      default: throw new Error(`Unexpected EAU leading byte ${lead}`);
    }
  }
  
  // If it ended with a delay, we must append a dummy event to capture that delay.
  // MIDI's Meta 0x2f End Of Track is appropriate for this.
  if (now && (!song.events.length || (now > song.events[song.events.length-1].time))) {
    const event = new SongEvent();
    event.time = now;
    event.type = "m";
    event.opcode = 0xff;
    event.a = 0x2f;
    event.v = new Uint8Array(0);
    song.events.push(event);
  }
}
