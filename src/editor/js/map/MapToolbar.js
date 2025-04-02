/* MapToolbar.js
 * Widgets along the top of MapEditor.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { PickTileModal } from "../PickTileModal.js";
import { MapService } from "./MapService.js";
import { MapPaint } from "./MapPaint.js";

export class MapToolbar {
  static getDependencies() {
    return [HTMLElement, Dom, Data, MapService, MapPaint];
  }
  constructor(element, dom, data, mapService, mapPaint) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.mapService = mapService;
    this.mapPaint = mapPaint;
    
    this.buildUi();
    this.populateUi();
    
    this.mapPaintListener = this.mapPaint.listen(e => this.onPaintEvent(e));
  }
  
  onRemoveFromDom() {
    this.mapPaint.unlisten(this.mapPaintListener);
  }
  
  /* UI.
   ************************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    this.dom.spawn(this.element, "CANVAS", ["palette"], { "on-click": () => this.onClickPalette() });
    
    const toolbox = this.dom.spawn(this.element, "DIV", ["toolbox"]);
    for (const tool of MapPaint.TOOLS) {
      const button = this.dom.spawn(toolbox, "DIV", ["tool"], { "data-name": tool.name, "on-click": () => this.onClickTool(tool.name) });
      if (tool.name === this.mapPaint.effectiveTool) button.classList.add("selected");
    }
    
    const actionsMenu = this.dom.spawn(this.element, "SELECT", ["actions"], { "on-change": () => this.onAction() });
    this.dom.spawn(actionsMenu, "OPTION", { value: "", disabled: "disabled", selected: "selected" }, "Actions...");
    for (const action of MapPaint.ACTIONS) {
      this.dom.spawn(actionsMenu, "OPTION", { value: action.name }, action.label);
    }
    
    const vzone = this.dom.spawn(this.element, "DIV", ["vzone"]);
    const toggles = this.dom.spawn(vzone, "DIV", ["toggles"], { "on-change": () => this.onToggle() });
    for (const toggleName of ["image", "grid", "poi", "physics", "joinOutside"]) {
      const id = `MapToolbar-${this.nonce}-toggle-${toggleName}`;
      const input = this.dom.spawn(toggles, "INPUT", ["toggle"], { id, type: "checkbox", name: toggleName });
      input.checked = this.mapPaint.toggles[toggleName];
      this.dom.spawn(toggles, "LABEL", ["toggle"], { for: id }, toggleName);
    }
    
    const tattles = this.dom.spawn(vzone, "DIV", ["tattles"]);
    this.dom.spawn(tattles, "DIV", ["tattle", "position"], "");
    this.dom.spawn(tattles, "DIV", ["tattle", "detail"], "");
  }
  
  populateUi() {
    this.drawPalette();
  }
  
  drawPalette() {
    const palette = this.element.querySelector("canvas.palette");
    const ctx = palette.getContext("2d");
    if (this.mapPaint.image) {
      const tilesize = this.mapPaint.image.naturalWidth >> 4;
      palette.width = tilesize;
      palette.height = tilesize;
      ctx.clearRect(0, 0, tilesize, tilesize);
      const srcx = (this.mapPaint.tileid & 15) * tilesize;
      const srcy = (this.mapPaint.tileid >> 4) * tilesize;
      ctx.drawImage(this.mapPaint.image, srcx, srcy, tilesize, tilesize, 0, 0, tilesize, tilesize);
    } else {
      ctx.clearRect(0, 0, palette.width, palette.height);
    }
  }
  
  refreshToolHighlights() {
    for (const element of this.element.querySelectorAll(".toolbox .selected")) element.classList.remove("selected");
    const button = this.element.querySelector(`.toolbox .tool[data-name='${this.mapPaint.effectiveTool}']`);
    if (button) button.classList.add("selected");
  }
  
  setPositionTattle(x, y) {
    const tattle = this.element.querySelector(".tattle.position");
    if (x < 0) tattle.innerText = "";
    else tattle.innerText = x.toString().padStart(3) + "," + y.toString().padStart(3);
  }
  
  refreshDetailTattle() {
    const tattle = this.element.querySelector(".tattle.detail");
    tattle.innerText = "";
    
    // When a selection is being established, describe the box or cell count.
    // Egg v1 had a Pedometer tool, which I want to roll into the lasso.
    if (this.mapPaint.tempSelection) {
      if (this.mapPaint.toolInProgress === "lasso") {
        tattle.innerText = `${this.mapPaint.tempSelection.countCells()} cells`;
      } else {
        const s = this.mapPaint.tempSelection;
        tattle.innerText = `${s.x},${s.y},${s.w},${s.h} = ${s.w * s.h} cells`;
      }
      return;
    }
    
    // With any POI tool selected, show details for the hovered POI.
    if (
      (this.mapPaint.effectiveTool === "poiedit") ||
      (this.mapPaint.effectiveTool === "poimove") ||
      (this.mapPaint.effectiveTool === "door")
    ) {
      const poi = this.mapPaint.getFocusPoi();
      if (poi) tattle.innerText = this.describePoi(poi);
      return;
    }
    
    // Surely there's more worth showing up here.
  }
  
  describePoi(poi) {
    if (poi.mapid !== this.mapPaint.map.rid) {
      return `[from map:${poi.mapid}] ` + poi.cmd.join(" ");
    }
    return poi.cmd.join(" ");
  }
  
  /* Events.
   ********************************************************************************/
   
  onClickPalette() {
    const modal = this.dom.spawnModal(PickTileModal);
    modal.setup(this.mapPaint.image, this.mapPaint.tileid);
    modal.result.then(tileid => {
      if (typeof(tileid) !== "number") return;
      this.mapPaint.setPalette(tileid);
    });
  }
  
  onClickTool(name) {
    this.mapPaint.setTool(name);
  }
  
  onAction() {
    const actionsMenu = this.element.querySelector("select.actions");
    const name = actionsMenu.value;
    actionsMenu.value = "";
    const action = MapPaint.ACTIONS.find(a => a.name === name);
    if (!action) return;
    this.mapPaint.performAction(action);
  }
  
  onPaintEvent(event) {
    switch (event.type) {
      case "tileid": this.drawPalette(); break;
      case "tool": this.refreshToolHighlights(); this.refreshDetailTattle(); break;
      case "image": this.drawPalette(); break;
      case "mouse": this.setPositionTattle(event.x, event.y); break;
      case "selectionDirty": this.refreshDetailTattle(); break;
      case "refreshDetailTattle": this.refreshDetailTattle(); break;
    }
  }
  
  onToggle() {
    for (const element of this.element.querySelectorAll(".toggles input.toggle")) {
      this.mapPaint.setToggle(element.getAttribute("name"), element.checked);
    }
  }
}
