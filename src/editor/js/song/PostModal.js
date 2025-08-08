/* PostModal.js
 * Edit an entire post pipe.
 */
 
import { Dom } from "../Dom.js";
import { Encoder } from "../Encoder.js";

export class PostModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.post = null; // Post, see below.
    this.serial = null;
    this.raw = false;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(serial, raw) {
    this.serial = serial;
    this.post = new Post(serial);
    this.raw = raw;
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": event => {
        event.preventDefault();
        event.stopPropagation();
      },
    });
    if (this.raw) {
      const textarea = this.dom.spawn(form, "TEXTAREA", { name: "raw", "on-input": e => this.onRawChange(e) });
      textarea.value = this.hexdump(this.serial);
    } else {
      for (const stage of this.post.stages) {
        this.spawnStage(form, stage);
      }
      this.dom.spawn(form, "INPUT", { type: "button", value: "+", "on-click": e => this.onAddStage(e) });
    }
    this.dom.spawn(form, "DIV", ["spacer"]);
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  spawnStage(parent, stage) {
    const row = this.dom.spawn(parent, "DIV", ["stage"]);
    this.dom.spawn(row, "INPUT", { type: "button", value: "X", "on-click": e => this.onDeleteStage(stage.id) });
    this.dom.spawn(row, "INPUT", { type: "button", value: "^", "on-click": e => this.onMoveStage(stage.id, -1) });
    this.dom.spawn(row, "INPUT", { type: "button" , value: "v", "on-click": e => this.onMoveStage(stage.id, 1) });
    
    const stageidSelect = this.dom.spawn(row, "SELECT", { name: "stageid", "on-input": e => this.onStageIdChange(stage.id, +e.target.value) });
    this.dom.spawn(stageidSelect, "OPTION", { value: 0 }, "NOOP");
    this.dom.spawn(stageidSelect, "OPTION", { value: 1 }, "DELAY");
    this.dom.spawn(stageidSelect, "OPTION", { value: 2 }, "WAVESHAPER");
    this.dom.spawn(stageidSelect, "OPTION", { value: 3 }, "TREMOLO");
    for (let i=4; i<256; i++) {
      this.dom.spawn(stageidSelect, "OPTION", { value: i }, i);
    }
    
    //TODO friendlier per-stage-type ui
    this.dom.spawn(row, "INPUT", { type: "text", name: "serial", value: this.hexdump(stage.serial), "on-input": e => this.onStageInput(stage.id, e.target.value) });
  }
  
  hexdump(src) {
    if (!src) return "";
    let dst = "";
    for (let i=0; i<src.length; i++) {
      dst += "0123456789abcdef"[src[i] >> 4];
      dst += "0123456789abcdef"[src[i] & 15];
      dst += " ";
    }
    return dst;
  }
  
  unhexdump(src) {
    src = src.replace(/[^0-9a-fA-F]+/g, "");
    const dst = new Uint8Array(src.length >> 1);
    for (let i=0; i<src.length; i+=2) {
      dst[i >> 1] = parseInt(src.substring(i, i + 2), 16);
    }
    return dst;
  }
  
  onRawChange(event) {
    // Don't bother evaluating it until they submit.
  }
  
  onAddStage(event) {
    this.post.addStage();
    this.buildUi();
  }
  
  onDeleteStage(id) {
    const p = this.post.stages.findIndex(s => s.id === id);
    if (p < 0) return;
    this.post.stages.splice(p, 1);
    this.buildUi();
  }
  
  onMoveStage(id, d) {
    if (this.post.stages.length < 2) return;
    const p = this.post.stages.findIndex(s => s.id === id);
    if (p < 0) return;
    const stage = this.post.stages[p];
    this.post.stages.splice(p, 1);
    const np = Math.max(0, Math.min(this.post.stages.length, p + d));
    this.post.stages.splice(np, 0, stage);
    this.buildUi();
  }
  
  onStageIdChange(id, stageid) {
    const stage = this.post.stages.find(s => s.id === id);
    if (!stage) return;
    stage.stageid = stageid;
  }
  
  onStageInput(id, value) {
    const stage = this.post.stages.find(s => s.id === id);
    if (!stage) return;
    stage.serial = this.unhexdump(value);
  }
  
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    if (this.raw) {
      this.resolve(this.unhexdump(this.element.querySelector("textarea[name='raw']").value));
    } else {
      this.resolve(this.post.encode());
    }
    this.element.remove();
  }
}

export class Post {
  constructor(src) {
    this._init();
    console.log('Post input', src);
    if (!src) ;
    else if (src instanceof Uint8Array) this._decode(src);
    else throw new Error(`Unexpected input to Post`);
  }
  
  _init() {
    if (!Post.nextStageId) Post.nextStageId = 1;
    this.stages = []; // {id,stageid,serial}
  }
  
  _decode(src) {
    for (let srcp=0; srcp<src.length;) {
      const stageid = src[srcp++];
      const len = src[srcp++] || 0;
      if (srcp > src.length - len) break;
      const serial = new Uint8Array(src.buffer, src.byteOffset + srcp, len);
      this.stages.push({ id: Post.nextStageId++, stageid, serial });
      srcp += len;
    }
  }
  
  addStage() {
    this.stages.push({ id: Post.nextStageId++, stageid: 0, serial: new Uint8Array([]) });
  }
  
  encode() {
    const dst = new Encoder();
    for (const stage of this.stages) {
      dst.u8(stage.stageid);
      dst.pfxlen(1, () => dst.raw(stage.serial));
    }
    return dst.finish();
  }
}
