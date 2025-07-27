/* songEaut.js
 * We consume and produce strings (not Uint8Array like our EAU and MIDI counterparts).
 * Also, we lean on the EAU encoder: It's simpler to convert encoded-to-encoded, and let EAU handle the model side.
 */
 
//XXX what the fuck am i thinking? We should call the server for these conversions
 
import { eauSongDecode, eauSongEncode } from "./songEau.js";
import { Encoder } from "../Encoder.js";

function lineno(src, srcc) {
  let srcp=0, ln=1;
  while (srcp < srcc) {
    const nlp = src.indexOf("\n", srcp);
    if (nlp < 0) break;
    if (nlp > srcc) break;
    srcp = nlp + 1;
    ln++;
  }
  return ln;
}
 
/* Decode.
 *****************************************************************************************/
 
/* Return a function handler:
 *   handler(encoder, params, src, srcp): srcp
 * Most functions use (src,srcp) only for logging, but "len" also consumes more text.
 * Null if there's no function by this name, and that's normal, try us first.
 */
function eautLookupFunction(name) {
  switch (name) {
    //TODO
  }
  return null;
}

/* If (name) is a simple-replacement symbol, emit its value and return true.
 */
function eautReplaceSymbol(encoder, name) {
  switch (name) {
    //TODO
  }
  return false;
}
 
export function eautSongDecode(song, src) {
  const encoder = new Encoder();
  for (let srcp=0; srcp<src.length; ) {
    
    // Leading byte is '#', '"', whitespace, or the start of an identifier.
    let ch = src.charCodeAt(srcp++);
    if (ch === 0x23) { // Line comment.
      while ((srcp < src.length) && (src.charCodeAt(srcp++) !== 0x0a)) ;
      continue;
    }
    if (ch <= 0x20) { // Whitespace.
      continue;
    }
    if (ch === 0x22) { // String.
      const startp = srcp - 1;
      for (;;) {
        if (srcp >= src.length) throw new Error(`${lineno(src, startp)}: Unclosed string in EAUT`);
        ch = src.charCodeAt(srcp++);
        if (ch === 0x0a) throw new Error(`${lineno(src, startp)}: Unclosed string in EAUT`); // newlines not permitted
        if (ch === 0x22) break;
        if (ch === 0x5c) srcp++; // backslash
      }
      let str = src.substring(startp, srcp);
      try {
        str = JSON.parse(str);
      } catch (e) {
        throw new Error(`${lineno(src, startp)}: Failed to decode JSON string.`);
      }
      encoder.raw(str);
      continue;
    }
    
    // We have an identifier. Either a hex dump, replaceable token, or function call.
    if (
      ((ch >= 0x30) && (ch <= 0x39)) ||
      ((ch >= 0x41) && (ch <= 0x5a)) ||
      ((ch >= 0x61) && (ch <= 0x7a)) ||
      (ch === 0x5f)
    ) ; else throw new Error(`${lineno(src, srcp)}: Unexpected character 0x${ch.toString(16).padStart('0', 2)}`);
    const tokenp = srcp - 1;
    while (srcp < src.length) {
      ch = src.charCodeAt(srcp);
      if (
        ((ch >= 0x30) && (ch <= 0x39)) ||
        ((ch >= 0x41) && (ch <= 0x5a)) ||
        ((ch >= 0x61) && (ch <= 0x7a)) ||
        (ch === 0x5f)
      ) srcp++; else break;
    }
    const token = src.substring(tokenp, srcp);
    
    // If it's a function, read the parenthesized parameters and call out.
    const fn = eautLookupFunction(token);
    if (fn) {
      while ((srcp < src.length) && (src.charCodeAt(srcp) <= 0x20)) srcp++;
      if (src.charCodeAt(srcp++) !== 0x28) throw new Error(`${lineno(src, tokenp)}: Expected '(' after ${JSON.stringify(token)}`);
      const paramp = srcp;
      while ((srcp < src.length) && (src.charCodeAt(srcp) !== 0x29)) srcp++; // There's no nested parens, and we forbid them in strings.
      if (srcp >= src.length) throw new Error(`${lineno(src, tokenp)}: Unclosed parameters.`);
      const param = src.substring(paramp, srcp).trim();
      srcp++;
      const nextp = fn(encoder, param, src, srcp);
      if (nextp > srcp) srcp = nextp;
      continue;
    }
    
    // Check simple substitution symbols.
    if (eautReplaceSymbol(encoder, token)) continue;
    
    // Everything else must be a simple hex dump.
    if (token.startsWith("0x")) token = token.substring(2);
    if (token.length & 1) throw new Error(`${lineno(src, tokenp)}: Hex dump tokens must have even length (found ${token.length})`);
    for (let i=0; i<token.length; ) {
      encoder.u8(parseInt(token.substring(i, i + 2), 16));
    }
  }
  return eauSongDecode(song, encoder.finish());
}

/* Encode.
 *******************************************************************************************/
 
export function eautSongEncode(song) {
  const eau = eauSongEncode(song);
  const encoder = new Encoder();
  console.log(`TODO eautSongEncode`, { eau, song });//TODO
  return encoder.finish();
}
