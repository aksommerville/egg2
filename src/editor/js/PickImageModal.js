/* PickImageModal.js
 * Displays every image resource so user can pick one visually.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

const PAGE_SIZE = 5;

export class PickImageModal {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
    this.pagep = 0; // Index in (this.images) of first displayed image. Always a multiple of PAGE_SIZE.
    this.images = this.data.resv.filter(r => r.type === "image").sort((a, b) => {
      if (a.rid < b.rid) return -1;
      if (a.rid > b.rid) return 1;
      if (a.name < b.name) return -1;
      if (a.name > b.name) return 1;
      if (a.path < b.path) return -1;
      if (a.path > b.path) return 1;
      return 0;
    });
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(res) {
    this.res = res;
    this.element.innerHTML = "";
    this.pagep = 0;
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "<", "on-click": () => this.onNextPage(-1) }),
      this.dom.spawn(null, "DIV", ["pageId"]),
      this.dom.spawn(null, "INPUT", { type: "button", value: ">", "on-click": () => this.onNextPage(1) })
    );
    for (let i=0; i<PAGE_SIZE; i++) {
      this.dom.spawn(this.element, "BUTTON", ["image"], { "data-index": i, "on-click": () => this.onPickImage(this.pagep + i) });
    }
    this.populateUi();
  }
  
  populateUi() {
    const pagec = Math.ceil(this.images.length / PAGE_SIZE);
    this.element.querySelector(".pageId").innerText = `Page ${this.pagep / PAGE_SIZE + 1} of ${pagec}`;
    for (const button of this.element.querySelectorAll("button.image")) {
      const bp = +button.getAttribute("data-index");
      const imagep = this.pagep + bp;
      const image = this.images[imagep];
      button.innerHTML = "";
      button.classList.remove("prior");
      if (image) {
        button.disabled = false;
        const thumbnail = this.dom.spawn(button, "CANVAS", ["thumbnail"]);
        this.renderThumbnail(thumbnail, image);
        let dname = image.path || image.name || "";
        if (dname.startsWith("/data/image/")) dname = dname.substring(12);
        this.dom.spawn(button, "DIV", dname);
        if (this.res && (image.path === this.res.path)) button.classList.add("prior");
      } else {
        button.disabled = true;
      }
    }
  }
  
  renderThumbnail(dst, res) {
    if (!res.image) {
      this.data.getImageAsync(res.path).then(image => {
        this.renderThumbnail(dst, { ...res, image });
      }).catch(() => {});
      return;
    }
    const bounds = dst.getBoundingClientRect();
    dst.width = bounds.width;
    dst.height = bounds.height;
    const ctx = dst.getContext("2d");
    ctx.clearRect(0, 0, bounds.width, bounds.height);
    let dstx, dsty, dstw, dsth;
    if ((dstw = (bounds.height * res.image.naturalWidth) / res.image.naturalHeight) <= bounds.width) {
      dstx = (bounds.width >> 1) - (dstw >> 1);
      dsty = 0;
      dsth = bounds.height;
    } else {
      dstx = 0;
      dstw = bounds.width;
      dsth = (bounds.width * res.image.naturalHeight) / res.image.naturalWidth;
      dsty = (bounds.height >> 1) - (dsth >> 1);
    }
    ctx.drawImage(res.image, 0, 0, res.image.naturalWidth, res.image.naturalHeight, dstx, dsty, dstw, dsth);
  }
  
  onNextPage(d) {
    const np = this.pagep + d * PAGE_SIZE;
    if ((np < 0) || (np >= this.images.length)) return;
    this.pagep = np;
    this.populateUi();
  }
  
  onPickImage(p) {
    const res = this.images[p];
    if (!res) return;
    this.resolve(res);
    this.element.remove();
  }
}
