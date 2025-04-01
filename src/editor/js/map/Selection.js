/* Selection.js
 * Floating selected layers for use by MapPaint.
 * A selection describes an arbitrary set of cells, and values for those cells.
 */
 
export class Selection {
  constructor(x, y) {
  
    // My bounds in the map's space.
    // Everything else is relative to this (x,y).
    this.x = x;
    this.y = y;
    this.w = 1;
    this.h = 1;
    
    // null or Uint8Array(w*h). If present, nonzero cells are part of the selection.
    // If null, we're a full rectangle.
    this.mask = null;
    
    // null or Uint8Array(w*h). If present, we are a floating layer in addition to a selection.
    this.cellv = null;
    
    // If we're being used as a temporary, this was the starting point.
    this.anchorx = x;
    this.anchory = y;
  }
  
  contains(x, y) {
    if (x < this.x) return false;
    if (y < this.y) return false;
    if (x >= this.x + this.w) return false;
    if (y >= this.y + this.h) return false;
    if (!this.mask) return true; // We're a rectangle.
    return !!this.mask[(y - this.y) * this.w + (x - this.x)];
  }
  
  /* Replace content such that we become the smallest rectangle containing both (x,y) and our anchor.
   * (cellv) is quietly dropped if present.
   */
  setRectangle(x, y) {
    if (this.mask) throw new Error(`setRectangle on a non-rectangular Selection`);
    this.cellv = null;
    if (x < this.anchorx) {
      this.x = x;
      this.w = this.anchorx - x + 1;
    } else {
      this.x = this.anchorx;
      this.w = x - this.anchorx + 1;
    }
    if (y < this.anchory) {
      this.y = y;
      this.h = this.anchory - y + 1;
    } else {
      this.y = this.anchory;
      this.h = y - this.anchory + 1;
    }
  }
  
  /* Replace whatever needs replaced to include just this one more point in the selected range.
   * (cellv) is quietly dropped if present.
   * Returns true if changed.
   */
  addPoint(x, y) {
    this.cellv = null;
    
    // If we're a rectangle -- all Selections are initially -- try to remain one.
    // That can only happen when our width or height is one.
    if (!this.mask) {
      if ((x >= this.x) && (y >= this.y) && (x < this.x + this.w) && (y < this.y + this.h)) return false;
      if ((x === this.x) && (this.w === 1)) {
        if (y === this.y - 1) {
          this.y--;
          this.h++;
          return true;
        }
        if (y === this.y + this.h) {
          this.h++;
          return true;
        }
      }
      if ((y === this.y) && (this.h === 1)) {
        if (x === this.x - 1) {
          this.x--;
          this.w++;
          return true;
        }
        if (x === this.x + this.w) {
          this.w++;
          return true;
        }
      }
    
    // And in non-rectangle cases, get out if the point is already present.
    } else {
      const p = (y - this.y) * this.w + (x - this.x);
      if (this.mask[p]) return false;
    }
    
    this.forceMask(x, y);
    
    // Finally, add the point to my mask.
    const p = (y - this.y) * this.w + (x - this.x);
    this.mask[p] = 1;
    return true;
  }
  
