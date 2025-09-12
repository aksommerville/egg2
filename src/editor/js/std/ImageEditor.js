/* ImageEditor.js
 * Nevermind the name, we do not and will not actually edit images. Plenty of other options for that.
 * We display images and provide test facilities for animation.
 * The animation config will live in localStorage.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { Tilesheet } from "./Tilesheet.js";
import { Decalsheet } from "./Decalsheet.js";
import { Animation, ImageAnimationService } from "./ImageAnimationService.js";

const ZOOM_LIMIT = 32; // Both out and in.
const SCROLL_PROPORTION = 0.125; // Regardless of zoom, scrolling moves by this fraction of the smaller visible dimension.
const ZOOM_RATE = 0.0625; // Linear adjustment of an exponential value.

export class ImageEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window, ImageAnimationService];
  }
  constructor(element, dom, data, window, imageAnimationService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.imageAnimationService = imageAnimationService;
    
    this.res = null;
    this.renderFullTimeout = null;
    this.renderPreviewTimeout = null;
    this.sliceData = null; // null|Tilesheet|Decalsheet
    this.animation = null; // null|Animation
    this.selectedFaceName = "";
    this.hovertile = null; // null|number|string -- NB don't test logical identity, use (hovertile===null) instead.
    this.tattle = null; // Element. We access it frequently enough that I don't want to query it every time.
    this.fullCanvas = null; // ''
    this.previewCanvas = null; // ''
    this.zoom = 0; // -1..0..1
    this.midx = 0; // Center of view, in natural image coords.
    this.midy = 0;
    this.framep = 0; // Currently displayed frame, when animating.
    this.animateTimeout = null;
    this.previewZoom = 4; // Naive, user must set manually.
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.renderFullTimeout) {
      this.window.clearTimeout(this.renderFullTimeout);
      this.renderFullTimeout = null;
    }
    if (this.renderPreviewTimeout) {
      this.window.clearTimeout(this.renderPreviewTimeout);
      this.renderPreviewTimeout = null;
    }
    if (this.animateTimeout) {
      this.window.clearTimeout(this.animateTimeout);
      this.animateTimeout = null;
    }
  }
  
  static checkResource(res) {
    switch (res.format) {
      case "png":
      case "gif":
      case "jpeg":
      case "jpg":
      case "bmp":
      case "ico":
        return 2;
    }
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.requireImage()
      .then(() => this.acquireSliceData())
      .then(() => this.acquireAnimation())
      .then(() => {
        this.populateUi();
        this.fitZoom();
      });
  }
  
  requireImage() {
    if (!this.res) return Promise.resolve();
    if (this.res.image) return Promise.resolve();
    return this.data.getImageAsync(this.res.rid).then(img => {
      if (!this.res.image) this.res.image = img; // In case our (res) is a copy.
    }).catch(() => {});
  }
  
  acquireSliceData() {
    this.sliceData = null;
    if (!this.res?.image) return Promise.resolve();
    let res;
    if (res = this.data.resv.find(r => ((r.rid === this.res.rid) && (r.type === "tilesheet")))) {
      this.sliceData = new Tilesheet(res.serial);
    } else if (res = this.data.resv.find(r => ((r.rid === this.res.rid) && (r.type === "decalsheet")))) {
      this.sliceData = new Decalsheet(res.serial);
    //TODO Maybe also detect fonts? One can imagine some value in previewing text.
    } else if (this.boundsSuggestTilesheet(this.res.image.naturalWidth, this.res.image.naturalHeight)) {
      this.sliceData = new Tilesheet(null);
    }
    return Promise.resolve();
  }
  
  acquireAnimation() {
    if (!this.res) return Promise.resolve();
    // We could check whether (sliceData) present. Would be strange to have an Animation but not a sliceData.
    // But if that does happen, let's at least display the animation data. (tho it won't be usable).
    return this.imageAnimationService.getAnimationForImage(this.res.path).then(animation => {
      if (animation) {
        this.animation = animation;
      } else {
        this.animation = new Animation(null);
      }
    });
  }
  
  // Square images whose dimension is a multiple of 16 can be treated as tilesheets.
  boundsSuggestTilesheet(w, h) {
    if (w !== h) return false;
    if (w < 16) return false;
    if (w & 15) return false;
    return true;
  }
  
  /* UI
   ***************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    const top = this.dom.spawn(this.element, "DIV", ["top"]);
    this.fullCanvas = this.dom.spawn(top, "CANVAS", ["full"], {
      "on-mousewheel": e => this.onFullWheel(e),
      "on-pointerdown": e => this.onFullDown(e),
      "on-pointerup": e => this.onFullUp(e),
      "on-pointermove": e => this.onFullMove(e),
      "on-pointerleave": e => this.onFullLeave(e),
      "on-contextmenu": e => e.preventDefault(),
    });
    const setup = this.dom.spawn(top, "DIV", ["setup"]);
    this.dom.spawn(setup, "DIV", ["resName"]);
    this.dom.spawn(setup, "DIV", ["resDimensions"]);
    this.dom.spawn(setup, "DIV", ["cameraRow"],
      this.dom.spawn(null, "TABLE",
        this.dom.spawn(null, "TR",
          this.dom.spawn(null, "TD", ["k"], "Zoom"),
          this.dom.spawn(null, "TD",
            this.dom.spawn(null, "INPUT", { type: "range", min: -1, max: 1, step: 1/64, name: "zoom", "on-input": e => this.onZoomChanged(e) })
          )
        ),
        this.dom.spawn(null, "TR",
          this.dom.spawn(null, "TD", ["k"], "Mid X"),
          this.dom.spawn(null, "TD",
            this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 4096, name: "midx", "on-input": e => this.onMidXChanged(e) })
          )
        ),
        this.dom.spawn(null, "TR",
          this.dom.spawn(null, "TD", ["k"], "Mid Y"),
          this.dom.spawn(null, "TD",
            this.dom.spawn(null, "INPUT", { type: "number", min: 0, max: 4096, name: "midy", "on-input": e => this.onMidYChanged(e) })
          )
        ),
        this.dom.spawn(null, "TR",
          this.dom.spawn(null, "TD", ["k"], "Preview Zoom"),
          this.dom.spawn(null, "TD",
            this.dom.spawn(null, "INPUT", { type: "range", min: 0, max: ZOOM_LIMIT, step: 1, name: "previewZoom", "on-input": e => this.onPreviewZoomChanged(e) })
          )
        )
      )
    );
    this.dom.spawn(setup, "DIV",
      this.dom.spawn(null, "INPUT", { type: "button", value: "Reset", "on-click": () => this.resetZoom() }),
      this.dom.spawn(null, "INPUT", { type: "button", value: "Fit", "on-click": () => this.fitZoom() })
    );
    this.tattle = this.dom.spawn(setup, "DIV", ["tattle"]);
    
    const bottom = this.dom.spawn(this.element, "DIV", ["bottom"]);
    this.previewCanvas = this.dom.spawn(bottom, "CANVAS", ["preview"]);
    const animlist = this.dom.spawn(bottom, "DIV", ["animlist"]);
    const animedit = this.dom.spawn(bottom, "DIV", ["animedit"]);
  }
  
  populateUi() {
  
    // Camera controls (upper right).
    if (this.res) {
      this.element.querySelector(".resName").innerText = this.res.path.replace(/^.*\//, '');
      if (this.res.image) {
        this.element.querySelector(".resDimensions").innerText = `${this.res.image.naturalWidth}x${this.res.image.naturalHeight}`;
      } else {
        this.element.querySelector(".resDimensions").innerText = "";
      }
    } else {
      this.element.querySelector(".resName").innerText = "";
      this.element.querySelector(".resDimensions").innerText = "";
    }
    this.populateCameraUi();
    this.tattle.innerText = "";
    
    // Canvases.
    this.renderFullSoon();
    this.renderPreviewSoon();
    
    // Animation list and details.
    this.populateAnimationList();
    this.populateAnimationDetails();
  }
  
  populateAnimationList() {
    const animlist = this.element.querySelector(".animlist");
    animlist.innerHTML = "";
    if (!this.animation) return;
    for (const face of this.animation.faces) {
      const cls = ["face"];
      if (face.name === this.selectedFaceName) cls.push("selected");
      this.dom.spawn(animlist, "DIV", cls, { "data-name": face.name, "on-click": () => this.onSelectFace(face.name) }, face.name);
    }
    this.dom.spawn(animlist, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddFace() });
  }
    
  populateAnimationDetails() {
    const animedit = this.element.querySelector(".animedit");
    animedit.innerHTML = "";
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    this.dom.spawn(animedit, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteFace(this.selectedFaceName) });
    const table = this.dom.spawn(animedit, "TABLE");
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Name"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "text", name: "name", value: face.name, "on-input": e => this.onFaceNameInput(e) })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Def delay"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", min: 0, name: "defdelay", value: face.delay, "on-input": e => this.onFaceDelayInput(e) })
      )
    );
    let framepi = 0;
    let reprtile = v => v;
    if (this.sliceData instanceof Tilesheet) reprtile = v => "0x" + v.toString(16).padStart(2, '0');
    if (face.frames) for (const frame of face.frames) {
      const framep = framepi; // closure
      const row = this.dom.spawn(animedit, "DIV", ["frame"]);
      const controls = this.dom.spawn(row, "DIV", ["controls"]);
      this.dom.spawn(controls, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteFrame(framep) });
      this.dom.spawn(controls, "INPUT", { type: "button", value: "^", "on-click": () => this.onMoveFrame(framep, -1) });
      this.dom.spawn(controls, "INPUT", { type: "button", value: "v", "on-click": () => this.onMoveFrame(framep, 1) });
      this.dom.spawn(controls, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddTile(framep) });
      this.dom.spawn(controls, "INPUT", { type: "number", min: 0, value: frame.delay || 0, "on-input": e => this.onDelayInput(e, framep) });
      const tiles = this.dom.spawn(row, "DIV", ["tiles"]);
      let tilepi = 0;
      for (const tile of frame.tiles) {
        const tilep = tilepi; // closure
        const trow = this.dom.spawn(tiles, "DIV", ["tile"]);
        this.dom.spawn(trow, "INPUT", { type: "button", value: "X", "on-click": () => this.onDeleteTile(framep, tilep) });
        this.dom.spawn(trow, "INPUT", { type: "button", value: "^", "on-click": () => this.onMoveTile(framep, tilep, -1) });
        this.dom.spawn(trow, "INPUT", { type: "button", value: "v", "on-click": () => this.onMoveTile(framep, tilep, 1) });
        this.dom.spawn(trow, "INPUT", { type: "text", value: reprtile(tile.tileid), "on-input": e => this.onTileidInput(e, framep, tilep) });
        this.dom.spawn(trow, "INPUT", { type: "number", value: tile.x || 0, "on-input": e => this.onXInput(e, framep, tilep) });
        this.dom.spawn(trow, "INPUT", { type: "number", value: tile.y || 0, "on-input": e => this.onYInput(e, framep, tilep) });
        this.dom.spawn(trow, "INPUT", { type: "number", value: tile.xform || 0, min: 0, max: 7, "on-input": e => this.onXformInput(e, framep, tilep) });
        tilepi++;
      }
      framepi++;
    }
    this.dom.spawn(animedit, "INPUT", { type: "button", value: "+", "on-click": () => this.onAddFrame() });
  }
  
  populateCameraUi() {
    this.element.querySelector("input[name='zoom']").value = this.zoom;
    this.element.querySelector("input[name='midx']").value = this.midx;
    this.element.querySelector("input[name='midy']").value = this.midy;
    this.element.querySelector("input[name='previewZoom']").value = this.previewZoom;
  }
  
  /* Render canvases.
   ***************************************************************************************/
   
  renderFullSoon() {
    if (this.renderFullTimeout) return;
    this.renderFullTimeout = this.window.setTimeout(() => {
      this.renderFullTimeout = null;
      this.renderFullNow();
    }, 50);
  }
  
  renderPreviewSoon() {
    if (this.renderPreviewTimeout) return;
    this.renderPreviewTimeout = this.window.setTimeout(() => {
      this.renderPreviewTimeout = null;
      this.renderPreviewNow();
    }, 50);
  }
  
  renderFullNow() {
    const canvas = this.fullCanvas;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    if (!this.res?.image) return;
    ctx.imageSmoothingEnabled = false;
    const zoom = this.getFullZoom();
    const dstw = Math.round(this.res.image.naturalWidth * zoom);
    const dsth = Math.round(this.res.image.naturalHeight * zoom);
    const dstx = (bounds.width >> 1) - this.midx * zoom;
    const dsty = (bounds.height >> 1) - this.midy * zoom;
    ctx.drawImage(this.res.image, 0, 0, this.res.image.naturalWidth, this.res.image.naturalHeight, dstx, dsty, dstw, dsth);
    
    // If a tile is hovered, highlight it gently.
    let hx=0, hy=0, hw=0, hh=0;
    if (typeof(this.hovertile) === "number") {
      const tilesize = this.res.image.naturalWidth >> 4;
      hx = (this.hovertile & 15) * tilesize;
      hy = (this.hovertile >> 4) * tilesize;
      hw = tilesize;
      hh = tilesize;
    } else if (typeof(this.hovertile) === "string") {
      const decal = this.sliceData?.decals?.find(d => d.id === this.hovertile);
      if (decal) {
        hx = decal.x;
        hy = decal.y;
        hw = decal.w;
        hh = decal.h;
      }
    }
    if ((hw > 0) && (hh > 0)) {
      hx = dstx + hx * zoom;
      hy = dsty + hy * zoom;
      hw *= zoom;
      hh *= zoom;
      ctx.fillStyle = "#0f0";
      ctx.globalAlpha = 0.250;
      ctx.fillRect(hx, hy, hw, hh);
      ctx.globalAlpha = 1.000;
      ctx.beginPath();
      ctx.moveTo(hx, hy);
      ctx.lineTo(hx + hw, hy);
      ctx.lineTo(hx + hw, hy + hh);
      ctx.lineTo(hx, hy + hh);
      ctx.lineTo(hx, hy);
      ctx.strokeStyle = "#0f0";
      ctx.stroke();
    }
  }
  
  renderPreviewNow() {
    const canvas = this.previewCanvas;
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    
    if (!this.res?.image) return;
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    const frame = face.frames?.[this.framep];
    if (!frame) return;
    
    for (const tile of frame.tiles) {
      const srcbounds = this.boundsForTileid(tile.tileid);
      if (!srcbounds) continue;
      const zoom = this.previewZoom;
      const dstw = srcbounds[2] * zoom;
      const dsth = srcbounds[3] * zoom;
      let dstx = (bounds.width >> 1) - (dstw >> 1) + ((tile.x || 0) * zoom);
      let dsty = (bounds.height >> 1) - (dsth >> 1) + ((tile.y || 0) * zoom);
      const halfw = dstw >> 1;
      const halfh = dsth >> 1;
      ctx.save();
      if (tile.xform & 4) { // SWAP
        const tmp = dstx;
        dstx = dsty;
        dsty = dstx;
      }
      ctx.translate(halfw + dstx, halfh + dsty);
      switch (tile.xform) {
        case 1: ctx.scale(-1, 1); break; // XREV
        case 2: ctx.scale(1, -1); break; // YREV
        case 3: ctx.scale(-1, -1); break; // XREV|YREV
        case 4: ctx.rotate(Math.PI / 2); ctx.scale(1, -1); break; // SWAP
        case 5: ctx.rotate(Math.PI / -2); break; // SWAP|XREV
        case 6: ctx.rotate(Math.PI / 2); break; // SWAP|YREV
        case 7: ctx.rotate(Math.PI / 2); ctx.scale(-1, 1); break; // SWAP|XREV|YREV
      }
      ctx.drawImage(this.res.image, srcbounds[0], srcbounds[1], srcbounds[2], srcbounds[3], -halfw, -halfh, dstw, dsth);
      ctx.restore();
    }
  }
  
  boundsForTileid(tileid) {
    if (this.sliceData instanceof Tilesheet) {
      const tilesize = this.res.image.naturalWidth >> 4;
      const x = (tileid & 15) * tilesize;
      const y = (tileid >> 4) * tilesize;
      return [x, y, tilesize, tilesize];
    } else if (this.sliceData instanceof Decalsheet) {
      const decal = this.sliceData.decals?.find(d => d.id === tileid);
      if (decal) return [decal.x, decal.y, decal.w, decal.h];
    }
    return null;
  }
  
  reschedulePreview() {
    if (this.animateTimeout) {
      this.window.clearTimeout(this.animateTimeout);
      this.animateTimeout = null;
    }
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return this.renderPreviewSoon();
    this.framep = 0;
    this.renderPreviewNow();
    this.scheduleNextFrame(face);
  }
  
  scheduleNextFrame(face) {
    if (!face || (face.frames.length < 2)) return;
    const frame = face.frames[this.framep];
    if (!frame) return;
    this.animateTimeout = this.window.setTimeout(() => {
      this.animateTimeout = null;
      this.framep++;
      if (this.framep >= face.frames.length) this.framep = 0;
      this.renderPreviewNow();
      this.scheduleNextFrame(face);
    }, frame.delay || face.delay || 100);
  }
  
  // We store zoom in normalized exponential form, -1..1. This returns it as a simple positive multiplier.
  // So the base in this expression is the zoom limit, both In and Out.
  getFullZoom() {
    return ZOOM_LIMIT ** this.zoom;
  }
  
  /* Project (event)'s position on to the image and capture some related data.
   * Do this before changing zoom, then fitAnchor() with the result after,
   * and we will try to keep the cursor pointing at the same image pixel.
   */
  getAnchor(event) {
    if (!this.res?.image) return null;
    const canvas = event.target;
    const bounds = canvas.getBoundingClientRect();
    const uix = event.x - bounds.x;
    const uiy = event.y - bounds.y;
    const zoom = this.getFullZoom();
    const dstw = Math.round(this.res.image.naturalWidth * zoom);
    const dsth = Math.round(this.res.image.naturalHeight * zoom);
    const dstx = (bounds.width >> 1) - this.midx * zoom;
    const dsty = (bounds.height >> 1) - this.midy * zoom;
    const imgx = Math.round(((uix - dstx) * this.res.image.naturalWidth) / dstw);
    const imgy = Math.round(((uiy - dsty) * this.res.image.naturalHeight) / dsth);
    return { uix, uiy, imgx, imgy };
  }
  
  // We don't update camera UI or rerender, on the assumption that the caller is doing those anyway.
  fitAnchor(anchor) {
    if (!anchor) return;
    // Project the original ui point under the new zoom, and the difference from the old img point is what we need to add to 'mid'.
    const canvas = this.fullCanvas;
    const bounds = canvas.getBoundingClientRect();
    const zoom = this.getFullZoom();
    const dstw = Math.round(this.res.image.naturalWidth * zoom);
    const dsth = Math.round(this.res.image.naturalHeight * zoom);
    const dstx = (bounds.width >> 1) - this.midx * zoom;
    const dsty = (bounds.height >> 1) - this.midy * zoom;
    const imgx = Math.round(((anchor.uix - dstx) * this.res.image.naturalWidth) / dstw);
    const imgy = Math.round(((anchor.uiy - dsty) * this.res.image.naturalHeight) / dsth);
    this.midx -= imgx - anchor.imgx;
    this.midy -= imgy - anchor.imgy;
  }
  
  /* Events.
   ***************************************************************************/
   
  resetZoom() {
    if (this.res.image) {
      this.midx = this.res.image.naturalWidth >> 1;
      this.midy = this.res.image.naturalHeight >> 1;
    } else {
      this.midx = 0;
      this.midy = 0;
    }
    this.zoom = 0;
    this.populateCameraUi();
    this.renderFullSoon();
  }
  
  fitZoom() {
    if (this.res.image) {
      this.midx = this.res.image.naturalWidth >> 1;
      this.midy = this.res.image.naturalHeight >> 1;
      const canvas = this.fullCanvas;
      const bounds = canvas.getBoundingClientRect();
      if ((bounds.width > 0) && (bounds.height > 0)) {
        const xscale = bounds.width / this.res.image.naturalWidth;
        const yscale = bounds.height / this.res.image.naturalHeight;
        // Math.floor() here is debatable. I prefer integer multiples only.
        const scale = Math.max(1, Math.floor(Math.min(xscale, yscale)));
        this.zoom = Math.max(-1, Math.min(1, Math.log(scale) / Math.log(ZOOM_LIMIT)));
      } else {
        this.zoom = 0;
      }
    } else {
      this.zoom = 0;
      this.midx = 0;
      this.midy = 0;
    }
    this.populateCameraUi();
    this.renderFullSoon();
  }
  
  onZoomChanged(event) {
    this.zoom = event.target.value;
    this.renderFullSoon();
  }
  
  onMidXChanged(event) {
    this.midx = event.target.value;
    this.renderFullSoon();
  }
  
  onMidYChanged(event) {
    this.midy = event.target.value;
    this.renderFullSoon();
  }
  
  onPreviewZoomChanged(event) {
    this.previewZoom = +event.target.value;
    this.renderPreviewSoon();
  }
  
  scrollFull(dx, dy) {
    const canvas = this.fullCanvas;
    const bounds = canvas.getBoundingClientRect();
    const zoom = this.getFullZoom();
    // Try to scroll by 1/8 (SCROLL_PROPORTION) of the smaller visible dimension.
    // Scroll is always expressed in integer source pixels, so we might end up clamping to 1.
    const distanceUi = Math.min(bounds.width, bounds.height) * SCROLL_PROPORTION;
    const distanceImg = Math.max(1, Math.round(distanceUi / zoom));
    this.midx += distanceImg * dx;
    this.midy += distanceImg * dy;
    this.populateCameraUi();
    this.renderFullSoon();
  }
  
  zoomRelative(d, event) {
    const anchor = this.getAnchor(event);
    if (d > 0) {
      if ((this.zoom += ZOOM_RATE) > 1) this.zoom = 1;
    } else if (d < 0) {
      if ((this.zoom -= ZOOM_RATE) < -1) this.zoom = -1;
    }
    this.fitAnchor(anchor);
    this.populateCameraUi();
    this.renderFullSoon();
  }
  
  onFullWheel(event) {
    event.preventDefault();
    event.stopPropagation();
    let d = event.deltaX || event.deltaY;
    if (event.ctrlKey) {
      if (d > 0) this.zoomRelative(-1, event);
      else if (d < 0) this.zoomRelative(1, event);
    } else if (event.shiftKey) {
      if (d > 0) this.scrollFull(1, 0);
      else if (d < 0) this.scrollFull(-1, 0);
    } else {
      if (d > 0) this.scrollFull(0, 1);
      else if (d < 0) this.scrollFull(0, -1);
    }
    this.onFullMove(event); // To refresh the tattle.
  }
  
  onFullDown(event) {
    if (!this.hovertile) return;
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    face.frames.push({
      delay: 0,
      tiles: [{ tileid: this.hovertile }],
    });
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onFullUp(event) {
  }
  
  onFullMove(event) {
    const coords = this.imageCoordsFromEvent(event);
    const hovertile = this.getHoverTile(coords);
    if (hovertile === this.hovertile) return;
    this.hovertile = hovertile;
    this.tattle.innerText = this.tattleTextForHoverTile(this.hovertile, coords);
    this.renderFullSoon();
  }
  
  // null if oob, otherwise [x,y]
  imageCoordsFromEvent(event) {
    if (!this.sliceData || !this.res?.image) return null;
    const canvas = this.fullCanvas;
    const bounds = canvas.getBoundingClientRect();
    const uix = event.x - bounds.x;
    const uiy = event.y - bounds.y;
    const zoom = this.getFullZoom();
    const dstw = Math.round(this.res.image.naturalWidth * zoom);
    const dsth = Math.round(this.res.image.naturalHeight * zoom);
    const dstx = (bounds.width >> 1) - this.midx * zoom;
    const dsty = (bounds.height >> 1) - this.midy * zoom;
    const imgx = Math.round(((uix - dstx) * this.res.image.naturalWidth) / dstw);
    const imgy = Math.round(((uiy - dsty) * this.res.image.naturalHeight) / dsth);
    if ((imgx < 0) || (imgy < 0) || (imgx >= this.res.image.naturalWidth) || (imgy >= this.res.image.naturalHeight)) return null;
    return [imgx, imgy];
  }
  
  getHoverTile(coords) {
    if (!coords) return null;
    if (this.sliceData instanceof Tilesheet) {
      const tilesize = Math.max(1, this.res.image.naturalWidth >> 4);
      const col = Math.floor(coords[0] / tilesize);
      const row = Math.floor(coords[1] / tilesize);
      return (row << 4) | col;
    } else if (this.sliceData instanceof Decalsheet) {
      const decal = this.sliceData.decals.find(d => (
        ((coords[0] >= d.x) && (coords[1] >= d.y) && (coords[0] < d.x + d.w) && (coords[1] < d.y + d.h))
      ));
      if (decal && (typeof(decal.id) === "string")) {
        return decal.id;
      }
    }
    return null;
  }
  
  tattleTextForHoverTile(hovertile, coords) {
    if (hovertile === null) { // oob, no slice data, or between decals
      if (coords) return `${coords[0]},${coords[1]}`;
      return "";
    } else if (typeof(hovertile) === "number") { // tilesheet
      return "0x" + hovertile.toString(16).padStart(2, '0');
    } else if (typeof(hovertile) === "string") { // decalsheet
      return JSON.stringify(hovertile);
    }
    return "";
  }
  
  onFullLeave(event) {
    this.hovertile = null;
    this.tattle.innerText = "";
  }
  
  onDeleteFace(name) {
    if (!this.animation) return;
    const p = this.animation.faces?.find(f => f.name === name);
    if (p < 0) return;
    this.animation.faces.splice(p, 1);
    if (name === this.selectedFaceName) {
      this.selectedFaceName = "";
      this.populateAnimationDetails();
      this.reschedulePreview();
    }
    this.populateAnimationList();
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
  }
  
  onAddFace() {
    if (!this.animation) return;
    this.dom.modalText("Name for new face:", this.animation.uniqueFaceName()).then(rsp => {
      if (!rsp) return;
      this.selectedFaceName = this.sanitizeAnimationName(rsp);
      this.animation.faces.push({ name: rsp, delay: 200, frames: [] });
      this.populateAnimationList();
      this.populateAnimationDetails();
      this.reschedulePreview();
      this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    });
  }
  
  onSelectFace(name) {
    for (const element of this.element.querySelectorAll(".face.selected")) element.classList.remove("selected");
    const element = this.element.querySelector(`.face[data-name='${name}']`);
    if (!element) return;
    element.classList.add("selected");
    this.selectedFaceName = name;
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onFaceNameInput(event) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    const nname = this.sanitizeAnimationName(event.target.value);
    if (this.animation.faces.find(f => f.name === nname)) return; // Don't save duplicates.
    face.name = nname;
    this.selectedFaceName = nname;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationList();
  }
  
  sanitizeAnimationName(src) {
    if (!src || (typeof(src) !== "string")) return "";
    // It's important that we not allow apostrophes, because these get rolled into CSS strings.
    // As long as we're doing something, let's force a fairly restrictive whitelist.
    return src.replace(/[^a-zA-Z0-9_ .,-]+/g, "-");
  }
  
  onFaceDelayInput(event) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    face.delay = +event.target.value;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
  }
  
  onAddFrame() {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    let tileid;
    if (this.sliceData instanceof Tilesheet) tileid = 0;
    else if (this.sliceData instanceof Decalsheet) tileid = this.sliceData.decals[0]?.id || "";
    else return;
    face.frames.push({
      delay: 0,
      tiles: [{
        tileid,
        /* No need to store these if they take the default, which I expect to be very common.
        x: 0,
        y: 0,
        xform: 0,
        */
      }],
    });
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onDeleteFrame(p) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((p < 0) || (p >= face.frames.length)) return;
    face.frames.splice(p, 1);
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onMoveFrame(p, d) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((p < 0) || (p >= face.frames.length)) return;
    const frame = face.frames[p];
    const np = p + d;
    if ((np < 0) || (np >= face.frames.length)) return;
    face.frames.splice(p, 1);
    face.frames.splice(np, 0, frame);
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onDelayInput(event, framep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    const delay = +event.target.value || 0;
    if (delay === (frame.delay || 0)) return;
    if (delay) frame.delay = delay;
    else delete frame.delay;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.reschedulePreview();
  }
  
  onAddTile(framep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    let tileid;
    if (this.sliceData instanceof Tilesheet) tileid = 0;
    else if (this.sliceData instanceof Decalsheet) tileid = this.sliceData.decals[0]?.id || "";
    else return;
    frame.tiles.push({ tileid });
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onDeleteTile(framep, tilep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    frame.tiles.splice(tilep, 1);
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onMoveTile(framep, tilep, d) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    const np = tilep + d;
    if ((np < 0) || (np >= frame.tiles.length)) return;
    const tile = frame.tiles[p];
    frame.tiles.splice(tilep, 1);
    frame.tiles.splice(np, 0, tile);
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.populateAnimationDetails();
    this.reschedulePreview();
  }
  
  onTileidInput(event, framep, tilep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    const tile = frame.tiles[tilep];
    if (this.sliceData instanceof Tilesheet) {
      tile.tileid = +event.target.value || 0;
    } else if (this.sliceData instanceof Decalsheet) {
      tile.tileid = event.target.value || "";
    } else {
      return;
    }
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.reschedulePreview();
  }
  
  onXInput(event, framep, tilep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    const tile = frame.tiles[tilep];
    const v = +event.target.value;
    if (v) tile.x = v;
    else delete tile.x;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.reschedulePreview();
  }
  
  onYInput(event, framep, tilep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    const tile = frame.tiles[tilep];
    const v = +event.target.value;
    if (v) tile.y = v;
    else delete tile.y;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.reschedulePreview();
  }
  
  onXformInput(event, framep, tilep) {
    const face = this.animation?.faces?.find(f => f.name === this.selectedFaceName);
    if (!face) return;
    if ((framep < 0) || (framep >= face.frames.length)) return;
    const frame = face.frames[framep];
    if ((tilep < 0) || (tilep >= frame.tiles.length)) return;
    const tile = frame.tiles[tilep];
    const v = +event.target.value;
    if (v) tile.xform = v;
    else delete tile.xform;
    this.imageAnimationService.saveAnimation(this.res.path, this.animation);
    this.reschedulePreview();
  }
}
