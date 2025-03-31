/* MapPaint.js
 * Context for MapEditor, shareable across the various UI bits.
 * We are not a singleton! We live only as long as the MapEditor, and get injected as an override.
 * Everything interesting happens here.
 */
 
import { MapService } from "./MapService.js";
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { CommandListEditor } from "../std/CommandListEditor.js";
 
export class MapPaint {
  static getDependencies() {
    return [MapService, Dom, Data, Window];
  }
  constructor(mapService, dom, data, window) {
    this.mapService = mapService;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.mapEditor = null; // MapEditor should inject itself here during construction.
    
    this.path = "";
    this.map = null;
    this.image = null;
    this.nextListenerId = 1;
    this.listeners = [];
    this.modifiers = []; // "ctl","shift"
    this.scrollTimeout = null;
    this.mousex = -1; // in map cells
    this.mousey = -1; // ''
    
    this.manualTool = "rainbow";
    this.effectiveTool = this.manualTool;
    this.tileid = 0x00;
    this.tilesize = 16; // Natural size.
    this.zoom = 3; // Displayed tilesize relative to natural.
    this.scrollx = 0;
    this.scrolly = 0;
    this.toggles = {
      image: true,
      grid: true,
      poi: true,
      physics: false,
      joinOutside: false,
    };
    this.loadSettings();
  }
  
  destroy() {
    this.mapEditor = null;
    this.map = null;
    this.image = null;
    if (this.scrollTimeout) {
      this.window.clearTimeout(this.scrollTimeout);
      this.scrollTimeout = null;
    }
  }
  
  loadSettings() {
    try {
      const src = JSON.parse(this.window.localStorage.getItem("eggEditor.MapPaint"));
      if (typeof(src.manualTool) === "string") this.effectiveTool = this.manualTool = src.manualTool;
      if (typeof(src.tileid) === "number") this.tileid = src.tileid;
      if (typeof(src.tilesize) === "number") this.tilesize = src.tilesize;
      if (typeof(src.zoom) === "number") this.zoom = src.zoom;
      if (typeof(src.scrollx) === "number") this.scrollx = src.scrollx;
      if (typeof(src.scrolly) === "number") this.scrolly = src.scrolly;
      if (src.toggles && (typeof(src.toggles) === "object")) this.toggles = src.toggles;
    } catch (e) {
    }
  }
  
  saveSettings() {
    this.window.localStorage.setItem("eggEditor.MapPaint", JSON.stringify({
      manualTool: this.manualTool,
      tileid: this.tileid,
      tilesize: this.tilesize,
      zoom: this.zoom,
      scrollx: this.scrollx,
      scrolly: this.scrolly,
      toggles: this.toggles,
    }));
  }
  
  /* { type:"tool", tool:name }
   * { type:"tileid", tileid:0..255 }
   * { type:"zoom", zoom }
   * { type:"scroll", x, y } Delayed.
   * { type:"map", map, path, image:null }
   * { type:"image", image }
   * { type:"commands" }
   * { type:"toggle", k, v }
   * { type:"mouse", x, y } in map cells
   */
  listen(cb) {
    const id = this.nextListenerId++;
    this.listeners.push({ id, cb });
    return id;
  }
  
  unlisten(id) {
    const p = this.listeners.findIndex(l => l.id === id);
    if (p >= 0) this.listeners.splice(p, 1);
  }
  
  broadcast(event) {
    for (const { cb } of this.listeners) cb(event);
  }
  
  /* Editor state, data dispatch.
   *****************************************************************************************/
  
  setResource(path, map) {
    this.path = path;
    this.map = map;
    this.image = null;
    this.tilesize = 16;
    const imageName = this.map.cmd.getFirstArg("image");
    if (imageName) {
      this.data.getImageAsync(imageName).then(image => {
        this.image = image;
        this.tilesize = Math.max(1, image.naturalWidth >> 4);
        this.broadcast({ type: "image", image });
      }).catch(() => {});
    }
    this.broadcast({ type: "map", map, path, image: null });
  }
  
  setPalette(tileid) {
    if ((typeof(tileid) !== "number") || (tileid < 0) || (tileid > 0xff)) return;
    if (tileid === this.tileid) return;
    this.tileid = tileid;
    this.saveSettings();
    this.broadcast({ type: "tileid", tileid: this.tileid });
  }
  
  setZoom(zoom) {
    if ((typeof(zoom) !== "number") || (zoom < 0.010) || (zoom > 100)) return;
    if (this.zoom === zoom) return;
    this.zoom = zoom;
    this.saveSettings();
    this.broadcast({ type: "zoom", zoom: this.zoom });
  }
  
