/* Runtime.js
 * Top level of Egg's web runtime.
 * You give us an encoded ROM (Uint8Array), tell us "start", and that's it.
 */
 
export class Runtime {
  constructor(serial) {
    console.log(`new Runtime`, serial);
  }
  
  start() {
    console.log(`Runtime.start`);
  }
}
