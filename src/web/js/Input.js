/* Input.js
 */
 
export class Input {
  constructor(rt) {
    this.rt = rt;
  }
  
  start() {
  }
  
  stop() {
  }
  
  update() {
  }
  
  /* Platform API.
   *****************************************************************************/
   
  egg_input_configure() {
    console.log(`TODO Input.egg_input_configure`);
  }
  
  egg_input_get_all(dstp, dsta) {
    console.log(`TODO Input.egg_input_get_all ${dstp},${dsta}`);
  }
  
  egg_input_get_one(playerid) {
    console.log(`TODO Input.egg_input_get_one ${playerid}`);
    return 0;
  }
  
  egg_event_get(dstp, dsta) {
    console.log(`TODO Input.egg_event_get ${dstp},${dsta}`);
    return 0;
  }
  
  egg_event_enable(ev, en) {
    console.log(`TODO Input.egg_event_enable ${ev},${en}`);
    return -1;
  }
  
  egg_event_is_enabled(ev) {
    console.log(`TODO Input.egg_event_is_enabled ${ev}`);
    return 0;
  }
  
  egg_gamepad_get_name(dstp, dsta, vidp, pidp, verp, devid) {
    console.log(`TODO Input.egg_gamepad_get_name ${dstp},${dsta},${vidp},${pidp},${verp},${devid}`);
    return 0;
  }
  
  egg_gamepad_get_button(btnidp, hidusagep, lop, hip, restp, devid, btnix) {
    console.log(`TODO Input.egg_gamepad_get_button ${btnidp},${hidusagep},${lop},${hip},${restp},${devid},${btnix}`);
    return 0;
  }
}