  /* Create (mask) if it doesn't exist yet.
   * (x,y,w,h) are optional. If supplied, we expand to include that point or rect.
   * We DO NOT add the provided point or rect to the selection, we only ensure there's room for it.
   */
  forceMask(x, y, w, h) {
  
    // Determine what the new bounds should be.
    let nx=this.x, ny=this.y, nw=this.w, nh=this.h;
    if ((typeof(x) === "number") && (typeof(y) === "number")) {
      if (x < nx) { nw += nx - x; nx = x; }
      else if (x >= nx + nw) nw = x - nx + 1;
      if (y < ny) { nh += ny - y; ny = y; }
      else if (y >= ny + nh) nh = y - ny + 1;
      if ((typeof(w) === "number") && (typeof(h) === "number")) {
        x = x + w - 1;
        y = y + h - 1;
        if (x < nx) { nw += nx - x; nx = x; }
        else if (x >= nx + nw) nw = x - nx + 1;
        if (y < ny) { nh += ny - y; ny = y; }
        else if (y >= ny + nh) nh = y - ny + 1;
      }
    }
    
    // If bounds changed, expand a little. Prevent excessive reallocation when lassoing.
    let grow = false;
    if ((nx !== this.x) || (ny !== this.y) || (nw !== this.w) || (nh !== this.h)) {
      const pad = 4;
      nx -= pad;
      ny -= pad;
      nw += pad * 2;
      nh += pad * 2;
      grow = true;
    }
    
    // If we don't have a mask yet, allocate it and fill in the rectangle for the prior bounds.
    if (!this.mask) {
      this.mask = new Uint8Array(nw * nh);
      this.setCellsRect(this.mask, nw, this.x - nx, this.y - ny, this.w, this.h, 1);
      
    // If we already have a mask and don't need to grow, we're done.
    } else if (!grow) {
      return;
      
    // If we already have a mask, allocate the new one and copy prior mask into it.
    } else {
      const nmask = new Uint8Array(nw * nh);
      this.copyCells(nmask, nw, nh, this.mask, this.w, this.h, this.x - nx, this.y - ny);
      this.mask = nmask;
    }
    
    // If we have cells, resize them too.
    if (this.cellv && grow) {
      const nv = new Uint8Array(nw * nh);
      this.copyCells(nv, nw, nh, this.cellv, this.w, this.h, this.x - nx, this.y - ny);
      this.cellv = nv;
    }
    
    // Update bounds.
    this.x = nx;
    this.y = ny;
    this.w = nw;
    this.h = nh;
  }
  
  // Copy all of (src) into (dst) -- Caller guarantees it fits.
  copyCells(dst, dstw, dsth, src, srcw, srch, dstx, dsty) {
    let dstrowp = dsty * dstw + dstx;
    let srcrowp = 0;
    for (let yi=srch; yi-->0; dstrowp+=dstw, srcrowp+=srcw) {
      for (let xi=srcw, dstp=dstrowp, srcp=srcrowp; xi-->0; dstp++, srcp++) {
        dst[dstp] = src[srcp];
      }
    }
  }
  
  copyTrueCells(dst, dstw, dsth, src, srcw, srch, dstx, dsty) {
    let dstrowp = dsty * dstw + dstx;
    let srcrowp = 0;
    for (let yi=srch; yi-->0; dstrowp+=dstw, srcrowp+=srcw) {
      for (let xi=srcw, dstp=dstrowp, srcp=srcrowp; xi-->0; dstp++, srcp++) {
        if (src[srcp]) dst[dstp] = src[srcp];
      }
    }
  }
  
  setCellsRect(dst, dstw, x, y, w, h, v) {
    let dstrowp = y * dstw + x;
    for (let yi=h; yi-->0; dstrowp+=dstw) {
      for (let xi=w, dstp=dstrowp; xi-->0; dstp++) {
        dst[dstp] = v;
      }
    }
  }
  
  /* Retain my shape exactly but move by some relative cell count.
   */
  move(dx, dy) {
    this.x += dx;
    this.y += dy;
  }
  
  /* If I have cells, copy them down onto (map).
   */
  anchor(map) {
    if (!this.cellv) return;
    let srcx=0, srcy=0, dstx=this.x, dsty=this.y, w=this.w, h=this.h;
    if (dstx < 0) { srcx -= dstx; w += dstx; dstx = 0; }
    if (dsty < 0) { srcy -= dsty; h += dsty; dsty = 0; }
    if (dstx + w > map.w) w = map.w - dstx;
    if (dsty + h > map.h) h = map.h - dsty;
    if ((w < 1) || (h < 1)) return;
    let srcrowp = srcy * this.w + srcx;
    let dstrowp = dsty * map.w + dstx;
    for (let yi=h; yi-->0; srcrowp+=this.w, dstrowp+=map.w) {
      for (let xi=w, srcp=srcrowp, dstp=dstrowp; xi-->0; srcp++, dstp++) {
        if (this.mask && !this.mask[srcp]) continue;
        map.v[dstp] = this.cellv[srcp];
      }
    }
  }
  
