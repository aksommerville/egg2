/* Input.js
 */

// Match src/eggrt/inmgr/inmgr.h:INMGR_ACTION_*, not because they need to, but just to keep things straight.
const ACTION_QUIT       = 0x00010001;
const ACTION_FULLSCREEN = 0x00010002;

// From egg/egg.h.
const BTN_SOUTH = 0x0001;
const BTN_EAST  = 0x0002;
const BTN_WEST  = 0x0004;
const BTN_NORTH = 0x0008;
const BTN_L1    = 0x0010;
const BTN_R1    = 0x0020;
const BTN_L2    = 0x0040;
const BTN_R2    = 0x0080;
const BTN_AUX2  = 0x0100;
const BTN_AUX1  = 0x0200;
const BTN_AUX3  = 0x0400;
const BTN_CD    = 0x0800;
const BTN_UP    = 0x1000;
const BTN_DOWN  = 0x2000;
const BTN_LEFT  = 0x4000;
const BTN_RIGHT = 0x8000;

// From egg/egg.h
const EVENT_KEY         =  1;
const EVENT_TEXT        =  2;
const EVENT_MMOTION     =  3;
const EVENT_MBUTTON     =  4;
const EVENT_MWHEEL      =  5;
const EVENT_TOUCH       =  6;
const EVENT_GAMEPAD     =  7;
const EVENT_HIDECURSOR  =  8;
const EVENT_LOCKCURSOR  =  9;
const EVENT_NOMAPCURSOR = 10;

const EVTQ_SIZE = 256;
const EVT_SIZE = 20; // struct egg_event, in egg/egg.h
 
export class Input {
  constructor(rt) {
    this.rt = rt;
    this.statev = [0]; // Player states, including player zero.
    this.playerc = 1;
    this.keyListener = null;
    this.pointerListener = null;
    
    // Keyboard, mouse, and touch only listen when running.
    // Gamepads, we listen always:
    this.gamepadListener = e => this.onGamepad(e);
    window.addEventListener("gamepadconnected", this.gamepadListener);
    window.addEventListener("gamepaddisconnected", this.gamepadListener);
    
    this.evtmask = 0;
    this.evtq = new Uint8Array(EVT_SIZE * EVTQ_SIZE); // Preencoded, ring buffer, 20 bytes each.
    this.evtqp = 0;
    this.evtqc = 0;
    
    // Default keyMap should match src/eggrt/inmgr/inmgr_device.c:inmgr_keymapv
    this.keyMap = {
      KeyW: BTN_UP,
      KeyA: BTN_LEFT,
      KeyS: BTN_DOWN,
      KeyD: BTN_RIGHT,
      KeyE: BTN_SOUTH,
      KeyQ: BTN_WEST,
      KeyR: BTN_EAST,
      Space: BTN_SOUTH,
      Comma: BTN_WEST,
      Period: BTN_EAST,
      Slash: BTN_NORTH,
      
      ArrowLeft: BTN_LEFT,
      ArrowRight: BTN_RIGHT,
      ArrowUp: BTN_UP,
      ArrowDown: BTN_DOWN,
      KeyZ: BTN_SOUTH,
      KeyX: BTN_WEST,
      KeyC: BTN_EAST,
      KeyV: BTN_NORTH,
      
      Tab: BTN_L1,
      Backslash: BTN_R1,
      Backquote: BTN_L2,
      Backspace: BTN_R2,
      Enter: BTN_AUX1,
      BracketRight: BTN_AUX2,
      BracketLeft: BTN_AUX3,
      
      Numpad8: BTN_UP,
      Numpad4: BTN_LEFT,
      Numpad5: BTN_DOWN,
      Numpad6: BTN_RIGHT,
      Numpad2: BTN_DOWN,
      Numpad7: BTN_L1,
      Numpad9: BTN_R1,
      Numpad1: BTN_L2,
      Numpad3: BTN_R2,
      Numpad0: BTN_SOUTH,
      NumpadEnter: BTN_WEST,
      NumpadAdd: BTN_EAST,
      NumpadDecimal: BTN_NORTH,
      NumpadDivide: BTN_AUX1,
      NumpadMultiply: BTN_AUX2,
      NumpadSubtract: BTN_AUX3,
      
      Escape: ACTION_QUIT,
      F11: ACTION_FULLSCREEN,
    };
  }
  
