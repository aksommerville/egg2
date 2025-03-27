/* modals.js
 * Minor modals used by the generic Dom.
 */
 
import { Dom } from "./Dom.js";

/* ModalPickOne: Bunch of buttons, and we dismiss when one gets clicked.
 *********************************************************************************************/

export class ModalPickOne {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.options = [];
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(options) {
    this.options = options;
    this.element.innerHTML = "";
    for (const option of options) {
      this.dom.spawn(this.element, "INPUT", { type: "button", value: option, "on-click": () => this.onClick(option) });
    }
  }
  
  onClick(option) {
    this.resolve(option);
    this.element.close();
  }
}

/* ModalText: Question and answer, short loose text.
 ***********************************************************************************************/

export class ModalText {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    this.dom.spawn(form, "DIV", ["prompt"]);
    this.dom.spawn(form, "INPUT", ["response"], { type: "text" });
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": event => {
      this.resolve(this.element.querySelector(".response").value);
      this.element.close();
      event.preventDefault();
      event.stopPropagation();
    }});
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(prompt, preset) {
    this.element.querySelector(".prompt").innerText = prompt;
    const response = this.element.querySelector(".response");
    response.value = preset || "";
    response.select();
  }
}

/* ModalError: Display a message. Accepts anything at all. String and Error are preferred.
 **************************************************************************************************/
 
export class ModalError {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
  }
  
  setup(error) {
    if (!error) error = "Unspecified error.";
    if (typeof(error) === "string") ;
    else if (error instanceof Error) error = error.stack || error.message || "Unspecified error.";
    else error = JSON.stringify(error);
    this.element.innerHTML = "";
    this.element.innerText = error;
  }
}
