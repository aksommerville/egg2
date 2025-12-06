/* Input.js
 */
 
import { Incfg } from "./Incfg.js";
import { TouchInput } from "./TouchInput.js";

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
const MODE_RAW     = -1; // Private mode for Incfg.
 
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
    this.rawCb = null; // (devid,btnid,value) => {}
    this.touch = null; // null or TouchInput. Only when running, and only if we deem it appropriate.
    
    // Keyboard, only listen when running.
    // Gamepads, we listen always:
    this.gamepadListener = e => this.onGamepad(e);
    window.addEventListener("gamepadconnected", this.gamepadListener);
    window.addEventListener("gamepaddisconnected", this.gamepadListener);
    
    this.loadMaps();
  }
  
  loadMaps() {
    try {
      this.keyMap = JSON.parse(localStorage.getItem("egg.keyMap"));
      if (!Object.keys(this.keyMap).length) throw null;
    } catch (e) {
      this.keyMap = this.defaultKeyMap();
    }
    try {
      this.joyMap = JSON.parse(localStorage.getItem("egg.joyMap"));
      if (!this.joyMap || (typeof(this.joyMap) !== "object")) throw null; // Empty is fine, but don't let it be null or whatever.
    } catch (e) {
      this.joyMap = {};
    }
  }
  
  saveMaps() {
    localStorage.setItem("egg.keyMap", JSON.stringify(this.keyMap));
    localStorage.setItem("egg.joyMap", JSON.stringify(this.joyMap));
  }
  
  defaultKeyMap() {
    // Default keyMap should match src/eggrt/inmgr/inmgr_device.c:inmgr_keymapv
    return {
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
  }
  
  defaultJoyMap() {
    // Indexed by Standard Mapping gamepad button index.
    return {
      buttons: [
        BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,
        BTN_L1, BTN_R1, BTN_L2, BTN_R2,
        BTN_AUX2, BTN_AUX1, // Select, Start
        BTN_AUX3, ACTION_QUIT, // Plungers.
        BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
        BTN_AUX3,
      ],
      axes: [
        BTN_LEFT | BTN_RIGHT,
        BTN_UP | BTN_DOWN,
      ],
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
    this.statev = [BTN_CD, BTN_CD];
    if (!this.keyListener) {
      this.keyListener = e => this.onKey(e);
      window.addEventListener("keydown", this.keyListener);
      window.addEventListener("keyup", this.keyListener);
    }
    if (this.mode === MODE_MOUSE) {
      this.requireMouseListener();
    }
    if (!this.touch && this.shouldUseTouchInput()) {
      this.touch = new TouchInput(this);
      this.touch.onEvent = e => this.onTouch(e);
    }
  }
  
  stop() {
    if (this.keyListener) {
      window.removeEventListener("keydown", this.keyListener);
      window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
    this.dropMouseListener();
    if (this.touch) {
      this.touch.stop();
      this.touch = null;
    }
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
      case ACTION_QUIT: this.rt.toggleUmenu(); break;
      case ACTION_FULLSCREEN: console.log(`TODO Input: ACTION_FULLSCREEN`); break;
      default: console.log(`Input.dispatchAction: Unknown action 0x${action.toString(16)}`);
    }
  }
  
  zeroAllStates() {
    for (let i=this.statev.length; i-->0; ) {
      this.statev[i] &= 0x8000; // Don't blank CD.
    }
    for (const gp of this.gamepads) {
      if (gp) gp.state &= 0x8000;
    }
  }
  
  /* Raw mode, for Incfg.
   ****************************************************************************/
  
  /* In raw mode, normal operation is suspended, and instead we call (cb)(devid,btnid,value)
   * for all nonzero input state changes.
   */
  beginRawMode(cb) {
    this.pvmode = this.mode;
    this.mode = MODE_RAW;
    this.rawCb = cb;
    this.zeroAllStates();
  }
  
  /* (assignments) null or an array of [devid,srcbtnid,dstbtnid].
   * (srcbtnid) may be negative to reverse axes.
   */
  endRawMode(assignments) {
    this.mode = this.pvmode;
    this.rawCb = null;
    if (assignments?.length) {
      let dirty = false;
      for (const assignment of assignments) {
        if (this.updateAssignment(assignment[0], assignment[1], assignment[2])) {
          dirty = true;
        }
      }
      if (dirty) {
        this.saveMaps();
        this.reassignLiveMaps();
      }
    }
  }
  
  updateAssignment(devid, srcbtnid, dstbtnid) {
    if (!devid || !srcbtnid) return false;
    
    if (devid === "Keyboard") {
      if (this.keyMap[srcbtnid] === dstbtnid) return false;
      this.keyMap[srcbtnid] = dstbtnid;
    
    } else {
      let map = this.joyMap[devid];
      if (!map) {
        map = this.defaultJoyMap();
        this.joyMap[devid] = map;
      }
      if ((srcbtnid > -0x200) && (srcbtnid <= -0x100)) { // reverse axis
        const p = (-srcbtnid) & 0xff;
        while (map.axes.length <= p) map.axes.push(0);
        if (map.axes[p] === dstbtnid) return false;
        map.axes[p] = dstbtnid;
      } else if ((srcbtnid >= 0x100) && (srcbtnid < 0x200)) { // axis
        const p = srcbtnid & 0xff;
        while (map.axes.length <= p) map.axes.push(0);
        if (map.axes[p] === dstbtnid) return false;
        map.axes[p] = dstbtnid;
      } else if ((srcbtnid >= 0x200) && (srcbtnid < 0x300)) { // button
        const p = srcbtnid & 0xff;
        while (map.buttons.length <= p) map.buttons.push(0);
        if (map.buttons[p] === dstbtnid) return false;
        map.buttons[p] = dstbtnid;
      } else {
        return false;
      }
    }
    return true;
  }
  
  reassignLiveMaps() {
    for (const gp of this.gamepads) {
      if (!gp) continue;
      const map = this.joyMap[gp.id];
      if (!map) continue;
      gp.map = map;
      gp.state &= BTN_CD;
    }
  }
  
  buttonIsAxis(devid, btnid) {
    if (devid === "Keyboard") return false;
    return ((btnid >= 0x100) && (btnid < 0x200));
  }
  
  /* Touch.
   *******************************************************************************/
  
  shouldUseTouchInput() {
    return window?.navigator?.maxTouchPoints > 0;
  }
   
  onTouch(event) {
    if (!(event.btnid & 0xffff)) return; // Not handling signals, I don't think we're going to use them from TouchInput.
    if (event.btnid === BTN_CD) return; // Don't want TouchInput's opinion of CD, it shouldn't have one.
    if (event.value) {
      if (this.statev[1] & event.btnid) return; // Already down.
      // Down.
      this.statev[1] |= event.btnid;
      this.statev[0] |= event.btnid;
    } else {
      if (!(this.statev[1] & event.btnid)) return; // Already up.
      // Up.
      this.statev[1] &= ~event.btnid;
      this.statev[0] &= ~event.btnid;
    }
  }
  
  /* Keyboard.
   ******************************************************************************/
   
  onKey(event) {
  
    // If a modifier key is down, ignore it and do not consume.
    if (event.ctrlKey || event.altKey || event.shiftKey || event.metaKey) return;
    
    // In raw mode, consume and report everything.
    if (this.mode === MODE_RAW) {
      event.stopPropagation();
      event.preventDefault();
      if ((event.type === "keydown") && !event.repeat) {
        this.rawCb("Keyboard", event.code, 1);
      }
      return;
    }
    
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
      const record = {
        id: event.gamepad.id,
        index: event.gamepad.index,
        axes: event.gamepad.axes.map(v => 0),
        buttons: event.gamepad.buttons.map(v => 0),
        playerid: 0, // Zero until the first significant event.
        state: BTN_CD,
        map: this.joyMap[event.gamepad.id] || this.defaultJoyMap(),
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
    for (let i=device.axes.length; i-->0; ) {
      let nv = device.axes[i] || 0;
      if (nv > AXTHRESH) nv = 1;
      else if (nv < -AXTHRESH) nv = -1;
      else nv = 0;
      if (nv === record.axes[i]) continue;
      if (this.mode === MODE_RAW) {
        this.rawCb?.(device.id, 0x100 + i, nv);
        record.axes[i] = nv;
        continue;
      }
      const btnid = record.map.axes[i];
      if (btnid) {
        const btnidlo = btnid & (BTN_LEFT | BTN_UP);//TODO This doesn't accomodate reverse.
        const btnidhi = btnid & (BTN_RIGHT | BTN_DOWN);//TODO And are we going to allow axes to map to regular buttons?
        if (record.axes[i] < 0) this.setGamepadButton(record, btnidlo, 0);
        else if (record.axes[i] > 0) this.setGamepadButton(record, btnidhi, 0);
        if (nv < 0) this.setGamepadButton(record, btnidlo, 1);
        else if (nv > 0) this.setGamepadButton(record, btnidhi, 1);
      }
      record.axes[i] = nv;
    }
    for (let i=record.buttons.length; i-->0; ) {
      const nv = device.buttons[i]?.value ? 1 : 0;
      if (record.buttons[i] === nv) continue;
      record.buttons[i] = nv;
      if (this.mode === MODE_RAW) {
        this.rawCb?.(device.id, 0x200 + i, nv);
        continue;
      }
      const dstbtnid = record.map.buttons[i];
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
    if (this.rt.incfg) return;
    this.rt.incfg = new Incfg(this.rt);
    this.rt.incfg.start();
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
          this.statev[0] &= 0x8000;
          this.dropMouseListener();
        } break;
      case MODE_MOUSE: {
          this.mode = mode;
          this.statev[0] &= 0x8000;
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
