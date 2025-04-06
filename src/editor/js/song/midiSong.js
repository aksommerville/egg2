/* midiSong.js
 * Encode and decode Song objects to/from MIDI.
 * We support EAU and EAU-Text, but MIDI is the one we actually expect to see.
 * (the EAUs are largely runtime-only).
 */
 
import { SongChannel, SongEvent } from "./Song.js";

/* Encode.
 ******************************************************************************/

export function midiSongEncode(song) {
  console.log(`TODO midiSongEncode`);
  return new Uint8Array(0); // TODO
}

/* Decode.
 *****************************************************************************/
 
export function midiSongDecode(song, src) {
  song.format = "mid";
    
  /* Dechunk the file before doing anything else.
   * We'll tolerate MThd in the wrong place (tho it would fail signature match and not land here in the first place).
   * Unknown chunks will be quietly discarded.
   */
  let mthd=null, mtrkv=[];
  for (let srcp=0; srcp<src.length; ) {
    if (srcp > src.length - 8) throw new Error(`Unexpected EOF in MIDI file.`);
    const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
    srcp += 4;
    const chunklen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3];
    srcp += 4;
    if ((chunklen < 0) || (srcp > src.length - chunklen)) throw new Error(`Unexpected EOF in MIDI file.`);
    switch (chunkid) {
      case 0x4d546864: { // MThd
          if (mthd) throw new Error(`Multiple MThd in MIDI file.`);
          mthd = new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen);
        } break;
      case 0x4d54726b: { // MTrk
          mtrkv.push(new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen));
        } break;
    }
    srcp += chunklen;
  }
    
  /* Decode MThd.
   */
  if (!mthd) throw new Error(`MIDI file missing MThd chunk.`);
  let format = (mthd[0] << 8) | mthd[1];
  let trackCount = (mthd[2] << 8) | mthd[3];
  let division = (mthd[4] << 8) | mthd[5];
  if ((division < 1) || (division >= 0x8000)) throw new Error(`Illegal division ${division} in MIDI file.`);
  // (format) should be 1, but take whatever.
    
  const requireChannel = (chid) => {
    if ((chid < 0) || (chid > 15)) throw `Invalid chid ${chid}`;
    let channel = song.channels[chid];
    if (!channel) channel = song.channels[chid] = new SongChannel(chid);
    return channel;
  };
  let gotEauHeaders = false;
    
  /* Decode events from all MTrk in parallel.
   * We have to do it this way because we allow Set Tempo events anywhere, which will impact our calculation of timestamps.
   * Fill in (this.channels) as events warrant. Any chid with a Note On, we must have a channel, even if it's a dummy.
   */
  forEachMidiEvent(song, mtrkv, division, event => {
      
    // Drop Note Off, after applying to their Note On.
    if ((event.type === "m") && (event.opcode === 0x80)) {
      for (let i=song.events.length; i-->0; ) {
        const onevt = song.events[i];
        if (onevt.type !== "n") continue;
        if (onevt.chid !== event.chid) continue;
        if (onevt.noteid !== event.a) continue; // noteid
        if (onevt.durms) break; // already set! We have a redundant Note Off or something.
        onevt.durms = event.time - onevt.time;
        break;
      }
      return;
    }
      
    // Program Change, Bank Select, Volume, and Pan get applied to the channel header and dropped.
    // Unrecognized events pass thru.
    // Do not process them if we have an EAU header. (and we're inconsistent whether they pass thru in that case, yawn, don't care).
    if ((event.type === "m") && (event.chid >= 0) && (event.chid < 0x10) && !gotEauHeaders) {
      if (event.opcode === 0xc0) { // Program Change
        const channel = requireChannel(event.chid);
        channel.pid = event.a;
        return;
      }
      if (event.opcode === 0xb0) switch (event.a) { // Control Change
        case 0x00: requireChannel(event.chid).bankhi = event.b; return;
        case 0x07: requireChannel(event.chid).trim = (event.b << 1) | (event.b & 1); return;
        case 0x0a: requireChannel(event.chid).pan = (event.b << 1) | (event.b & 1); return;
        case 0x20: requireChannel(event.chid).banklo = event.b; return;
        case 0x27: return; // Volume LSB, don't care.
        case 0x2a: return; // Pan LSB, don't care.
      }
    }
      
    // Meta 0x77 is a chunk of EAU Channel Headers, which we must apply verbatim.
    // This drops any channels already received.
    if ((event.type === "m") && (event.opcode === 0xff) && (event.a === 0x77)) {
      gotEauHeaders = true;
      song.channels = SongChannel.decodeHeaders(event.v);
      return;
    }
      
    // Note events require a channel. It's OK for the channel to be completely unconfigured.
    if (event.type === "n") requireChannel(event.chid);
      
    song.events.push(event);
  });
}

  
/* Trigger (cb) for all events across multiple MTrk chunks (mtrkv: Uint8Array[]), in chronological order.
 * (cb) is called with a new SongEvent, unlisted in (this.events).
 */
