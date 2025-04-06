/* eauSong.js
 * Encode and decode Song objects to/from EAU Binary.
 */
 
import { SongChannel, SongEvent } from "./Song.js";
import { Encoder } from "../Encoder.js";

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