  start() {
    this.playerc = 1;
    try {
      const src = this.rt.rom.getMeta("players").split("..");
      if (src.length > 0) {
        this.playerc = +src[src.length - 1] || 0;
        if (this.playerc < 1) this.playerc = 1;
        else if (this.playerc > 8) this.playerc = 8;
      }
    } catch (e) {}
    this.statev = [0];
    if (!this.keyListener) {
      this.keyListener = e => this.onKey(e);
      window.addEventListener("keydown", this.keyListener);
      window.addEventListener("keyup", this.keyListener);
    }
    if (!this.pointerListener) {
      this.pointerListener = e => this.onPointer(e);
      window.addEventListener("pointerdown", this.pointerListener);
      window.addEventListener("pointerup", this.pointerListener);
      window.addEventListener("pointermove", this.pointerListener);
    }
  }
  
  stop() {
    if (this.keyListener) {
      window.removeEventListener("keydown", this.keyListener);
      window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
    if (this.pointerListener) {
      window.removeEventListener("pointerdown", this.pointerListener);
      window.removeEventListener("pointerup", this.pointerListener);
      window.removeEventListener("pointermove", this.pointerListener);
      this.pointerListener = null;
    }
  }
  
  update() {
  }
  
  dispatchAction(action) {
    switch (action) {
      case ACTION_QUIT: this.rt.stop(); break;
      case ACTION_FULLSCREEN: console.log(`TODO Input: ACTION_FULLSCREEN`); break;
      default: console.log(`Input.dispatchAction: Unknown action 0x${action.toString(16)}`);
    }
  }
  
  /* Keyboard.
   ******************************************************************************/
   
  onKey(event) {
    let handled = false;
    
    // If the client requested raw keyboard events, queue it and remember that we've handled.
    let dstevent = this.pushEvent(EVENT_KEY);
    if (dstevent) {
      handled = true;
      dstevent[1] = this.keycodeFromEvent(event);
      if (event.type === "keydown") {
        dstevent[2] = event.repeat ? 2 : 1;
      } else {
        dstevent[2] = 0;
      }
    }
    
    // If it's a "down" and user wants text events, check for that.
    if ((event.type === "keydown") && (event.key.length === 1) && (dstevent = this.pushEvent(EVENT_TEXT))) {
      handled = true;
      dstevent[1] = event.key.charCodeAt(0);
    }
    
    // Did we handle it?
    if (handled) {
      event.stopPropagation();
      event.preventDefault();
      return;
    }
  
    // If a modifier key is down, ignore it and do not consume.
    if (event.ctrlKey || event.altKey || event.shiftKey || event.metaKey) return;
    
    // Locate in keyMap. If it's not named there, ignore and do not consume.
    const btnid = this.keyMap[event.code];
    if (!btnid) {
      if (event.type === "keydown") console.log(`IGNORE KEY: ${event.code}`);
      return;
    }
    
    // We're consuming everything mapped.
    event.stopPropagation();
    event.preventDefault();
    
    // Do not process repeat events, even for actions. But DO let them get this far; it's important that we preventDefault on them.
    if (event.repeat) return;
    
    // If any of the high 16 bits is set, it's a stateless action.
    if (btnid & ~0xffff) {
      if (event.type === "keydown") this.dispatchAction(btnid);
      return;
    }
    
    // Everything else is a button on player 1.
    if (event.type === "keydown") {
      if (this.statev[1] & btnid) return;
      this.statev[1] |= btnid;
      this.statev[0] |= btnid;
    } else {
      if (!(this.statev[1] & btnid)) return;
      this.statev[1] &= ~btnid;
      this.statev[0] &= ~btnid;
    }
  }
  
  keycodeFromEvent(event) {
    return 0x00070004;//TODO
  }
  
  /* Pointer.
   *****************************************************************************/
   
  onPointer(event) {
    console.log(`Input.onPointer ${event.type}`, event);
  }
  
  /* Gamepad.
   *******************************************************************************/
   
  onGamepad(event) {
    console.log(`Input.onGamepad ${event.type}`, event);
  }
  
  /* Event queue.
   *****************************************************************************/
  
  /* true if (ev) is valid, enablable, and turning it on worked.
   * We don't check or update (this.evtmask).
   */
  enableEvent(ev) {
    switch (ev) {
      case EVENT_KEY:
      case EVENT_TEXT:
      case EVENT_MMOTION://TODO pointer visibility
      case EVENT_MBUTTON://''
      case EVENT_MWHEEL://''
      case EVENT_TOUCH:
      case EVENT_GAMEPAD:
        return true;
      case EVENT_HIDECURSOR://TODO
      case EVENT_LOCKCURSOR://TODO
      case EVENT_NOMAPCURSOR:
        return true;
    }
    return false;
  }
  
  /* true if (ev) is valid, enablable, and turning it off worked.
   * We don't check or update (this.evtmask).
   */
  disableEvent(ev) {
    switch (ev) {
      case EVENT_KEY://TODO unmap devices
      case EVENT_TEXT:
      case EVENT_MMOTION://TODO pointer visibility
      case EVENT_MBUTTON://''
      case EVENT_MWHEEL://''
      case EVENT_TOUCH:
      case EVENT_GAMEPAD://TODO unmap devices
      case EVENT_HIDECURSOR://TODO
      case EVENT_LOCKCURSOR://TODO
      case EVENT_NOMAPCURSOR:
        return true;
    }
    return false;
  }
  
  /* null if disabled, otherwise a Uint32Array with the type set, and 4 zeroes after.
   * Events are 5 integers: Type, then 4 type-specific ints. Always 32 bits each.
   * See egg/egg.h:struct egg_event.
   */
  pushEvent(ev) {
    if (!(this.evtmask & (1 << ev))) return null;
    let p = this.evtqp + this.evtqc;
    if (p >= EVTQ_SIZE) p -= EVTQ_SIZE;
    if (this.evtqc >= EVTQ_SIZE) this.evtqp++; // Should we log a warning about dropped events?
    else this.evtqc++;
    const event = new Uint32Array(this.evtq.buffer, this.evtq.byteOffset + p * EVT_SIZE, 5);
    event[0] = ev;
    event[1] = 0;
    event[2] = 0;
    event[3] = 0;
    event[4] = 0;
    return event;
  }
  
  /* Platform API.
   *****************************************************************************/
   
  egg_input_configure() {
    console.log(`TODO Input.egg_input_configure`);
  }
  
  egg_input_get_all(dstp, dsta) {
    if ((dstp < 1) || (dsta < 0)) return;
    dstp >>= 2;
    const m32 = this.rt.exec.mem32;
    if (dstp > m32.length - dsta) return;
    for (let i=0; dsta-->0; i++, dstp++) m32[dstp] = this.statev[i] || 0;
  }
  
  egg_input_get_one(playerid) {
    return this.statev[playerid] || 0;
  }
  
  egg_event_get(dstp, dsta) {
    const cpc = Math.min(this.evtqc, dsta);
    if (cpc < 1) return cpc;
    const dst = this.rt.exec.getMemory(dstp, cpc * EVT_SIZE);
    if (!dst) return -1;
    const headc = EVTQ_SIZE - this.evtqp;
    if (headc >= cpc) { // The whole run is contiguous in (evtq).
      dst.set(new Uint8Array(this.evtq.buffer, this.evtq.byteOffset + this.evtqp * EVT_SIZE, cpc * EVT_SIZE));
      if (this.evtqc -= cpc) {
        if ((this.evtqp += cpc) >= EVTQ_SIZE) this.evtqp = 0;
      } else {
        this.evtqp = 0;
      }
    } else { // Straddles end of ring buffer; two copies.
      const tailc = cpc - headc;
      const dsthead = new Uint8Array(dst.buffer, dst.byteOffset, EVT_SIZE * headc);
      const dsttail = new Uint8Array(dst.buffer, dst.byteOffset + EVT_SIZE * headc, EVT_SIZE * tailc);
      const srchead = new Uint8Array(this.evtq.buffer, this.evtq.byteOffset + this.evtqp * EVT_SIZE, headc * EVT_SIZE);
      const srctail = new Uint8Array(this.evtq.buffer, this.evtq.byteOffset, tailc * EVT_SIZE);
      dsthead.set(srchead);
      dsttail.set(srctail);
      this.evtqc -= cpc;
      this.evtqp = this.evtqp + cpc - EVTQ_SIZE;
    }
    return cpc;
  }
  
  egg_event_enable(ev, en) {
    const bit = 1 << ev;
    if (!bit) return -1;
    if (en) {
      if (this.evtmask & bit) return 0;
      if (!this.enableEvent(ev)) return -1;
      this.evtmask |= bit;
    } else {
      if (!(this.evtmask & bit)) return 0;
      if (!this.disableEvent(ev)) return -1;
      this.evtmask &= ~bit;
    }
    return 0;
  }
  
  egg_event_is_enabled(ev) {
    return (this.evtmask & (1 << ev)) ? 1 : 0;
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
