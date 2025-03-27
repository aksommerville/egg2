/* ImageEditor.js
 * Nevermind the name, we do not and will not actually edit images. Plenty of other options for that.
 * TODO Allow scale and scroll, maybe some tile previewer, or a robust animation preview like we had in v1.
 */
 
import { Dom } from "./Dom.js";
import { Data } from "./Data.js";

export class ImageEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data];
  }
  constructor(element, dom, data) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    
    this.res = null;
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
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "IMG", { src: res.path });
  }
}
