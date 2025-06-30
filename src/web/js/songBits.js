/* songBits.js
 * Standalone utilities shared around the synthesizer.
 */
 
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