  /* Create (this.cellv) and populate it with cells from (map) where we cover.
   * All affected cells, we zero them in (map).
   */
  float(map) {
    this.cellv = new Uint8Array(this.w * this.h);
    // NB copied from anchor(). So "dst" means "map", and "src" means "this".
    let srcx=0, srcy=0, dstx=this.x, dsty=this.y, w=this.w, h=this.h;
    if (dstx < 0) { srcx -= dstx; w += dstx; dstx = 0; }
    if (dsty < 0) { srcy -= dsty; h += dsty; dsty = 0; }
    if (dstx + w > map.w) w = map.w - dstx;
    if (dsty + h > map.h) h = map.h - dsty;
    if ((w < 1) || (h < 1)) return;
    let srcrowp = srcy * this.w + srcx;
    let dstrowp = dsty * map.w + dstx;
    for (let yi=h; yi-->0; srcrowp+=this.w, dstrowp+=map.w) {
      for (let xi=w, srcp=srcrowp, dstp=dstrowp; xi-->0; srcp++, dstp++) {
        if (this.mask && !this.mask[srcp]) continue;
        this.cellv[srcp] = map.v[dstp];
        map.v[dstp] = 0;
      }
    }
  }
  
  /* Add (other)'s range to mine, and if (map), float all the added cells.
   * (other) should not contain cell data.
   */
  combine(other, map) {
  
    // Put both selections in mask mode, growing (this) to cover (other).
    this.forceMask(other.x, other.y, other.w, other.h);
    other.forceMask();
    
    // For any cell set in (other) but not (this), set in (this) and float from (map) if present.
    let dstrowp = (other.y - this.y) * this.w + (other.x - this.x);
    let srcrowp = 0;
    const cellstride = map?.w || 0;
    const celllimit = map ? (map.w * map.h) : 0;
    let cellrowp = other.y * cellstride + other.x;
    for (let yi=other.h; yi-->0; dstrowp+=this.w, srcrowp+=other.w, cellrowp+=cellstride) {
      for (let xi=other.w, dstp=dstrowp, srcp=srcrowp, cellp=cellrowp; xi-->0; dstp++, srcp++, cellp++) {
        if (this.mask[dstp]) continue;
        if (!other.mask[srcp]) continue;
        this.mask[dstp] = 1;
        if (this.cellv && map && (cellp >= 0) && (cellp < celllimit)) {
          this.cellv[dstp] = map.v[cellp];
          map.v[cellp] = 0;
        }
      }
    }
  }
  
  /* Remove (other)'s range from mine, and if (map), anchor all the removed cells.
   * (other) should not contain cell data.
   */
  remove(other, map) {
    
    // Determine the range of overlap. If none, great, we're done.
    const xlo = Math.max(this.x, other.x);
    const ylo = Math.max(this.y, other.y);
    const xhi = Math.min(this.x + this.w, other.x + other.w);
    const yhi = Math.min(this.y + this.h, other.y + other.h);
    const w = xhi - xlo;
    const h = yhi - ylo;
    if ((w < 1) || (h < 1)) return;
    
    // Put (this) in mask mode, but keep (other) in rect mode if it's there.
    this.forceMask();
    
    // Iterate the overlap range. If it's present in both, remove from (this), and anchor if applicable.
    // To go easy on myself, I'm doing this with map coordinates throughout. Would of course be more efficient with dedicated iterators.
    for (let y=ylo; y<yhi; y++) {
      for (let x=xlo; x<xhi; x++) {
        if (!other.contains(x, y)) continue;
        if (!this.contains(x, y)) continue;
        const p = (y - this.y) * this.w + (x - this.x);
        this.mask[p] = 0;
        if (map && this.cellv && (x >= 0) && (y >= 0) && (x < map.w) && (y < map.h)) {
          const mapp = y * map.w + x;
          map.v[mapp] = this.cellv[p];
          this.cellv[p] = 0;
        }
      }
    }
  }
  
  countCells() {
    if (!this.mask) return this.w * this.h;
    let c = 0;
    for (let i=this.w*this.h; i-->0; ) {
      if (this.mask[i]) c++;
    }
    return c;
  }
}
