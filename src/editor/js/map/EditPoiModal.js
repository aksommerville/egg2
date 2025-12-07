/* EditPoiModal.js
 */
 
import { Dom } from "../Dom.js";
import { MapService } from "./MapService.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { Data } from "../Data.js";
import { CommandList } from "../std/CommandList.js";

export class EditPoiModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MapService, SharedSymbols, Window, Data];
  }
  constructor(element, dom, mapService, sharedSymbols, window, data) {
    this.element = element;
    this.dom = dom;
    this.mapService = mapService;
    this.sharedSymbols = sharedSymbols;
    this.window = window;
    this.data = data;
    
    this.poi = null;
    this.map = null;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* (map) is the map you are editing. Not necessarily the one where the command lives.
   * (poi) is optional; null to create a new one.
   * (x,y) only relevant for new poi.
   */
  setup(poi, map, x, y) {
    this.poi = poi;
    this.map = map;
    if (!this.poi) {
      this.buildUiNew(x, y);
    } else if (this.poi.mapid !== this.map.rid) {
      this.buildUiRemote();
    } else {
      this.buildUiLocal();
    }
  }
  
  /* UI.
   *******************************************************************************/
   
  buildUiNew(x, y) {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    const macros = this.dom.spawn(form, "DIV", ["row"]);
    
    // Offer sprite shortcuts if applicable.
    // This is more than just saving the user some typing: There can be type-specific arguments that are easy to forget, and we can prompt them.
    if (this.sharedSymbols.getValue("CMD", "map", "sprite")) {
      // If there are sprite resources, offer a list of proposals.
      const sprites = this.data.resv.filter(r => (r.type === "sprite"));
      if (sprites.length) {
        sprites.sort((a, b) => a.rid - b.rid);
        const select = this.dom.spawn(macros, "SELECT", { name: "sprite", "on-change": e => this.onSelectSprite(e) });
        this.dom.spawn(select, "OPTION", { value: "", disabled: true }, "Sprite...");
        for (const res of sprites) {
          this.dom.spawn(select, "OPTION", { value: res.rid }, res.path.replace(/^.*\//, ""));
        }
        select.value = "";
      }
    }
    
    // Offer door shortcuts if applicable.
    // There is a separate Door tool that does a slightly better job of it, but hey we're already here, let's do this.
    if (this.sharedSymbols.getValue("CMD", "map", "door")) {
      const maps = this.data.resv.filter(r => (r.type === "map"));
      if (maps.length > 1) {
        maps.sort((a, b) => (a.path < b.path) ? -1 : (a.path > b.path) ? 1 : 0);
        const select = this.dom.spawn(macros, "SELECT", { name: "door", "on-change": e => this.onSelectDoor(e) });
        this.dom.spawn(select, "OPTION", { value: "", disabled: true }, "Door to...");
        for (const res of maps) {
          if (res.rid === this.map.rid) continue;
          this.dom.spawn(select, "OPTION", { value: res.rid }, res.path.replace(/^.*\//, ""));
        }
        select.value = "";
      }
    }
    
    const input = this.dom.spawn(form, "INPUT", { name: "cmd", value: `KEYWORD @${x},${y}` });
    
    const buttons = this.dom.spawn(form, "DIV", ["row"]);
    this.dom.spawn(buttons, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    
    this.window.setTimeout(() => {
      input.focus();
      input.select();
    }, 20);
  }
   
  buildUiRemote() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", ["row"], `Editing command in map:${this.poi.mapid}.`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});

    const input = this.dom.spawn(form, "INPUT", { name: "cmd", value: this.poi.cmd.join(" ") });
    
    const buttons = this.dom.spawn(form, "DIV", ["row"]);
    this.dom.spawn(buttons, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    
    this.window.setTimeout(() => {
      input.focus();
      input.select();
    }, 20);
  }
   
  buildUiLocal() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});

    const input = this.dom.spawn(form, "INPUT", { name: "cmd", value: this.poi.cmd.join(" ") });
    
    const buttons = this.dom.spawn(form, "DIV", ["row"]);
    this.dom.spawn(buttons, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
    
    this.window.setTimeout(() => {
      input.focus();
      input.select();
    }, 20);
  }
  
  /* Events.
   ******************************************************************************/
   
  onSelectSprite(event) {
    const select = event.target;
    const rid = +select.value;
    const res = this.data.resv.find(r => ((r.type === "sprite") && (r.rid === rid)));
    const textInput = this.element.querySelector("input[name='cmd']");
    const textBefore = textInput.value;
    const location = textBefore.split(/\s+/g)[1] || "@0,0";
    const commandList = new CommandList(res.serial);
    const typeArg = commandList.getFirstArg("type") || "";
    // To be useful to us, (typeArg) must be "(u16:sprtype)TYPE_NAME". But tolerate anything.
    let addl = "";
    const match = typeArg.match(/^\(u16:([a-zA-Z_][0-9a-zA-Z_]+)\)(.*)$/);
    if (match) {
      const ns = match[1];
      const name = match[2];
      addl = this.sharedSymbols.getComment("NS", ns, name);
    }
    textInput.value = `sprite ${location} sprite:${res.name || res.rid} ${addl}`;
    textInput.focus();
    textInput.select();
    select.value = "";
  }
  
  onSelectDoor(event) {
    const select = event.target;
    const rid = +select.value;
    const res = this.data.resv.find(r => ((r.type === "map") && (r.rid === rid)));
    const textInput = this.element.querySelector("input[name='cmd']");
    // We assume that CMD_map_door has its default structure: "u16:location u16:mapid u16:dstlocation u16:arg"
    const textBefore = textInput.value.split(/\s+/g);
    const location = textBefore[1] || "@0,0";
    const arg = textBefore[3] || "(u16)0";
    textInput.value = `door ${location} map:${res.name || res.rid} ${location} ${arg}`;
    textInput.focus();
    textInput.select();
    select.value = "";
  }
  
  onSubmit(event) {
    event.preventDefault();
    const text = this.element.querySelector("input[name='cmd']").value;
    this.resolve(text);
    this.element.remove();
  }
}
