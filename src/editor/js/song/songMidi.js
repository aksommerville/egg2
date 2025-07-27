/* songMidi.js
 */
 
//XXX what the fuck am i thinking? We should call the server for these conversions
 
import { Encoder } from "../Encoder.js";
import { SongEvent, SongChannel } from "./Song.js";
 
/* Encode.
 *****************************************************************************/

/* Insert at the correct time.
 * We guarantee that new events appear after existing events at the same time.
 */
function addMidiEvent(track, event) {
  let lo=0, hi=track.length;
  while (lo < hi) {
    const ck = (lo + hi) >> 1;
    const q = track[ck];
         if (event.time < q.time) hi = ck;
    else if (event.time > q.time) lo = ck + 1;
    else {
      lo = ck;
      while ((lo < hi) && (track[lo].time === event.time)) lo++;
      break;
    }
  }
  track.splice(lo, 0, event);
}

/* Return an array of tracks, which are each an array of events:
 *  { time, chid, opcode, a, b, v }
 * (time) are integer ms, and events are sorted by it.
 * Note events are split into On and Off.
 */
function midiEventsByTrack(song) {
  let tracks = []; // Initially index by (trackId) or zero if missing. May be sparse.
  for (const event of song.events) {
    const trackId = event.trackId || 0;
    let track = tracks[trackId];
    if (!track) track = tracks[trackId] = [];
    switch (event.type) {
      case "n": {
          addMidiEvent(track, { time: ~~event.time, chid: event.chid, opcode: 0x90, a: event.noteid, b: event.velocity });
          addMidiEvent(track, { time: ~~event.time + ~~event.durms, chid: event.chid, opcode: 0x80, a: event.noteid, b: 0x40 });
        } break;
      case "w": {
          addMidiEvent(track, { time: ~~event.time, chid: event.chid, opcode: 0xe0, a: event.v & 0x7f, b: event.v >> 7 });
        } break;
      case "m": {
          if ((event.opcode === 0xff) && (event.a === 0x51)) continue; // Meta 0x51 Set Tempo. We'll synthesize this below.
          if ((event.opcode === 0xff) && (event.a === 0x77)) continue; // Meta 0x77 EAU Channel Header. ''
          addMidiEvent(track, { time: ~~event.time, chid: event.chid, opcode: event.opcode, a: event.a, b: event.b, v: event.v });
        } break;
      // Others eg "" get dropped, though they do cause us to create a track.
    }
  }
  
  // Drop missing tracks. Note that this changes (trackId). I feel that's ok, trackId is meant to be transient.
  tracks = tracks.filter(v => v);
  if (!tracks.length) return tracks;
  
  // For each channel in the song, prepend a Meta 0x77 EAU Channel Header to the most appropriate track.
  for (const channel of song.channels) {
    const v = channel.encode();
    const track = tracks.find(t => t.find(e => e.chid === channel.chid)) || tracks[0];
    track.splice(0, 0, { time: 0, opcode: 0xff, a: 0x77, v });
  }
  
  // Append Meta 0x2f End Of Track to any non-empty track missing it.
  for (const track of tracks) {
    if (track.length < 1) continue;
    const last = track[track.length - 1];
    if ((last.opcode === 0xff) && (last.a === 0x2f)) continue;
    track.push({ time: last.time, opcode: 0xff, a: 0x2f, v: [] });
  }
  
  // Declare tempo at the start of the first track.
  const first = tracks[0];
  const uspq = Math.max(0, Math.min(0xffffff, song.tempo * 1000));
  const tempobody = new Uint8Array([ uspq >> 16, uspq >> 8, uspq ]);
  first.splice(0, 0, { time: 0, opcode: 0xff, a: 0x51, v: tempobody });
  
  return tracks;
}

function encodeMThd(dst, song, trackCount) {
  dst.raw("MThd");
  dst.u32be(6);
  dst.u16be(1); // Format
  dst.u16be(trackCount);
  dst.u16be(song.tempo); // Division. We arrange for ticks and ms to be the same thing.
}

