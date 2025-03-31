/* MapCanvas.js
 * Main body of MapEditor. Shows the cells and manages scroll, zoom, and mouse interaction.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { MapService } from "./MapService.js";
import { MapPaint } from "./MapPaint.js";
import { TilesheetEditor } from "../std/TilesheetEditor.js";

export class MapCanvas {
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
    
    this.margin = 200; // Margin on all sides, in framebuffer pixels.
    this.edgeGap = 10; // Within (margin), a gap to accentuate the real map's edge.
    this.fbw = 100; // refreshed at render
    this.fbh = 100;
    this.scrollx = 0; // refreshed at render
    this.scrolly = 0;
    this.canvasBounds = null; // refreshed at render
    this.renderTimeout = null;
    this.scrollTimeout = null; // Debounce dispatch of scroll events, they come in hot and heavy.
    this.neighborImages = []; // {image,dx,dy} (dx,dy) in -1..1 and can't both be zero. (image) at natural size.
    this.acquireNeighborImages();
    this.buildUi();
    this.mapPaintListener = this.mapPaint.listen(e => this.onPaintEvent(e));
    this.refreshSizer();
    this.forceScrollerPosition();
  }
  
  onRemoveFromDom() {
    this.mapPaint.unlisten(this.mapPaintListener);
    if (this.renderTimeout) {
      this.window.cancelAnimationFrame(this.renderTimeout);
      this.renderTimeout = null;
    }
    if (this.scrollTimeout) {
      this.window.clearTimeout(this.scrollTimeout);
      this.scrollTimeout = null;
    }
  }
  
  /* UI.
   **************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "CANVAS", ["main"]);
    this.dom.spawn(this.element, "DIV", ["scroller"],
      { "on-scroll": e => this.onScroll(e) },
      { "on-wheel": e => this.onWheel(e) },
      { "on-pointermove": e => this.onMotion(e) },
      { "on-pointerenter": e => this.onMotion(e) },
      { "on-pointerleave": e => this.onMotion(e) },
      { "on-pointerdown": e => this.onMouseDown(e) },
      { "on-pointerup": e => this.onMouseUp(e) },
      this.dom.spawn(null, "DIV", ["sizer"])
    );
    this.renderSoon();
  }
  
  /* Render.
   **************************************************************************/
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.requestAnimationFrame(() => {
      this.renderTimeout = null;
      this.renderNow();
    });
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas.main");
    const bounds = canvas.getBoundingClientRect();
    this.canvasBounds = bounds;
    this.fbw = canvas.width = bounds.width;
    this.fbh = canvas.height = bounds.height;
    const scroller = this.element.querySelector(".scroller");
    this.scrollx = scroller.scrollLeft;
    this.scrolly = scroller.scrollTop;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    const nw = this.coordsMapFromElement(0, 0);
    const se = this.coordsMapFromElement(bounds.width, bounds.height);
    const tilesize = this.mapPaint.tilesize * this.mapPaint.zoom;
    
    const cola = Math.max(0, Math.floor(nw[0]));
    const colz = Math.min(this.mapPaint.map.w - 1, Math.floor(se[0]));
    const rowa = Math.max(0, Math.floor(nw[1]));
    const rowz = Math.min(this.mapPaint.map.h - 1, Math.floor(se[1]));
    let dsty = rowa * tilesize - this.scrolly + this.margin;
    const dstx0 = cola * tilesize - this.scrollx + this.margin;
    let rowp = rowa * this.mapPaint.map.w + cola;
    const phcolors = this.mapPaint.toggles.physics && TilesheetEditor.getColors();
    for (let row=rowa; row<=rowz; row++, dsty+=tilesize, rowp+=this.mapPaint.map.w) {
      for (let col=cola, dstx=dstx0, cellp=rowp; col<=colz; col++, dstx+=tilesize, cellp++) {
        if (this.mapPaint.image && this.mapPaint.toggles.image) {
          const srcx = (this.mapPaint.map.v[cellp] & 15) * this.mapPaint.tilesize;
          const srcy = (this.mapPaint.map.v[cellp] >> 4) * this.mapPaint.tilesize;
          ctx.drawImage(this.mapPaint.image, srcx, srcy, this.mapPaint.tilesize, this.mapPaint.tilesize, dstx, dsty, tilesize, tilesize);
        }
        if (this.mapPaint.toggles.poi) {
          //TODO poi badges
        }
        if (this.mapPaint.toggles.physics && this.mapPaint.tilesheet) {
          const ph = this.mapPaint.tilesheet.tables.physics?.[this.mapPaint.map.v[cellp]] || 0;
          ctx.fillStyle = phcolors[ph];
          ctx.globalAlpha = 0.750;
          ctx.fillRect(dstx + 2, dsty + 2, tilesize - 4, tilesize - 4);
          ctx.globalAlpha = 1;
        }
      }
    }
    
    //TODO Selection and provisional selection.
    
    if (this.mapPaint.toggles.grid) {
      ctx.beginPath();
      const xa = this.margin - this.scrollx;
      const xz = this.margin + this.mapPaint.map.w * tilesize - this.scrollx;
      const ya = this.margin - this.scrolly;
      const yz = this.margin + this.mapPaint.map.h * tilesize - this.scrolly;
      for (let col=cola, x=Math.floor(dstx0)+0.5; col<=colz+1; col++, x+=tilesize) {
        ctx.moveTo(x, ya);
        ctx.lineTo(x, yz);
      }
      for (let row=rowa, y=Math.floor(rowa*tilesize-this.scrolly+this.margin)+0.5; row<=rowz+1; row++, y+=tilesize) {
        ctx.moveTo(xa, y);
        ctx.lineTo(xz, y);
      }
      ctx.strokeStyle = "#0f0";
      ctx.lineWidth = 1;
      ctx.globalAlpha = 0.500;
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
    
    for (const ni of this.neighborImages) {
      const dstw = ni.image.width * this.mapPaint.zoom;
      const dsth = ni.image.height * this.mapPaint.zoom;
      let dstx;
      if (ni.dx < 0) dstx = this.margin - this.edgeGap - dstw;
      else if (ni.dx > 0) dstx = this.margin + this.mapPaint.map.w * this.mapPaint.tilesize + this.edgeGap;
      else dstx = this.margin;
      if (ni.dy < 0) dsty = this.margin - this.edgeGap - dsth;
      else if (ni.dy > 0) dsty = this.margin + this.mapPaint.map.h * this.mapPaint.tilesize + this.edgeGap;
      else dsty = this.margin;
      ctx.drawImage(ni.image, 0, 0, ni.image.width, ni.image.height, dstx, dsty, dstw, dsth);
    }
  }
  
  /* Neighbor images.
   **********************************************************************/
   
  acquireNeighborImages() {
    this.neighborImages = [];
    const depth = 6; // How many columns or rows to draw, in the direction away from home.
    this.mapService.forMapNeighbors(this.mapPaint.map, (map, dx, dy) => {
      let srcx=0, srcy=0, w=map.w, h=map.h;
      if (dx < 0) {
        w = depth;
        srcx = map.w - w;
      } else if (dx > 0) {
        w = depth;
      }
      if (dy < 0) {
        h = depth;
        srcy = map.h - h;
      } else if (dy > 0) {
        h = depth;
      }
      this.renderNeighborImage(map, srcx, srcy, w, h).then(image => {
        if (image) {
          this.neighborImages.push({ image, dx, dy });
        }
      });
    });
  }
  
  renderNeighborImage(map, x, y, w, h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    const iname = map.cmd.getFirstArg("image");
    if (!iname) return Promise.resolve(null);
    return this.data.getImageAsync(iname)
      .catch(() => null)
      .then((srcimg) => {
        if (!srcimg) return null;
        const tilesize = srcimg.naturalWidth >> 4;
        const dstimg = this.dom.spawn(null, "CANVAS");
        dstimg.width = w * tilesize;
        dstimg.height = h * tilesize;
        const dstctx = dstimg.getContext("2d");
        for (let dsty=0, yi=h, row=y; yi-->0; dsty+=tilesize, row++) {
          for (let dstx=0, xi=w, col=x; xi-->0; dstx+=tilesize, col++) {
            const tileid = map.v[row * map.w + col];
            const srcx = (tileid & 15) * tilesize;
            const srcy = (tileid >> 4) * tilesize;
            dstctx.drawImage(srcimg, srcx, srcy, tilesize, tilesize, dstx, dsty, tilesize, tilesize);
          }
        }
        dstctx.fillStyle = "#888";
        dstctx.globalAlpha = 0.500;
        dstctx.fillRect(0, 0, dstimg.width, dstimg.height);
        return dstimg;
      });
  }
  
  /* Coordinate projection.
   *****************************************************************/
   
  coordsMapFromElement(ex, ey) {
    const tilesize = this.mapPaint.tilesize * this.mapPaint.zoom;
    return [
      (ex + this.scrollx - this.margin) / tilesize,
      (ey + this.scrolly - this.margin) / tilesize,
    ];
  }
  
  coordsElementFromMap(mx, my) {
    const tilesize = this.mapPaint.tilesize * this.mapPaint.zoom;
    return [
      mx * tilesize - this.scrollx + this.margin,
      my * tilesize - this.scrolly + this.margin,
    ];
  }
  
  refreshSizer() {
    const sizer = this.element.querySelector(".sizer");
    sizer.style.width = (this.mapPaint.map.w * this.mapPaint.tilesize * this.mapPaint.zoom + this.margin * 2) + "px";
    sizer.style.height = (this.mapPaint.map.h * this.mapPaint.tilesize * this.mapPaint.zoom + this.margin * 2) + "px";
    this.renderSoon();
  }
  
  forceScrollerPosition() {
    const scroller = this.element.querySelector(".scroller");
    scroller.scrollLeft = this.mapPaint.scrollx;
    scroller.scrollTop = this.mapPaint.scrolly;
  }
  
  /* For a vert-wheel-plus-control event, call this first to record the pointer's position in the map, and also its position on screen.
   * Whatever we return, deliver that to applyZoomAnchor after effecting the zoom and size change.
   */
  captureZoomAnchor(event) {
    if (!event || !this.canvasBounds) return null;
    const canvasPosition = [event.x - this.canvasBounds.x, event.y - this.canvasBounds.y];
    const mapPosition = this.coordsMapFromElement(canvasPosition[0], canvasPosition[1]);
    return { canvasPosition, mapPosition };
  }
  
  applyZoomAnchor(anchor) {
    if (!anchor) return;
    const tsInMap = this.mapPaint.tilesize * this.mapPaint.zoom;
    const mapViewW = this.canvasBounds.width / tsInMap;
    const mapViewH = this.canvasBounds.height / tsInMap;
    const mx = anchor.mapPosition[0] - ((mapViewW * anchor.canvasPosition[0]) / this.canvasBounds.width);
    const my = anchor.mapPosition[1] - ((mapViewH * anchor.canvasPosition[1]) / this.canvasBounds.height);
    const scroller = this.element.querySelector(".scroller");
    scroller.scrollLeft = this.margin + mx * tsInMap;
    scroller.scrollTop = this.margin + my * tsInMap;
  }
  
  /* Events.
   **************************************************************************/
   
  onPaintEvent(event) {
    switch (event.type) {
      case "image": this.renderSoon(); break;
      case "map": this.refreshSizer(); this.forceScrollerPosition(); break;
      case "zoom": this.refreshSizer(); break;
      case "cellDirty": this.renderSoon(); break;
      case "toggle": this.renderSoon(); break;
    }
  }
  
  onScroll(event) {
    if (!this.scrollTimeout) this.scrollTimeout = this.window.setTimeout(() => {
      this.scrollTimeout = null;
      this.mapPaint.setScroll(event.target.scrollLeft, event.target.scrollTop);
    }, 250);
    this.renderSoon();
    this.mapPaint.setMouse(-1, -1);
  }
  
  onWheel(event) {
    if (!event.ctrlKey) return;
    event.preventDefault();
    event.stopPropagation();
    // I get quanta of 120 per click, but I can't find that documented anywhere.
    // The spec says it should be in pixels -- pixels! -- which is such a stupid idea I don't even know how to mock it.
    // Rather than trying to get fancy about it, we'll just assume that each event is one click of the wheel.
    const adjust = 1.500; // 1 is noop .. 2 is too much
    const anchor = this.captureZoomAnchor(event);
    if (event.wheelDeltaY < 0) {
      this.mapPaint.setZoom(Math.max(0.01, this.mapPaint.zoom / adjust));
    } else if (event.wheelDeltaY > 0) {
      this.mapPaint.setZoom(Math.min(100, this.mapPaint.zoom * adjust));
    }
    this.applyZoomAnchor(anchor);
  }
  
  onMotion(event) {
    if (event.type === "pointerleave") {
      this.mapPaint.setMouse(-1, -1);
      return;
    }
    if (!this.canvasBounds) return;
    const mp = this.coordsMapFromElement(event.x - this.canvasBounds.x, event.y - this.canvasBounds.y);
    if (!this.mapPaint.setMouse(mp[0], mp[1])) return;
  }
  
  onMouseDown(event) {
    if (event.button !== 0) return;
    event.target.setPointerCapture(event.pointerId);
    this.mapPaint.onMouseDown();
  }
  
  onMouseUp(event) {
    if (event.button !== 0) return;
    this.mapPaint.onMouseUp();
  }
}
