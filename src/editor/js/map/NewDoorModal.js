/* NewDoorModal.js
 */
 
import { Dom } from "../Dom.js";

export class NewDoorModal {
  static getDependencies() {
    return [HTMLElement, Dom, Window];
  }
  constructor(element, dom, window) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(map, x, y) {
    console.log(`NewDoorModal.setup`, { map, x, y });
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", `New door from map:${map.rid} at ${x},${y}...`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    
    const table = this.dom.spawn(form, "TABLE");
    let dstmapid;
    let tr;
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "To map");
      this.dom.spawn(tr, "TD",
        dstmapid = this.dom.spawn(null, "INPUT", { type: "text", name: "dstmapid" })
      );
    }
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "To cell");
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name: "dstx", min: 0, max: 255, value: x }),
        this.dom.spawn(null, "INPUT", { type: "number", name: "dsty", min: 0, max: 255, value: y })
      );
    }
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Round trip?");
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "checkbox", name: "roundtrip", checked: "checked" })
      );
    }
    
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": () => this.onSubmit() });
    
    this.window.requestAnimationFrame(() => {
      dstmapid.focus();
    });
  }
  
  onSubmit() {
    const dstmapid = this.element.querySelector("input[name='dstmapid']").value;
    const dstx = +this.element.querySelector("input[name='dstx']").value;
    const dsty = +this.element.querySelector("input[name='dsty']").value;
    const roundtrip = this.element.querySelector("input[name='roundtrip']").checked;
    this.resolve({ dstmapid, dstx, dsty, roundtrip });
    this.element.remove();
  }
}