function encodeMTrk(dst, track, song) {
  dst.raw("MTrk");
  const lenp = dst.c;
  dst.u32be(0); // Length placeholder.
  let time = 0;
  for (const event of track) {
    dst.vlq(event.time - time);
    time = event.time;
    switch (event.opcode) {
      case 0x80:
      case 0x90:
      case 0xa0:
      case 0xb0:
      case 0xe0: {
          dst.u8(event.opcode | event.chid);
          dst.u8(event.a);
          dst.u8(event.b);
        } break;
      case 0xc0:
      case 0xd0: {
          dst.u8(event.opcode | event.chid);
          dst.u8(event.a);
        } break;
      case 0xf0:
      case 0xf7: {
          dst.u8(event.opcode);
          dst.vlq(event.v.length);
          dst.raw(event.v);
        } break;
      case 0xff: {
          dst.u8(0xff);
          dst.u8(event.a);
          dst.vlq(event.v.length);
          dst.raw(event.v);
        } break;
      default: throw new Error(`Unexpected opcode ${event.opcode}`);
    }
  }
  const len = dst.c - lenp - 4;
  dst.v[lenp+0] = len >> 24;
  dst.v[lenp+1] = len >> 16;
  dst.v[lenp+2] = len >> 8;
  dst.v[lenp+3] = len;
}
 
export function midiSongEncode(song) {
  const dst = new Encoder();
  const tracks = midiEventsByTrack(song);
  encodeMThd(dst, song, tracks.length);
  for (const track of tracks) {
    encodeMTrk(dst, track, song);
  }
  return dst.finish();
}

/* Add tempo change to an array of [time,usperqnote].
 * New insertions will replace any at the same time, and we maintain chronological order.
 */
 
function addMidiTempoChange(tempo, time, usperqnote) {
  let lo=0, hi=tempo.length;
  while (lo < hi) {
    const ck = (lo + hi) >> 1;
    const q = tempo[ck];
         if (time < q[0]) hi = ck;
    else if (time > q[0]) lo = ck + 1;
    else {
      q[1] = usperqnote;
      return;
    }
  }
  tempo.splice(lo, 0, [time, usperqnote]);
}

/* Read from MIDI tracks, during decode.
 * Events are returned as proper SongEvent instances.
 * (time,trackId) are initially zero; decoder should set.
 * (durms) is initially <0 for all Note On events, which are otherwise normal "n" events.
 * Note Off events are reported verbatim as "m" events.
 * (track) is { v, p, status, chpfx, trackId }.
 */
 
function readVlqFromMidiTrack(track) {
  let v = 0;
  for (let i=4; i-->0; ) {
    if (track.p >= track.v.length) throw new Error("Unexpected end of track.");
    const next = track.v[track.p++];
    v <<= 7;
    v |= next & 0x7f;
    if (!(next & 0x80)) return v;
  }
  throw new Error("Malformed VLQ.");
}

function readEventFromMidiTrack(track) {
  if (track.p >= track.v.length) throw new Error("Unexpected end of track.");
  const event = new SongEvent();
  
  let lead = track.v[track.p];
  if (lead & 0x80) track.p++;
  else if (track.status) lead = track.status;
  else throw new Error(`Unexpected leading byte ${lead}`);
  track.status = lead;
  
  switch (lead & 0xf0) {
      
    // Note On: "n".
    case 0x90: {
        if (track.p > track.v.length - 2) throw new Error("Unexpected end of track.");
        if (track.v[track.p+1]) { // Nonzero velocity, a real Note On.
          event.type = "n";
          event.chid = lead & 0x0f;
          event.noteid = track.v[track.p++];
          event.velocity = track.v[track.p++];
          event.durms = -1; // Our caller will replace this when the Note Off is reached; or default to zero.
        } else { // Zero velocity, report as Note Off.
          event.type = "m";
          event.chid = lead & 0x0f;
          event.opcode = 0x80;
          event.a = track.v[track.p++];
          event.b = track.v[track.p++];
        }
      } break;
      
    // Wheel: "w"
    case 0xe0: {
        if (track.p > track.v.length - 2) throw new Error("Unexpected end of track.");
        event.type = "w";
        event.chid = lead & 0x0f;
        event.v = track.v[track.p] | (track.v[track.p + 1] << 7);
        track.p += 2;
      } break;
  
    // Note Off, Note Adjust, Control Change: "m"
    // Note Off will be consumed by our immediate caller; they don't end up in the Song.
    case 0x80: case 0xa0: case 0xb0: {
        if (track.p > track.v.length - 2) throw new Error("Unexpected end of track.");
        event.type = "m";
        event.chid = lead & 0x0f;
        event.opcode = lead & 0xf0;
        event.a = track.v[track.p++];
        event.b = track.v[track.p++];
      } break;
      
    // Program Change, Channel Pressure: "m" (but just one data byte)
    case 0xc0: case 0xd0: {
        if (track.p > track.v.length - 1) throw new Error("Unexpected end of track.");
        event.type = "m";
        event.chid = lead & 0x0f;
        event.opcode = lead & 0xf0;
        event.a = track.v[track.p++];
      } break;
      
    // Meta or Sysex: "m"
    default: {
        track.status = 0;
        event.type = "m";
        event.opcode = lead;
        switch (event.opcode) {
          case 0xff: { // Meta. Same as Sysex but for a leading "type" byte.
              if (track.p >= track.v.length) throw new Error("Unexpected end of track.");
              event.a = track.v[track.p++];
            } break;
          case 0xf0: case 0xf7: break; // Sysex.
          default: throw new Error(`Unexpected leading byte ${lead}`);
        }
        const paylen = readVlqFromMidiTrack(track);
        if (track.p > track.v.length - paylen) throw new Error("Unexpected end of track.");
        event.v = new Uint8Array(track.v.buffer, track.v.byteOffset + track.p, paylen);
        track.p += paylen;
        if (event.opcode === 0xff) switch (event.a) {
          case 0x20: { // MIDI Channel Prefix.
              if (event.v.length >= 1) {
                track.chpfx = event.v[0];
              }
            } break;
        }
        event.chid = track.chpfx;
        // One more thing: If it's Meta 0x77 EAU Channel Header, set (chid) to what's indicated in the payload, regardless of MIDI Channel Prefix.
        if ((event.opcode === 0xff) && (event.a === 0x77) && (event.v.length >= 1)) {
          event.chid = event.v[0];
        }
      } break;
  }
  return event;
}

