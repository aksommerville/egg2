/* NewMapModal.js
 */
 
import { Dom } from "../Dom.js";
import { MapService } from "./MapService.js";

export class NewMapModal {
  static getDependencies() {
    return [HTMLElement, Dom, MapService, Window];
  }
  constructor(element, dom, mapService, window) {
    this.element = element;
    this.dom = dom;
    this.mapService = mapService;
    this.window = window;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(map, dx, dy) {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "DIV", `New map at ${dx},${dy} from map:${map.rid}...`);
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": e => {
      e.preventDefault();
      e.stopPropagation();
    }});
    const table = this.dom.spawn(form, "TABLE");
    let tr, nameinput;
    
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "Name");
      this.dom.spawn(tr, "TD",
        nameinput = this.dom.spawn(null, "INPUT", { type: "text", name: "name" })
      );
    }
    if (tr = this.dom.spawn(table, "TR")) {
      this.dom.spawn(tr, "TD", "ID");
      this.dom.spawn(tr, "TD",
        this.dom.spawn(null, "INPUT", { type: "number", name: "rid", min: 1, max: 65535, value: this.mapService.data.unusedId("map") })
      );
    }
    // Don't bother asking for dimensions -- when neighbors in play, all maps are supposed to be the same size.
    
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": () => this.onSubmit() });
    
    this.window.requestAnimationFrame(() => {
      nameinput.focus();
      nameinput.select();
    });
  }
  
  onSubmit() {
    const name = this.element.querySelector("input[name='name']").value;
    const rid = +this.element.querySelector("input[name='rid']").value;
    this.resolve({ name, rid });
  }
}
