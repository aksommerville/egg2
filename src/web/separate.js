/* separate.js
 * Bootstrap for Egg Separate HTML.
 * We must fetch the ROM as "game.bin", which is served adjacent to us.
 */
 
import { Runtime } from "./js/Runtime.js";
 
addEventListener("load", () => {
  fetch("./game.bin").then(rsp => {
    if (!rsp.ok) throw rsp;
    return rsp.arrayBuffer();
  }).then(serial => {
    new Runtime(new Uint8Array(serial)).start();
  }).catch(e => {
    console.error(e?.message || e?.toString() || e);
  });
}, { once: true });
