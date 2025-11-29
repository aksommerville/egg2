/* Input.js
 */

// Match src/eggrt/inmgr/inmgr.h, not because they need to, but just to keep things straight.
const ACTION_QUIT       = 0x01000001;
const ACTION_FULLSCREEN = 0x01000002;

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
const BTN_L2     = 0x0400;
const BTN_R2     = 0x0800;
const BTN_AUX1   = 0x1000;
const BTN_AUX2   = 0x2000;
const BTN_AUX3   = 0x4000;
const BTN_CD     = 0x8000;
const MODE_GAMEPAD = 0;
const MODE_MOUSE   = 1;
 
export class Input {
  constructor(rt) {
    this.rt = rt;
    this.statev = [0x8000, 0x8000]; // Player states, including player zero. The keyboard is always connected, hence a pair of CDs.
    this.playerc = 1;
    this.keyListener = null;
    this.gamepads = []; // Sparse, indexed by (gamepad.index). {id,index,axes:(-1,0,1)[],buttons:(0,1)[],playerid,state}
    this.mode = MODE_GAMEPAD;
    this.mousex = this.rt.video.fbw >> 1;
    this.mousey = this.rt.video.fbh >> 1;
    this.mouseListener = null;
    
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
      
      Backquote: BTN_L2,
      Tab: BTN_L1,
      Backspace: BTN_R2,
      Backslash: BTN_R1,
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
    
    // Indexed by Standard Mapping gamepad button index.
    this.joyMap = [
      BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,
      BTN_L1, BTN_R1, BTN_L2, BTN_R2,
      BTN_AUX2, BTN_AUX1, // Select, Start
      BTN_AUX3, ACTION_QUIT, // Plungers.
      BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
      BTN_AUX3,
    ];
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
    this.statev = [BTN_CD, BTN_CD];
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
    this.dropMouseListener();
  }
  