/* Overwrite channel's config with the payload of Meta 0x77 EAU Channel Header.
 * This replaces everything in the channel, and once set, we'll ignore configuration from other Meta, Control, and Program events.
 * Our spec is explicit about this behavior: "If Meta 0x77 is present, we don't guess anything from the MIDI events."
 * That's good policy, must have been written by somebody very handsome.
 */
function applyChdr(channel, src) {
  if (channel.explicitChdr) {
    console.warn(`Multiple Meta 0x77 EAU Channel Header for channel ${channel.chid}. Using the later one.`);
  }
  channel.explicitChdr = true;
  if (!src?.length || (src[0] !== channel.chid)) throw new Error(`Meta 0x77 names a different channel, applying to ${channel.chid}`);
  channel._decode(src);
}

/* If no Meta 0x77 has been applied yet, consider taking clues from this Meta event.
 * It must have been preceded by 0x20 MIDI Channel Prefix.
 * Actually we cheat a little: Channel names are not stored in CHDR, so we'll accept those even if already CHDR'd.
 */
function applyMetaIfNoChdr(channel, type, v) {
  //if (channel.explicitChdr) return;
  switch (type) {
    case 0x01: case 0x03: case 0x04: { // Text, Track Name, Instrument Name: Use as channel name, first one wins.
        if (v.length && !channel.name) {
          channel.name = new TextDecoder("utf8").decode(v);
        }
      } break;
  }
}

/* If no Meta 0x77 has been applied yet, consider taking clues from the Control or Program event.
 * (k==0xc0) for Program Change, that wouldn't be a legal key for Control Change.
 */
function applyControlIfNoChdr(channel, k, v) {
  if (channel.explicitChdr) return;
  switch (k) {
    case 0x00: channel.pid = (channel.pid & 0x003fff) | (v << 14); break; // Bank MSB
    case 0x07: channel.trim = (v << 1) | (v & 1); break; // Volume MSB (sticky low bit; don't expect an LSB)
    case 0x0a: channel.pan = (v << 1) | (v & 1); break; // Pan MSB, not coincidentally MIDI phrases them the same way we do. Don't expect an LSB.
    case 0x20: channel.pid = (channel.pid & 0x1fc07f) | (v << 7); break; // Bank LSB.
    case 0xc0: channel.pid = (channel.pid & 0x1fff80) | v; break; // Program Change masquerading as Control Change.
  }
}

/* Decode.
 ***************************************************************************/
 
