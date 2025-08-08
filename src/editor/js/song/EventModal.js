/* EventModal.js
 * For adding or editing an event.
 */
 
import { Dom } from "../Dom.js";
import { SongEvent } from "./Song.js";

export class EventModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* Required.
   * (event) may be null for "new event", otherwise it's a SongEvent.
   */
  setup(event) {
    if (event) this.event = event;
    else this.event = SongEvent.newFullyPopulated();
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const form = this.dom.spawn(this.element, "FORM", {
      "on-submit": e => {
        e.preventDefault();
        e.stopPropagation();
      },
    });
    const table = this.dom.spawn(form, "TABLE");
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Time"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "time", type: "number", value: this.event.time })
      )
    );
    let typeMenu;
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Type"),
      this.dom.spawn(null, "TD",
        typeMenu = this.dom.spawn(null, "SELECT", { name: "type", "on-change": e => this.enableAndDisableFields() })
      )
    );
    this.dom.spawn(typeMenu, "OPTION", { value: "noop" }, "noop");
    this.dom.spawn(typeMenu, "OPTION", { value: "note" }, "note");
    this.dom.spawn(typeMenu, "OPTION", { value: "wheel" }, "wheel");
    this.dom.spawn(typeMenu, "OPTION", { value: "loop" }, "loop");
    typeMenu.value = this.event.type;
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Channel"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "chid", type: "number", min: -1, max: 15, value: this.event.chid })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Note"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "noteid", type: "number", min: 0, max: 127, value: this.event.noteid || 0 })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Velocity"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "velocity", type: "number", min: 0, max: 127, value: this.event.velocity || 0 })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Dur (ms)"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "durms", type: "number", value: this.event.durms || 0 })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["k"], "Wheel 0..1023"),
      this.dom.spawn(null, "TD",
        this.dom.spawn(null, "INPUT", { name: "wheel", type: "number", min: 0, max: 1023, value: this.event.wheel || 0 })
      )
    );
    this.enableAndDisableFields();
    this.dom.spawn(form, "INPUT", { type: "submit", value: "OK", "on-click": e => this.onSubmit(e) });
  }
  
  enableAndDisableFields() {
    const type = this.element.querySelector("select[name='type']").value;
    const _ = (name, forType) => this.element.querySelector(`input[name='${name}']`).disabled = (type !== forType);
    _("noteid", "note");
    _("velocity", "note");
    _("durms", "note");
    _("wheel", "wheel");
    this.element.querySelector("input[name='chid']").disabled = ((type !== "note") && (type !== "wheel"));
  }
  
  eventFromUi() {
    const event = new SongEvent();
    event.time = +this.element.querySelector("input[name='time']").value;
    event.type = this.element.querySelector("select[name='type']").value;
    switch (event.type) {
      case "note": {
          event.chid = +this.element.querySelector("input[name='chid']").value;
          event.noteid = +this.element.querySelector("input[name='noteid']").value;
          event.velocity = +this.element.querySelector("input[name='velocity']").value;
          event.durms = +this.element.querySelector("input[name='durms']").value;
        } break;
      case "wheel": {
          event.chid = +this.element.querySelector("input[name='chid']").value;
          event.wheel = +this.element.querySelector("input[name='wheel']").value;
        } break;
    }
    return event;
  }
  
  onSubmit(event) {
    event.preventDefault();
    event.stopPropagation();
    this.resolve(this.eventFromUi());
    this.element.remove();
  }
}