function forEachMidiEvent(song, mtrkv, division, cb) {
  const trackv = mtrkv.map((src, trackId) => ({
    src,
    trackId,
    p: 0,
    status: 0,
    delay: -1, // ticks; <0 if we haven't read yet (expect vlq delay next in stream)
  }));
  let mspertick = 500 / division;
  let now = 0; // ticks
  for (;;) {
    
    // Acquire delays and identify the track with the next event.
    let nextTrack = null;
    for (const track of trackv) {
      if (track.p >= track.src.length) continue;
      if (track.delay < 0) {
        track.delay = readVlq(track);
      }
      if (!nextTrack || (track.delay < nextTrack.delay)) nextTrack = track;
    }
    if (!nextTrack) break;
      
    // If the selected track has a delay, apply it to all track readers and the clock.
    if (nextTrack.delay > 0) {
      const advance = nextTrack.delay;
      now += advance;
      for (const track of trackv) {
        if (track.p >= track.src.length) continue;
        track.delay -= advance;
      }
    }
      
    // Read event and deliver it.
    const event = readMidiEvent(nextTrack);
    event.time = now * mspertick;
      
    // Set Tempo updates our timekeeping. To avoid confusion, we drop the event.
    // The last Set Tempo event wins, for the global tempo. Songs are supposed to contain only one, at time zero.
    if ((event.type === "m") && (event.opcode === 0xff) && (event.a === 0x51) && (event.v?.length >= 3)) {
      const usperqnote = (event.v[0] << 16) | (event.v[1] << 8) | event.v[2];
      if (usperqnote > 0) {
        mspertick = usperqnote / (division * 1000);
        song.tempo = Math.max(1, Math.round(mspertick * division));
      }
      continue;
    }
      
    //TODO Look for Meta 0x20 MIDI Channel Prefix, then 0x01..0x0f text events, to apply as channel name.
      
    cb(event);
  }
}
  
// Read a VLQ off the stream and advance track position.
function readVlq(track) {
  // It feels a little perverse, but actually works out neater to read content first, then check length.
  // OOB slots of Uint8Array collapse to zero and are perfectly safe to read.
  let v = 0;
  if (!(track.src[track.p] & 0x80)) {
    v = track.src[track.p];
    track.p += 1;
  } else if (!(track.src[track.p+1] & 0x80)) {
    v = ((track.src[track.p] & 0x7f) << 7) | track.src[track.p+1];
    track.p += 2;
  } else if (!(track.src[track.p+2] & 0x80)) {
    v = ((track.src[track.p] & 0x7f) << 14) | ((track.src[track.p+1] & 0x7f) << 7) | track.src[track.p+2];
    track.p += 3;
  } else if (!(track.src[track.p+3] & 0x80)) {
    v = ((track.src[track.p] & 0x7f) << 21) | ((track.src[track.p+1] & 0x7f) << 14) | ((track.src[track.p+2] & 0x7f) << 7) | track.src[track.p+3];
    track.p += 4;
  } else throw new Error(`Expected VLQ at ${track.p}/${track.src.length}`);
  if (track.p > track.src.length) throw new Error(`Expected VLQ before EOF`);
  return v;
}
  
/* Read an event and return it as SongEvent.
 * (time) will be unpopulated.
 * (track.delay) gets reset to -1.
 * Any Note event will have (durms === 0).
 * We'll return Note Off events as (type === "m"), and you must pick them off and find the corresponding Note.
 */
function readMidiEvent(track) {
  track.delay = -1;
  let lead = track.src[track.p];
  if (lead & 0x80) track.p++;
  else if (track.status) lead = track.status;
  else throw new Error(`Unexpected byte ${lead} at ${track.p}/${track.src.length} in MIDI track, expected status byte.`);
  track.status = lead;
  const event = new SongEvent();
  event.trackId = track.trackId;
  event.chid = lead & 0x0f; // until proven otherwise
  switch (lead & 0xf0) {
    
    // Generic 2-byte payload:
    case 0x80: // Note Off
    case 0xa0: // Note Adjust
    case 0xb0: // Control Change
      {
        event.type = "m";
        event.opcode = lead & 0xf0;
        event.a = track.src[track.p++] || 0;
        event.b = track.src[track.p++] || 0;
      } break;
        
    // Generic 1-byte payload:
    case 0xc0: // Program Change
    case 0xd0: // Channel Pressure
      {
        event.type = "m";
        event.opcode = lead & 0xf0;
        event.a = track.src[track.p++] || 0;
      } break;
        
    case 0x90: { // Note On.
        const noteid = track.src[track.p++] || 0;
        const velocity = track.src[track.p++] || 0;
        if (!velocity) {
          event.type = "m";
          event.opcode = 0x80;
          event.a = noteid;
          event.b = 0x40;
        } else {
          event.type = "n";
          event.noteid = noteid;
          event.velocity = velocity >> 3; // 7 bits in MIDI; 4 in EAU.
          event.durms = 0;
        }
      } break;
        
    case 0xe0: { // Wheel
        const lo = track.src[track.p++] || 0;
        const hi = track.src[track.p++] || 0;
        event.type = "w";
        event.v = (hi << 1) | (lo >> 6); // 14 bits in MIDI; 8 in EAU.
      } break;
        
    case 0xf0: { // Meta, Sysex, or invalid
        if ((lead !== 0xf0) && (lead !== 0xf7) && (lead !== 0xff)) {
          throw new Error(`Unexpected status byte ${lead} in MIDI track.`);
        }
        track.status = 0;
        event.chid = -1;
        event.type = "m";
        event.opcode = lead;
        if (lead === 0xff) { // Meta begins with a "type" byte.
          event.a = track.src[track.p++] || 0;
        }
        const len = readVlq(track);
        if (track.p > track.src.length - len) {
          throw new Error(`Overrun in MIDI Meta or Sysex payload.`);
        }
        event.v = new Uint8Array(track.src.buffer, track.src.byteOffset + track.p, len);
        track.p += len;
      } break;
      
    default: throw new Error(`Unexpected status byte ${lead} in MIDI track.`);
  }
  return event;
}