  setScroll(x, y) {
    if ((x === this.scrollx) && (y === this.scrolly)) return;
    this.scrollx = x;
    this.scrolly = y;
    // Require one second idle before broadcasting or saving.
    if (this.scrollTimeout) this.window.clearTimeout(this.scrollTimeout);
    this.scrollTimeout = this.window.setTimeout(() => {
      this.scrollTimeout = null;
      this.broadcast({ type: "scroll", x: this.scrollx, y: this.crolly });
      this.saveSettings();
    }, 1000);
  }
  
  setToggle(k, v) {
    if (!k || (typeof(v) !== "boolean")) return;
    if (typeof(this.toggles[k]) !== "boolean") return;
    if (this.toggles[k] === v) return;
    this.toggles[k] = v;
    this.saveSettings();
    this.broadcast({ type: "toggle", k, v });
  }
  
  setTool(name) {
    if (name === this.manualTool) return;
    this.manualTool = name;
    this.effectiveTool = name;
    this.saveSettings();
    this.broadcast({ type: "tool", tool: this.effectiveTool });
  }
  
  setModifier(name, value) {
    if (!["ctl", "shift"].includes(name)) return;
    if (value) {
      if (this.modifiers.includes(name)) return;
      this.modifiers.push(name);
    } else {
      const p = this.modifiers.indexOf(name);
      if (p < 0) return;
      this.modifiers.splice(p, 1);
    }
    this.refreshEffectiveTool();
  }
  
  refreshEffectiveTool() {
    let nv = this.manualTool;
    const tool = MapPaint.TOOLS.find(t => t.name === this.manualTool);
    if (tool) {
      let ctl=false, shift=false;
      for (const mod of this.modifiers) {
        if (mod === "ctl") ctl = true;
        else if (mod === "shift") shift = true;
      }
      if (ctl && shift && tool.ctlShift) nv = tool.ctlShift;
      else if (ctl && tool.ctl) nv = tool.ctl;
      else if (shift && tool.shift) nv = tool.shift;
    }
    if (nv === this.effectiveTool) return;
    this.effectiveTool = nv;
    this.broadcast({ type: "tool", tool: nv });
  }
  
  setMouse(x, y) {
    x = Math.floor(x);
    y = Math.floor(y);
    if (!this.map || (x < 0) || (x >= this.map.w) || (y < 0) || (y >= this.map.h)) x = y = -1;
    if ((x === this.mousex) && (y === this.mousey)) return;
    this.mousex = x;
    this.mousey = y;
    this.broadcast({ type: "mouse", x, y });
  }
  
  /* Impulse actions.
   *************************************************************************************/
  
  performAction(action) {
    if (!action.name.match(/^[a-z][a-zA-Z0-9_]*$/)) return;
    const fnname = "action_" + action.name;
    this[fnname]?.();
  }
  
  action_commands() {
    const modal = this.dom.spawnModal(CommandListEditor);
    modal.setup(this.map.cmd, "map");
    modal.ondirty = () => {
      this.broadcast({ type: "commands" });
    };
  }
  
  action_resize() {
    console.log(`MapPaint.action_resize`);//TODO
  }
  
  action_healAll() {
    console.log(`MapPaint.action_healAll`);//TODO
  }
  
  action_neighbors() {
    console.log(`MapPaint.action_neighbors`);//TODO
  }
}

MapPaint.singleton = false; // sic false, despite appearances.

/* Tool definitions.
 *******************************************************************/

MapPaint.TOOLS = [
  {
    name: "pencil",
    ctl: "pickup",
  },
  {
    name: "rainbow",
    ctl: "pickup",
  },
  {
    name: "monalisa",
    ctl: "pickup",
  },
  {
    name: "heal",
  },
  {
    name: "pickup",
  },
  {
    name: "marquee",
    // Don't assign ctlShift, ctl, or shift -- the tool uses them itself!
  },
  {
    name: "lasso",
    // Don't assign ctlShift, ctl, or shift -- the tool uses them itself!
  },
  {
    name: "poiedit",
    shift: "poimove",
  },
  {
    name: "poimove",
    // Uses shift for copy.
    ctl: "poiedit",
  },
  {
    name: "door",
    shift: "poimove",
    ctl: "poiedit",
  },
];

/* Impulse action definitions.
 **********************************************************************/
 
MapPaint.ACTIONS = [
  {
    name: "commands",
    label: "Commands...",
  },
  {
    name: "resize",
    label: "Resize...",
  },
  {
    name: "healAll",
    label: "Heal All",
  },
  {
    name: "neighbors",
    label: "Neighbors...",
  },
];
