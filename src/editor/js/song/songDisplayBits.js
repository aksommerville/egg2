/* songDisplayBits.js
 * Little utilities for displaying song data consistently.
 */
 
/* Common set of colors per chid, so we can color-code UI bits for clarity.
 * These are all meant to leave white text legible on top.
 */
export function getChannelColor(chid) {
  return [
    "#300","#030","#006","#320",
    "#230","#302","#203","#032",
    "#023","#533","#353","#335",
    "#766","#676","#667","#776",
  ][chid] || "#555";
}

/* Short friendly description of a noteid.
 * Note zero is C-1. Octaves begin at C, not A.
 * Why C? Why not A? A is the first goddamn letter. Fucking musicians.
 * We'll call all accidentals Sharp, with a hash.
 */
export function reprNoteid(noteid) {
  if ((typeof(noteid) !== "number") || isNaN(noteid) || (noteid < 0) || (noteid > 0x7f)) return "???";
  const pfx = "0x" + noteid.toString(16).padStart(2, '0') + " ";
  const octave = Math.floor(noteid / 12) - 1;
  switch (noteid % 12) {
    case 0: return pfx + "c" + octave;
    case 1: return pfx + "c#" + octave;
    case 2: return pfx + "d" + octave;
    case 3: return pfx + "d#" + octave;
    case 4: return pfx + "e" + octave;
    case 5: return pfx + "f" + octave;
    case 6: return pfx + "f#" + octave;
    case 7: return pfx + "g" + octave;
    case 8: return pfx + "g#" + octave;
    case 9: return pfx + "a" + octave;
    case 10: return pfx + "a#" + octave;
    case 11: return pfx + "b" + octave;
  }
  return "???";
}

/* For known Meta event types (0..255), a friendly name.
 */
export function reprMetaType(type) {
  if (typeof(type) !== "number") return "";
  let dst = `Meta 0x${type.toString(16).padStart(2, '0')}`;
  switch (type) {
    case 0x02: dst += " Copyright"; break;
    case 0x03: dst += " Track Name"; break;
    case 0x04: dst += " Instrument Name"; break;
    case 0x05: dst += " Lyrics"; break;
    case 0x06: dst += " Marker"; break;
    case 0x07: dst += " Cue Point"; break;
    case 0x20: dst += " MIDI Channel Prefix"; break;
    case 0x2f: dst += " End Of Track"; break;
    case 0x51: dst += " Set Tempo"; break;
    case 0x54: dst += " SMPTE Offset"; break;
    case 0x58: dst += " Time Signature"; break;
    case 0x59: dst += " Key Signature"; break;
    case 0x77: dst += " Egg Channel Headers"; break;
    case 0x7f: dst += " Egg v1 Headers"; break;
    default: {
        if ((type >= 1) && (type <= 0x0f)) dst += " Text";
      }
  }
  return dst;
}

/* The only control keys we expect to see are 0x07 Volume MSB, 0x0a Pan MSB, and 0x20 Bank LSB.
 * Naming a few others that might happen. There are many more omitted.
 * In general, our compiler drops Control Change.
 */
export function reprControlKey(k) {
  if (typeof(k) !== "number") return "";
  switch (k) {
    case 0x00: return "Bank MSB";
    case 0x01: return "Mod MSB";
    case 0x02: return "Breath MSB";
    case 0x03: return "Foot MSB";
    case 0x07: return "Volume MSB";
    case 0x0a: return "Pan MSB";
    case 0x0b: return "Expression MSB";
    case 0x20: return "Bank LSB";
    case 0x21: return "Mod LSB";
    case 0x22: return "Breath LSB";
    case 0x23: return "Foot LSB";
    case 0x27: return "Volume LSB";
    case 0x2a: return "Pan LSB";
    case 0x2b: return "Expression LSB";
    case 0x40: return "Sustain";
    case 0x41: return "Portamento";
    case 0x42: return "Sustenuto";
    case 0x43: return "Soft";
    case 0x44: return "Legato";
    case 0x45: return "Hold 2";
  }
  return "0x" + k.toString(16).padStart(2, '0');
}

/* Given a Uint8Array of usually short binary data, return:
 *   [str, mode]
 * Where (str) is plain text of a sensible length, and (mode) is one of:
 *  - "text": Content was ASCII text of reasonable length and we've returned it in quotes.
 *  - "hexdump": Content was short enough, but not empty, and we've returned it as a straight hex dump.
 *  - "placeholder": We've returned a description of the data eg "empty".
 */
export function reprPayload(src) {

  // Empty input is special.
  if (!src?.length) return ["empty", "placeholder"];
  
  // ASCII below some length limit, return text.
  if (src.length < 40) {
    let ok = true;
    for (let i=src.length; i-->0; ) {
      if ((src[i] < 0x20) || (src[i] > 0x7e)) {
        ok = false;
        break;
      }
    }
    if (ok) return [JSON.stringify(new TextDecoder("utf8").decode(src)), "text"];
  }
  
  // If reasonably short, return a hex dump.
  if (src.length < 25) {
    let dst = ""
    for (let i=0; i<src.length; i++) {
      dst += src[i].toString(16).padStart(2, '0');
    }
    return [dst, "hexdump"];
  }
  
  // And finally, a generic placeholder.
  return [`${src.length} bytes`, "placeholder"];
}

/* General hex dump.
 * Uint8Array on the binary side, string on the text side.
 */
export function reprHexdump(src) {
  let dst = "";
  for (let i=0; i<src.length; i++) {
    dst += src[i].toString(16).padStart(2, '0');
  }
  return dst;
}
export function evalHexdump(src) {
  src = src.replace(/\s+/g, "");
  const invalid = src.match(/[^0-9a-fA-F]/);
  if (invalid) throw new Error(`Invalid character '${invalid[0]}' in hex dump`);
  const dst = new Uint8Array(src.length >> 1);
  for (let dstp=0, srcp=0; srcp<src.length; dstp++, srcp+=2) {
    dst[dstp] = parseInt(src.substring(srcp, srcp+2), 16);
  }
  return dst;
}
