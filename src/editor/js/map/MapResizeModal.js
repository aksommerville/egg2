/* MapResizeModal.js
 */
 
import { Dom } from "../Dom.js";

export class MapResizeModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.map = null;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(map) {
    this.map = map;
    this.element.innerHTML  = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    const table = this.dom.spawn(form, "TABLE");
    let winput;
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Width"),
      this.dom.spawn(null, "TD",
        winput = this.dom.spawn(null, "INPUT", { name: "w", type: "number", min: 1, max: 255, value: map.w })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Height"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "h", type: "number", min: 1, max: 255, value: map.h })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", "Anchor"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "SELECT", { name: "anchor" },
          this.dom.spawn(null, "OPTION", { value: "nw" }, "NW"),
          this.dom.spawn(null, "OPTION", { value: "n" }, "N"),
          this.dom.spawn(null, "OPTION", { value: "ne" }, "NE"),
          this.dom.spawn(null, "OPTION", { value: "w" }, "W"),
          this.dom.spawn(null, "OPTION", { value: "c" }, "Center"),
          this.dom.spawn(null, "OPTION", { value: "e" }, "E"),
          this.dom.spawn(null, "OPTION", { value: "sw" }, "SW"),
          this.dom.spawn(null, "OPTION", { value: "s" }, "S"),
          this.dom.spawn(null, "OPTION", { value: "se" }, "SE"),
        )
      )
    );
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": () => this.onSubmit() });
    
    this.window.requestAnimationFrame(() => {
      winput.focus();
      winput.select();
    });
  }
  
  onSubmit() {
    const w = +this.element.querySelector("input[name='w']").value;
    const h = +this.element.querySelector("input[name='h']").value;
    const anchor = this.element.querySelector("select[name='anchor']").value;
    this.resolve({ w, h, anchor });
    this.element.remove();
  }
}