export function midiSongDecode(song, src) {
  song.events = [];

  /* Read MThd, and make a reader for each MTrk.
   */
  let division = 0;
  const tracks = []; // { v, p, status, chpfx, trackId }
  let srcp = 0;
  while (srcp < src.length) {
    if (srcp > src.length - 8) throw new Error("Malformed MIDI");
    const chunkid = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
    const chunklen = (src[srcp] << 24) | (src[srcp+1] << 16) | (src[srcp+2] << 8) | src[srcp+3]; srcp += 4;
    if ((chunklen < 0) || (srcp > src.length - chunklen)) throw new Error("Malformed MIDI");
    const chunk = new Uint8Array(src.buffer, src.byteOffset + srcp, chunklen);
    srcp += chunklen;
    switch (chunkid) {
      case 0x4d546864: { // MThd
          if (division) throw new Error("Multiple MThd in MIDI file");
          if (chunklen < 6) throw new Error("Invalid MThd length");
          division = (chunk[4] << 8) | chunk[5];
          if ((division < 1) || (division >= 0x8000)) throw new Error("Invalid division");
        } break;
      case 0x4d54726b: { // MTrk
          tracks.push({
            v: chunk,
            p: 0,
            status: 0,
            chpfx: -1,
            trackId: tracks.length,
          });
        } break;
      // Ignore unknown chunks.
    }
  }
  if (!division) throw new Error("Missing MThd");
  
  /* Read the tracks serially, no need to interleave.
   * Keep all timing initially in ticks, and we'll convert to ms after all events are captured.
   * Have to do it like that, because a Set Tempo event on one track controls timing of all tracks.
   */
  const tempo = [ // Tempo changes. [time(ticks),us/qnote]. Sorted by time.
    [0, 500000],
  ];
  for (let trackId=0; trackId<tracks.length; trackId++) {
    const track = tracks[trackId];
    let time = 0;
    while (track.p < track.v.length) {
      time += readVlqFromMidiTrack(track); // Every event is preceded by a VLQ delay in ticks.
      const event = readEventFromMidiTrack(track);
      event.time = time;
      event.trackId = trackId;
      
      /* Note Off events are special.
       * Find a Note On event for this track, channel, and note, and update its (durms).
       * Note that we do require each On/Off pair to be on the same track, I don't think MIDI strictly requires that.
       */
      if ((event.type === "m") && (event.opcode === 0x80)) {
        const onEvent = song.events.find(e => (
          (e.trackId === trackId) &&
          (e.chid === event.chid) &&
          (e.type === "n") &&
          (e.noteid === event.a) &&
          (e.durms < 0)
        ));
        if (onEvent) {
          onEvent.durms = time - onEvent.time;
        } else {
          console.log(`Failed to locate Note On for a Note Off. track=${trackId}/${tracks.length} time=${time} chid=${event.chid} noteid=${event.a}`);
        }
        continue;
      }
      
      /* Set Tempo events get recorded like any other Meta event, but also we need to track them.
       */
      if ((event.type === "m") && (event.opcode === 0xff) && (event.a === 0x51)) {
        if (event.v?.length >= 3) {
          addMidiTempoChange(tempo, time, (event.v[0] << 16) | (event.v[1] << 8) | event.v[2]);
        }
      }
      
      song.insertEvent(event);
    }
  }
  
  /* Convert event times from ticks to milliseconds.
   * And any "n" event with (durms<0), issue a warning and force it to zero.
   */
  let tempop=0, nowms=0, nowtick=0, mspertick=0;
  for (const event of song.events) {
    if ((event.type === "n") && (event.durms < 0)) {
      console.log(`Note On unmatched by Note Off. track=${event.trackId}/${tracks.length} time=${event.time} chid=${event.chid} noteid=${event.noteid}`);
      event.durms = 0;
    }
    if ((tempop < tempo.length) && (event.time <= tempo[tempop][0])) {
      const usperqnote = tempo[tempop][1];
      tempop++;
      mspertick = usperqnote / (division * 1000);
    }
    const dtick = event.time - nowtick;
    const dms = dtick * mspertick;
    nowtick = event.time;
    nowms += dms;
    event.time = Math.round(nowms);
  }
  
  /* Collect channel headers.
   */
  for (const event of song.events) {
    if (event.chid < 0) continue;
    if (!song.channelsByChid[event.chid]) {
      const channel = new SongChannel(event.chid);
      song.channels.push(channel);
      song.channelsByChid[event.chid] = channel;
    }
    if (event.type !== "m") continue; // "n" and "w" events can force the channel to exist, but only "m" can configure it.
    const channel = song.channelsByChid[event.chid];
    switch (event.opcode) {
      case 0xff: switch (event.a) {
          case 0x77: applyChdr(channel, event.v); break;
          default: applyMetaIfNoChdr(channel, event.a, event.v); break;
        } break;
      case 0xb0: applyControlIfNoChdr(channel, event.a, event.b); break;
      case 0xc0: applyControlIfNoChdr(channel, 0xc0, event.a); break;
    }
  }
  song.channels.sort((a, b) => a.chid - b.chid);
}
