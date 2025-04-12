/* ModecfgDrumModal.js
 * Edit a channel's payload as a straight hex dump.
 */
 
import { Dom } from "../Dom.js";

export class ModecfgDrumModal {
  static getDependencies() {
    return [HTMLElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    
    this.channel = null;
    this.song = null;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* Build UI.
   *************************************************************************/
  
  setup(channel, song) {
    this.channel = channel;
    this.song = song;
    
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", { "on-submit": event => {
      event.preventDefault();
      event.stopPropagation();
    }});
    
    //TODO build ui
    
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  /* Read from UI.
   ************************************************************************/
   
  readPayloadFromUi() {
    //TODO
    return new Uint8Array(this.channel.payload);
  }
  
  /* Events.
   ***************************************************************************/
   
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    const payload = this.readPayloadFromUi();
    this.resolve(payload);
    this.element.remove();
  }
}
