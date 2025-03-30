/* SpriteEditor.js
 * Just a CommandListEditor with a few custom fields.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { CommandList } from "./CommandList.js";
import { CommandListEditor } from "./CommandListEditor.js";
import { PickImageModal } from "../PickImageModal.js";
import { PickTileModal } from "../PickTileModal.js";

export class SpriteEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.clctl = null; // CommandListEditor
    this.displayedImage = "";
    this.displayedTile = "";
    this.renderTimeout = null;
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  static checkResource(res) {
    if (res.type === "sprite") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    this.model = new CommandList(res.serial);
    this.buildUi();
  }
  
  /* UI.
   *********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["row", "topRow"]);
    this.dom.spawn(topRow, "CANVAS", ["preview"], { "on-click": () => this.onPickTile(+this.displayedTile.split(/\s+/)[0]) });
    this.dom.spawn(topRow, "INPUT", ["image"], { type: "button", value: this.reprImage(), "on-click": () => this.onPickImage(this.displayedImage) });
    
    this.clctl = this.dom.spawnController(this.element, CommandListEditor);
    this.clctl.ondirty = () => this.onCommandsDirty();
    this.clctl.onhelp = (cmd, p) => this.onCommandHelp(cmd, p);
    this.clctl.setup(this.model, "sprite");
    
    this.displayedImage = "";
    this.displayedTile = "";
    this.populateUpperUi();
  }
  
  reprImage() {
    return this.model?.getFirstArg("image") || "";
  }
  
  populateUpperUi() {
    if (!this.model) return;
    const nextImage = this.model.getFirstArg("image");
    const nextTile = this.model.getFirstArg("tile");
    if ((nextImage !== this.displayedImage) || (nextTile !== this.displayedTile)) {
      this.displayedImage = nextImage;
      this.displayedTile = nextTile;
      this.element.querySelector("input.image").value = this.displayedImage;
      this.renderSoon();
    }
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector("canvas.preview");
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    this.data.getImageAsync(this.displayedImage).then(image => {
      const tokens = this.displayedTile.split(/\s+/g);
      const tileid = +tokens[0] || 0;
      const xform = +tokens[1] || 0;
      const tilesize = image.naturalWidth >> 4;
      canvas.width = tilesize;
      canvas.height = tilesize;
      const srcx = (tileid & 15) * tilesize
      const srcy = (tileid >> 4) * tilesize;
      const halftile = tilesize >> 1;
      ctx.save();
      ctx.translate(halftile, halftile);
      switch (xform) {
        case 1: ctx.scale(-1, 1); break; // XREV
        case 2: ctx.scale(1, -1); break; // YREV
        case 3: ctx.scale(-1, -1); break; // XREV|YREV
        case 4: ctx.rotate(Math.PI / 2); ctx.scale(1, -1); break; // SWAP
        case 5: ctx.rotate(Math.PI / -2); break; // SWAP|XREV
        case 6: ctx.rotate(Math.PI / 2); break; // SWAP|YREV
        case 7: ctx.rotate(Math.PI / 2); ctx.scale(-1, 1); break; // SWAP|XREV|YREV
      }
      ctx.drawImage(image, srcx, srcy, tilesize, tilesize, -halftile, -halftile, tilesize, tilesize);
      ctx.restore();
    }).catch(() => {});
  }
  
  /* Events.
   *****************************************************************************/
   
  dirty() {
    this.data.dirty(this.res.path, () => this.model.encode());
  }
  
  onCommandsDirty() {
    this.populateUpperUi();
    this.dirty();
  }
  
  onCommandHelp(cmd, p) {
    switch (cmd[0]) {
      case "image": this.onPickImage(cmd[1]); return true;
      case "tile": this.onPickTile(+cmd[1]); return true;
    }
    return false;
  }
  
  onPickImage(id) {
    if (!this.model) return;
    const modal = this.dom.spawnModal(PickImageModal);
    modal.setup(this.data.findResource(id, "image"));
    modal.result.then(res => {
      if (!res) return;
      this.setImage(`image:${res.name || res.rid}`);
    });
  }
  
  onPickTile(tileid) {
    if (!this.model) return;
    this.data.getImageAsync(this.displayedImage)
      .catch(() => null)
      .then(image => {
        const modal = this.dom.spawnModal(PickTileModal);
        modal.setup(image, tileid);
        return modal.result;
      }).then(tileid => {
        if (typeof(tileid) === "number") this.setTile(tileid);
      });
  }
  
  setImage(src) {
    if (!this.model) return;
    let ok = false;
    for (const command of this.model.commands) {
      if (command[0] !== "image") continue;
      command[1] = src;
      ok = true;
      break;
    }
    if (!ok) this.model.commands.push(["image", src]);
    this.clctl.setup(this.model, "sprite");
    this.populateUpperUi();
    this.dirty();
  }
  
  setTile(tileid) {
    if (!this.model) return;
    let ok = false;
    for (const command of this.model.commands) {
      if (command[0] !== "tile") continue;
      command[1] = tileid.toString();
      ok = true;
      break;
    }
    if (!ok) this.model.commands.push(["tile", tileid.toString(), "0"]);
    this.clctl.setup(this.model, "sprite");
    this.populateUpperUi();
    this.dirty();
  }
}
