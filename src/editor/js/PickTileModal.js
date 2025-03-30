/* PickTileModal.js
 * Invites the user to pick a tile from a 16x16 tilesheet.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class PickTileModal {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data, Window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.image = null;
    this.tileid = -1;
    this.renderTimeout = null;
    this.hoverp = -1;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(image, tileid) {
    this.image = image;
    this.tileid = tileid;
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", {
      "on-mouseenter": e => this.onMotion(e),
      "on-mouseleave": e => this.onMotion(e),
      "on-mousemove": e => this.onMotion(e),
      "on-click": e => this.onClick(e),
    });
    this.renderSoon();
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.font = "10pt monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    
    if (this.image) {
      ctx.drawImage(this.image, 0, 0, this.image.naturalWidth, this.image.naturalHeight, 0, 0, bounds.width, bounds.height);
    }
    
    this.highlightTile(ctx, bounds, this.tileid, "#fc0", "", "");
    this.highlightTile(ctx, bounds, this.hoverp, "", "#0f0", "0x" + this.hoverp.toString(16).padStart(2, '0'));
  }
  
  highlightTile(ctx, bounds, tileid, fill, stroke, tattle) {
    if ((tileid < 0) || (tileid > 0xff)) return;
    const tilesize = bounds.width >> 4;
    const col = tileid & 15;
    const row = tileid >> 4;
    const hlx = col * tilesize;
    const hly = row * tilesize;
    if (fill) {
      ctx.fillStyle = fill;
      ctx.globalAlpha = 0.500;
      ctx.fillRect(hlx, hly, tilesize, tilesize);
      ctx.globalAlpha = 1;
    }
    if (stroke) {
      ctx.strokeStyle = "#0f0";
      ctx.lineWidth = 4;
      ctx.globalAlpha = 0.800;
      ctx.strokeRect(hlx + 2, hly + 2, tilesize - 4, tilesize - 4);
      ctx.globalAlpha = 1;
    }
    if (tattle) {
      const dsth = 15;
      const dsty = row ? (row * tilesize - dsth) : tilesize;
      ctx.globalAlpha = 0.750;
      ctx.fillStyle = "#fff";
      ctx.fillRect(hlx, dsty, tilesize, dsth);
      ctx.globalAlpha = 1;
      ctx.fillStyle = "#000";
      ctx.fillText(tattle, hlx + (tilesize >> 1), dsty + (dsth >> 1) + 2);
    }
  }
  
  onMotion(event) {
    const canvas = event.target;
    if (!canvas || (canvas.tagName !== "CANVAS")) return;
    const bounds = canvas.getBoundingClientRect();
    const col = Math.floor(((event.x - bounds.x) * 16) / bounds.width);
    const row = Math.floor(((event.y - bounds.y) * 16) / bounds.height);
    const tileid = ((col < 0) || (row < 0) || (col >= 16) || (row >= 16)) ? -1 : ((row << 4) | col);
    if (tileid === this.hoverp) return;
    this.hoverp = tileid;
    this.renderSoon();
  }
  
  onClick(event) {
    if ((this.hoverp >= 0) && (this.hoverp < 0x100)) this.resolve(this.hoverp);
    else this.resolve(null);
    this.element.remove();
  }
}
