/* Input.js
 */

// Match src/eggrt/inmgr/inmgr.h:INMGR_ACTION_*, not because they need to, but just to keep things straight.
const ACTION_QUIT       = 0x00010001;
const ACTION_FULLSCREEN = 0x00010002;

// From egg/egg.h.
const BTN_LEFT   = 0x0001;
const BTN_RIGHT  = 0x0002;
const BTN_UP     = 0x0004;
const BTN_DOWN   = 0x0008;
const BTN_SOUTH  = 0x0010;
const BTN_WEST   = 0x0020;
const BTN_EAST   = 0x0040;
const BTN_NORTH  = 0x0080;
const BTN_L1     = 0x0100;
const BTN_R1     = 0x0200;
const BTN_AUX1   = 0x0400;
 
export class Input {
  constructor(rt) {
    this.rt = rt;
    this.statev = [0]; // Player states, including player zero.
    this.playerc = 1;
    this.keyListener = null;
    
    // Keyboard, only listen when running.
    // Gamepads, we listen always:
    this.gamepadListener = e => this.onGamepad(e);
    window.addEventListener("gamepadconnected", this.gamepadListener);
    window.addEventListener("gamepaddisconnected", this.gamepadListener);
    
    // Default keyMap should match src/eggrt/inmgr/inmgr_device.c:inmgr_keymapv
    this.keyMap = {
      KeyW: BTN_UP,
      KeyA: BTN_LEFT,
      KeyS: BTN_DOWN,
      KeyD: BTN_RIGHT,
      KeyE: BTN_SOUTH,
      KeyQ: BTN_WEST,
      KeyR: BTN_EAST,
      KeyF: BTN_NORTH,
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
      
      Backquote: BTN_L1,
      Backspace: BTN_R1,
      Enter: BTN_AUX1,
      
      Numpad8: BTN_UP,
      Numpad4: BTN_LEFT,
      Numpad5: BTN_DOWN,
      Numpad6: BTN_RIGHT,
      Numpad2: BTN_DOWN,
      Numpad7: BTN_L1,
      Numpad9: BTN_R1,
      Numpad0: BTN_SOUTH,
      NumpadEnter: BTN_WEST,
      NumpadAdd: BTN_EAST,
      NumpadDecimal: BTN_NORTH,
      NumpadSubtract: BTN_AUX1,
      
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
  }
  
  stop() {
    if (this.keyListener) {
      window.removeEventListener("keydown", this.keyListener);
      window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
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
  
  /* Gamepad.
   *******************************************************************************/
   
  onGamepad(event) {
    console.log(`Input.onGamepad ${event.type}`, event);
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
}
