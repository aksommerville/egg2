/* MapEditor.js
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { MapService } from "./MapService.js";
import { MapPaint } from "./MapPaint.js";
import { MapToolbar } from "./MapToolbar.js";
import { MapCanvas } from "./MapCanvas.js";

export class MapEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, MapService, MapPaint, Window];
  }
  constructor(element, dom, data, mapService, mapPaint, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.mapService = mapService;
    this.mapPaint = mapPaint;
    this.window = window;
    
    this.mapPaint.mapEditor = this;
    
    this.res = null;
    this.map = null;
    this.mapToolbar = null;
    this.mapCanvas = null;
    this.mapPaintListener = this.mapPaint.listen(e => this.onPaintEvent(e));
    this.keyListener = e => this.onKey(e);
    this.window.addEventListener("keydown", this.keyListener);
    this.window.addEventListener("keyup", this.keyListener);
  }
  
  onRemoveFromDom() {
    this.mapPaint.unlisten(this.mapPaintListener);
    this.mapPaint.destroy();
    if (this.keyListener) {
      this.window.removeEventListener("keydown", this.keyListener);
      this.window.removeEventListener("keyup", this.keyListener);
      this.keyListener = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "map") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.map = this.mapService.getByPath(res.path);
    this.mapPaint.setResource(this.res.path, this.map);
    this.mapToolbar = this.dom.spawnController(this.element, MapToolbar, [this.mapPaint]);
    this.mapCanvas = this.dom.spawnController(this.element, MapCanvas, [this.mapPaint]);
  }
  
  encode() {
    if (!this.mapPaint.selection) return this.mapPaint.map.encode();
    // It's safe to anchor, encode, and re-float the selection. Only thing is, content under the selection will be lost.
    this.mapPaint.selection.anchor(this.mapPaint.map);
    const dst = this.mapPaint.map.encode();
    this.mapPaint.selection.float(this.mapPaint.map);
    return dst;
  }
  
  onPaintEvent(event) {
    switch (event.type) {
      case "commands": this.data.dirty(this.res.path, () => this.encode()); break;
      case "cellDirty": this.data.dirty(this.res.path, () => this.encode()); break;
    }
  }
  
  onKey(event) {
    //console.log(`MapEditor.onKey ${event.type}:${event.code}`);
    
    // MapPaint needs to track modifier keys.
    if ((event.code === "ControlLeft") || (event.code === "ControlRight")) {
      this.mapPaint.setModifier("ctl", event.type === "keydown");
      event.preventDefault();
      event.stopPropagation();
      return;
    }
    if ((event.code === "ShiftLeft") || (event.code === "ShiftRight")) {
      this.mapPaint.setModifier("shift", event.type === "keydown");
      event.preventDefault();
      event.stopPropagation();
      return;
    }
    
    // If a modal is open or modifiers down, don't do anything else.
    if (event.ctrlKey || event.shiftKey || event.altKey) return;
    if (this.dom.document.querySelector("dialog")) return;
    
    // 1..0 and q..p select a tool.
    // This is way more tools than we actually have; plenty of room for future expansion.
    if ((event.type === "keydown") && !event.ctrlKey && !event.shiftKey) {
      let toolp = -1;
      switch (event.code) {
        case "Digit1": toolp = 0; break;
        case "Digit2": toolp = 2; break;
        case "Digit3": toolp = 4; break;
        case "Digit4": toolp = 6; break;
        case "Digit5": toolp = 8; break;
        case "Digit6": toolp = 10; break;
        case "Digit7": toolp = 12; break;
        case "Digit8": toolp = 14; break;
        case "Digit9": toolp = 16; break;
        case "Digit0": toolp = 18; break;
        case "KeyQ": toolp = 1; break;
        case "KeyW": toolp = 3; break;
        case "KeyE": toolp = 5; break;
        case "KeyR": toolp = 7; break;
        case "KeyT": toolp = 9; break;
        case "KeyY": toolp = 11; break;
        case "KeyU": toolp = 13; break;
        case "KeyI": toolp = 15; break;
        case "KeyO": toolp = 17; break;
        case "KeyP": toolp = 19; break;
        case "Escape": this.mapPaint.dropSelection(); break;
        case "Space": this.mapToolbar?.onClickPalette(); break;
      }
      if ((toolp >= 0) && (toolp < MapPaint.TOOLS.length)) {
        this.mapPaint.setTool(MapPaint.TOOLS[toolp].name);
        event.stopPropagation();
        event.preventDefault();
        return;
      }
    }
  }
}
