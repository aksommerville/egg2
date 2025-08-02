/* standalone.js
 * Bootstrap for Egg Standalone HTML.
 * There is an <egg-rom> element in our document with the base64-encoded ROM.
 */
 
import { Runtime } from "./js/Runtime.js";
 
function acquireRomFromUi() {
  const src = document.querySelector("egg-rom")?.innerText || "";
  let srcp = 0;
  
  // Output will be <(src.length * 3/4), stricly less because there should also be some whitespace.
  // But it's fine to overshoot.
  const dst = new Uint8Array(src.length);
  let dstc = 0;
  
  // Decode base64. We can cheat a little and pad out the end to a 3-byte multiple. Trailing bytes do no harm.
  const tmp = [0, 0, 0, 0]; // 0..63
  let tmpc = 0;
  while (srcp < src.length) {
    let ch = src.charCodeAt(srcp++);
    if (ch <= 0x20) continue;
    else if ((ch >= 0x41) && (ch <= 0x5a)) ch = ch - 0x41;
    else if ((ch >= 0x61) && (ch <= 0x7a)) ch = ch - 0x61 + 26;
    else if ((ch >= 0x30) && (ch <= 0x39)) ch = ch - 0x30 + 52;
    else if (ch === 0x2b) ch = 62;
    else if (ch === 0x2f) ch = 63;
    else if (ch === 0x3d) continue;
    else throw new Error(`Unexpected byte ${ch} in base64-encoded ROM.`);
    tmp[tmpc++] = ch;
    if (tmpc === 4) {
      dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4);
      dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2);
      dst[dstc++] = (tmp[2] << 6) | tmp[3];
      tmpc = 0;
    }
  }
  if (tmpc) {
    dst[dstc++] = (tmp[0] << 2) | (tmp[1] >> 4);
    dst[dstc++] = (tmp[1] << 4) | (tmp[2] >> 2);
    dst[dstc++] = (tmp[2] << 6) | tmp[3];
  }
  if (dstc > dst.length) throw new Error(`internal error decoding base64`);
  
  return new Uint8Array(dst.buffer, 0, dstc);
}
 
addEventListener("load", () => {
  try {
    new Runtime(acquireRomFromUi()).start();
  } catch (e) {
    console.error(e?.message || e?.toString() || e);
  }
}, { once: true });
