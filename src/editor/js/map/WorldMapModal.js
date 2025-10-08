/* WorldMapModal.js
 * Shows all map resources arranged to expose neighbor relationships.
 * MapService already generates a model amenable to this.
 */
 
import { Dom } from "../Dom.js";
import { MapService, MapLayer } from "./MapService.js";
import { Actions } from "../Actions.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { Data } from "../Data.js";

export class WorldMapModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, MapService, Actions, Window, SharedSymbols, Data];
  }
  constructor(element, dom, mapService, actions, window, sharedSymbols, data) {
    this.element = element;
    this.dom = dom;
    this.mapService = mapService;
    this.actions = actions;
    this.window = window;
    this.sharedSymbols = sharedSymbols;
    this.data = data;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    /* (mapw,maph) are required for games that support map neighboring.
     * If they aren't globally defined, show an apology instead.
     */
    this.mapw = this.sharedSymbols.getValue("NS", "sys", "mapw");
    this.maph = this.sharedSymbols.getValue("NS", "sys", "maph");
    if (!this.mapw || !this.maph) {
      this.dom.spawn(this.element, "DIV", "This game doesn't use map neighboring (NS_sys_mapw or NS_sys_maph undefined).");
      return;
    }
    
    // Acquire the logical model, arrange the layers without overlap.
    this.generateMap();
    
    // (tilesize) is usually set in SharedSymbols, but if not, we can make something up.
    // Select (dsttilesize), ie zoom level, based on some reasonable opinions: Never more than 1x, never less than 1 px/tile, aim to keep the total size under 1000px.
    this.tilesize = this.sharedSymbols.getValue("NS", "sys", "tilesize") || 16;
    this.dsttilesize = this.tilesize;
    let canvasw = this.worldw * this.mapw * this.dsttilesize;
    let canvash = this.worldh * this.maph * this.dsttilesize;
    while ((this.dsttilesize > 1) && ((canvasw > 1000) || (canvash > 1000))) {
      this.dsttilesize >>= 1;
      canvasw >>= 1;
      canvash >>= 1;
    }
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* Generate the logical world map.
   **************************************************************************************************/
  
  /* Make a private copy of (mapService.layout) with empty layers eliminated and edges cropped.
   * There shouldn't be any empties, we just do that because it's easy.
   * Empty margins will almost always exist; that's an important optimization for MapService, which is mutable.
   * We're immutable, and really need exact boundaries for each layer.
   */
  generateMap() {
  
    // Copy (mapService.layout), trimming all margins.
    this.layers = this.mapService.layout.map(layer => layer.crop()).filter(v => v);
    
    // Put the first layer at (0,0) and pack the others around it, leaving one space between all layers.
    for (let i=0; i<this.layers.length; i++) {
      this.chooseLayerPosition(this.layers[i], this.layers, i);
    }
    
    // Measure the total extents, in maps.
    this.worldw = 0;
    this.worldh = 0;
    for (const layer of this.layers) {
      this.worldw = Math.max(this.worldw, layer.x + layer.w);
      this.worldh = Math.max(this.worldh, layer.y + layer.h);
    }
  }
  
  /* Replace (layer.x,y) to agree with the first (readyc) layers in (ready).
   * Part of generateMap().
   */
  chooseLayerPosition(layer, ready, readyc) {
    // First layer goes at (0,0).
    layer.x = 0;
    layer.y = 0;
    /* Subsequent layers go adjacent to an existing one.
     * Either rightward matching top or downward matching left.
     * First valid position wins.
     * This produces legal but not optimal packing. I don't think we care whether it's optimal.
     */
    for (let i=0; i<readyc; i++) {
      const q = ready[i];
      layer.x = q.x + q.w + 1;
      layer.y = q.y;
      if (this.positionValid(layer, ready, readyc)) return;
      layer.x = q.x;
      layer.y = q.y + q.h + 1;
      if (this.positionValid(layer, ready, readyc)) return;
    }
    // Impossible to land here unless (readyc) is zero.
  }
  
  positionValid(layer, ready, readyc) {
    for (let i=readyc; i-->0; ) {
      const q = ready[i];
      // False if we are overlapping *or adjacent*. A 1-cell margin is required everywhere.
      if (layer.x <= q.x + q.w) return false;
      if (layer.y <= q.y + q.h) return false;
      if (layer.x + layer.w >= q.x) return false;
      if (layer.y + layer.h >= q.y) return false;
    }
    return true;
  }
  
  /* Render.
   *****************************************************************************************/
   
  render(canvas) {
    const mdstw = Math.floor(this.mapw * this.dsttilesize);
    const mdsth = Math.floor(this.maph * this.dsttilesize);
    canvas.width = this.worldw * mdstw;
    canvas.height = this.worldh * mdsth;
    const ctx = canvas.getContext("2d");
    
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    for (const layer of this.layers) {
      for (let vp=0, yi=0; yi<layer.h; yi++) {
        for (let xi=0; xi<layer.w; xi++, vp++) {
          const res = layer.v[vp];
          if (!res) continue;
          this.renderMap(ctx, (layer.x + xi) * mdstw, (layer.y + yi) * mdsth, res.map);
        }
      }
    }
  }
  
  renderMap(ctx, dstx, dsty, map) {
    const imageName = map.cmd.getFirstArg("image");
    this.data.getImageAsync(imageName)
      .then(image => this.renderMapWithImage(ctx, dstx, dsty, map, image))
      .catch(e => console.log(e));
  }
  
  renderMapWithImage(ctx, dstx, dsty, map, image) {
    const w = Math.min(this.mapw, map.w);
    for (let y=dsty, yi=this.maph, rowp=0; yi-->0; rowp+=map.w, y+=this.dsttilesize) {
      for (let x=dstx, xi=w, cellp=rowp; xi-->0; cellp++, x+=this.dsttilesize) {
        const tileid = map.v[cellp] || 0;
        const srcx = (tileid & 15) * this.tilesize;
        const srcy = (tileid >> 4) * this.tilesize;
        ctx.drawImage(image, srcx, srcy, this.tilesize, this.tilesize, x, y, this.dsttilesize, this.dsttilesize);
      }
    }
  }
  
  /* UI.
   ******************************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const canvas = this.dom.spawn(this.element, "CANVAS", { "on-click": e => this.onClick(e) });
    this.render(canvas);
  }
  
  onClick(event) {
    const canvas = event.target;
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    const y = event.y - bounds.y;
    const abscol = x / (this.mapw * this.dsttilesize); // Click position in maps, comparable to (this.layers).
    const absrow = y / (this.maph * this.dsttilesize);
    for (const layer of this.layers) {
      const mx = Math.floor(abscol - layer.x);
      if ((mx < 0) || (mx >= layer.w)) continue;
      const my = Math.floor(absrow - layer.y);
      if ((my < 0) || (my >= layer.h)) continue;
      const res = layer.v[my * layer.w + mx];
      if (!res) return; // Clicked in a vacancy which is nevertheless covered by a layer.
      this.actions.editResource(res.path);
      this.resolve(null);
      this.element.remove();
      return;
    }
  }
}