  update() {
    for (const device of navigator?.getGamepads?.() || []) {
      if (!device) continue;
      const record = this.gamepads[device.index];
      if (!record) continue;
      this.updateGamepad(record, device);
    }
    
    if (this.mode === MODE_MOUSE) {
      if (this.statev[0] & 0x000f) {
        const speed = Math.max(1, Math.floor(this.rt.video.fbw / 150));
        switch (this.statev[0] & (BTN_LEFT | BTN_RIGHT)) {
          case BTN_LEFT: if ((this.mousex -= speed) < 0) this.mousex = -1; break;
          case BTN_RIGHT: if ((this.mousex += speed) >= this.rt.video.fbw) this.mousex = this.rt.video.fbw; break;
        }
        switch (this.statev[0] & (BTN_UP | BTN_DOWN)) {
          case BTN_UP: if ((this.mousey -= speed) < 0) this.mousey = -1; break;
          case BTN_DOWN: if ((this.mousey += speed) >= this.rt.video.fbh) this.mousey = this.rt.video.fbh; break;
        }
      }
    }
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
  
  /* Mouse.
   ************************************************************************/
   
  requireMouseListener() {
    if (!this.mouseListener && this.rt.video.canvas) {
      this.mouseListener = e => this.onMouse(e);
      this.rt.video.canvas.addEventListener("mousemove", this.mouseListener);
      this.rt.video.canvas.addEventListener("mouseenter", this.mouseListener);
      this.rt.video.canvas.addEventListener("mouseleave", this.mouseListener);
      this.rt.video.canvas.addEventListener("mousedown", this.mouseListener);
      this.rt.video.canvas.addEventListener("mouseup", this.mouseListener);
    }
  }
  
  dropMouseListener() {
    if (this.mouseListener && this.rt.video.canvas) {
      this.rt.video.canvas.removeEventListener("mousemove", this.mouseListener);
      this.rt.video.canvas.removeEventListener("mouseenter", this.mouseListener);
      this.rt.video.canvas.removeEventListener("mouseleave", this.mouseListener);
      this.rt.video.canvas.removeEventListener("mousedown", this.mouseListener);
      this.rt.video.canvas.removeEventListener("mouseup", this.mouseListener);
      this.mouseListener = null;
    }
  }
   
  onMouse(event) {
    switch (event.type) {
      case "mousemove":
      case "mouseenter":
      case "mouseleave": {
          const bounds = event.target.getBoundingClientRect();
          this.mousex = Math.max(-1, Math.min(this.rt.video.fbw, Math.floor(((event.x - bounds.x) * this.rt.video.fbw) / bounds.width)));
          this.mousey = Math.max(-1, Math.min(this.rt.video.fbh, Math.floor(((event.y - bounds.y) * this.rt.video.fbh) / bounds.height)));
        } break;
      case "mousedown":
      case "mouseup": {
          let btnid = 0;
          switch (event.button) {
            case 0: btnid = BTN_SOUTH; break; // Left
            case 1: btnid = BTN_EAST; break; // Middle
            case 2: btnid = BTN_WEST; break; // Right
          }
          if (btnid) {
            if (event.type === "mousedown") this.statev[0] |= btnid;
            else this.statev[0] &= ~btnid;
          }
        } break;
    }
    event.preventDefault();
    event.stopPropagation();
  }
  
  /* Gamepad.
   *******************************************************************************/
   
  onGamepad(event) {
    if (!event?.gamepad) return;
    if (event.type === "gamepadconnected") {
      //TODO Generic gamepad mapping. For now assuming Standard Mapping.
      //console.log(`connected gamepad`, event.gamepad);
      const record = {
        id: event.gamepad.id,
        index: event.gamepad.index,
        axes: [0, 0], // Two axes, regardless of what the device has.
        buttons: event.gamepad.buttons.map(v => 0), // Buttons match the device.
        playerid: 0, // Zero until the first significant event.
        state: BTN_CD,
      };
      // Standard Mapping only describes 17 buttons. If there are more, ignore the excess.
      if (record.buttons.length > 17) record.buttons = record.buttons.slice(0, 17);
      this.gamepads[event.gamepad.index] = record;
      
    } else if (event.type === "gamepaddisconnected") {
      const gamepad = this.gamepads[event.gamepad.index];
      if (!gamepad) return;
      delete this.gamepads[event.gamepad.index];
      if (gamepad.playerid) {
        this.statev[gamepad.playerid] &= ~gamepad.state;
        this.statev[0] &= ~gamepad.state;
        this.statev[0] |= BTN_CD; // Always present.
        this.statev[1] |= BTN_CD;
      }
    }
  }
  
  /* (record) lives in (this.gamepads); (device) is from the browser.
   */
  updateGamepad(record, device) {
    const AXTHRESH = 0.250;
    for (let i=0; i<2; i++) {
      let nv = device.axes[i] || 0;
      if (nv > AXTHRESH) nv = 1;
      else if (nv < -AXTHRESH) nv = -1;
      else nv = 0;
      if (nv === record.axes[i]) continue;
      const btnidlo = i ? BTN_UP : BTN_LEFT;
      const btnidhi = i ? BTN_DOWN : BTN_RIGHT;
      if (record.axes[i] < 0) this.setGamepadButton(record, btnidlo, 0);
      else if (record.axes[i] > 0) this.setGamepadButton(record, btnidhi, 0);
      record.axes[i] = nv;
      if (nv < 0) this.setGamepadButton(record, btnidlo, 1);
      else if (nv > 0) this.setGamepadButton(record, btnidhi, 1);
    }
    for (let i=record.buttons.length; i-->0; ) {
      const nv = device.buttons[i]?.value ? 1 : 0;
      if (record.buttons[i] === nv) continue;
      record.buttons[i] = nv;
      const dstbtnid = this.joyMap[i];
      if (dstbtnid & ~0xffff) {
        if (nv) this.dispatchAction(dstbtnid);
      } else if (dstbtnid) {
        this.setGamepadButton(record, dstbtnid, nv);
      }
    }
  }
  
  setGamepadButton(record, btnid, value) {
    if (value) {
      if (record.state & btnid) return;
      record.state |= btnid;
      this.requireGamepadPlayer(record);
      this.statev[record.playerid] |= btnid;
      this.statev[0] |= btnid;
    } else {
      if (!(record.state & btnid)) return;
      record.state &= ~btnid;
      if (record.playerid) {
        this.statev[record.playerid] &= ~btnid;
        this.statev[0] &= ~btnid;
      }
    }
  }
  
  requireGamepadPlayer(record) {
    if (record.playerid) return;
    if (this.playerc < 2) {
      record.playerid = 1;
    } else {
      const countByPlayerid = [];
      for (const record of this.gamepads) {
        if (!record?.playerid) continue;
        while (record.playerid >= countByPlayerid.length) countByPlayerid.push(0);
        countByPlayerid[record.playerid]++;
      }
      record.playerid = 1;
      for (let i=1; i<=this.playerc; i++) {
        if ((countByPlayerid[i] || 0) < countByPlayerid[record.playerid]) record.playerid = i;
      }
    }
    if (!this.statev[record.playerid]) this.statev[record.playerid] = 0;
    this.statev[record.playerid] |= BTN_CD;
    this.statev[record.playerid] |= BTN_CD;
  }
  
  /* Platform API.
   *****************************************************************************/
   
  egg_input_configure() {
    console.log(`TODO Input.egg_input_configure`);
  }
  
  egg_input_get_all(dstp, dsta) {
    if ((dstp < 1) || (dsta < 1)) return;
    dstp >>= 2;
    const m32 = this.rt.exec.mem32;
    if (dstp > m32.length - dsta) return;
    const dstp0 = dstp;
    for (let i=0; dsta-->0; i++, dstp++) m32[dstp] = this.statev[i] || 0;
    if (this.mode === MODE_MOUSE) {
      m32[dstp0] &= (BTN_SOUTH | BTN_WEST | BTN_EAST);
    }
  }
  
  egg_input_get_one(playerid) {
    if (!playerid && (this.mode === MODE_MOUSE)) {
      return this.statev[0] & (BTN_SOUTH | BTN_WEST | BTN_EAST);
    }
    return this.statev[playerid] || 0;
  }
  
  egg_input_set_mode(mode) {
    if (mode === this.mode) return;
    switch (mode) {
      case MODE_GAMEPAD: {
          this.mode = mode;
          this.statev[0] = 0;
          this.dropMouseListener();
        } break;
      case MODE_MOUSE: {
          this.mode = mode;
          this.statev[0] = 0;
          this.requireMouseListener();
        } break;
    }
  }
  
  egg_input_get_mouse(xp, yp) {
    switch (this.mode) {
      case MODE_MOUSE: {
          if (xp) this.rt.exec.mem32[xp >> 2] = this.mousex;
          if (yp) this.rt.exec.mem32[yp >> 2] = this.mousey;
        } return 1;
    }
    return 0;
  }
}
