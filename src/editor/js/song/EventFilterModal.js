/* EventFilterModal.js
 * Configures and executes several actions that operate on a configurable set of song events.
 */
 
import { Dom } from "../Dom.js";
import { SongEvent } from "./Song.js";

export class EventFilterModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Window, "nonce"];
  }
  constructor(element, dom, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.window = window;
    this.nonce = nonce;
    
    this.mode = "";
    this.song = null;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
    
    this.element.addEventListener("input", e => this.onInput(e));
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(mode, song) {
    this.mode = mode;
    this.song = song;
    switch (mode) {
      case "transpose": this.buildUiTranspose(); break;
      case "filter": this.buildUiFilter(); break;
      case "reduceWheels": this.buildUiReduceWheels(); break;
      case "adjustVelocities": this.buildUiAdjustVelocities(); break;
      default: throw new Error(`Unexpected mode ${JSON.stringify(mode)} for EventFilterModal`);
    }
    this.onInput(null); // Force an initial read of the model, populates eventCount
  }
  
  /* UI
   **********************************************************************/
  
  buildUiTranspose() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", "Transpose...");
    this.form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.addChannelSelect();
    this.addTimeSelect();
    this.addNoteidSelect();
    this.addNoteidDelta();
    this.addFinalUi();
  }
  
  buildUiFilter() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", "Delete all matching events:");
    this.form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.addChannelSelect();
    this.addTimeSelect();
    this.addTypeSelect();
    this.addNoteidSelect();
    this.addVelocitySelect();
    this.addFinalUi();
  }
  
  buildUiReduceWheels() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", "Reduce Wheels...");
    this.form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.addChannelSelect();
    this.addTimeSelect();
    this.addWheelGranularity();
    this.addFinalUi();
  }
  
  buildUiAdjustVelocities() {
    this.element.innerHTML = "";
    this.dom.spawn(this.element, "H2", "Adjust Velocities...");
    this.form = this.dom.spawn(this.element, "FORM", { "on-submit": e => this.onSubmit(e) });
    this.addChannelSelect();
    this.addTimeSelect();
    this.addNoteidSelect();
    this.addVelocitySelect();
    this.addVelocityDelta();
    this.addFinalUi();
  }
  
  addChannelSelect() {
    const container = this.dom.spawn(this.form, "DIV", ["channelSelect"]);
    if (this.mode === "filter") {
      const id = `EventFilterModal-${this.nonce}-channel--1`;
      const box = this.dom.spawn(container, "DIV", ["box"]);
      const input = this.dom.spawn(box, "INPUT", { type: "checkbox", name: `channel--1`, id, checked: true });
      const label = this.dom.spawn(box, "LABEL", { for: id }, "channelless");
    }
    for (const channel of this.song.channels) {
      const id = `EventFilterModal-${this.nonce}-channel-${channel.chid}`;
      const box = this.dom.spawn(container, "DIV", ["box"]);
      const input = this.dom.spawn(box, "INPUT", { type: "checkbox", name: `channel-${channel.chid}`, id, checked: true });
      const label = this.dom.spawn(box, "LABEL", { for: id }, this.song.getNameForce(channel.chid));
      box.style.backgroundColor = this.channelColor(channel.chid);
    }
  }
  
  channelColor(chid) {
    let color = "";
    const element = this.dom.document.querySelector(`.chid-${chid}`);
    if (element) {
      const style = this.window.getComputedStyle(element);
      color = style.backgroundColor;
    }
    return color || "#fff";
  }
  
  addTimeSelect() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Time:");
    this.dom.spawn(row, "INPUT", { type: "number", name: "timelo", value: "0" });
    this.dom.spawn(row, "INPUT", { type: "number", name: "timehi", value: Math.ceil(this.song.calculateDuration() * 1000 + 1) });
  }
  
  addTypeSelect() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Type:");
    for (const type of ["noop", "note", "wheel", "loop"]) {
      const id = `EventFilterModal-${this.nonce}-type-${type}`;
      const box = this.dom.spawn(row, "DIV", ["box"]);
      const input = this.dom.spawn(box, "INPUT", { type: "checkbox", name: `type-${type}`, id, checked: true });
      const label = this.dom.spawn(box, "LABEL", { for: id }, type);
    }
  }
  
  addNoteidSelect() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Notes:");
    this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 127, name: "noteidlo", value: "0" });
    this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 128, name: "noteidhi", value: "128" });
  }
  
  addVelocitySelect() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Velocity:");
    this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 127, name: "velocitylo", value: "0" });
    this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 128, name: "velocityhi", value: "128" });
  }
  
  addWheelGranularity() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Granularity:");
    this.dom.spawn(row, "INPUT", { type: "number", min: 0, max: 1024, name: "granularity", value: "0" });
  }
  
  addNoteidDelta() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Transpose:");
    this.dom.spawn(row, "INPUT", { type: "number", min: -127, max: 127, name: "transpose", value: "0" });
  }
  
  addVelocityDelta() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "DIV", "Delta:");
    this.dom.spawn(row, "INPUT", { type: "number", min: -127, max: 127, name: "velta", value: "0" });
  }
  
  addFinalUi() {
    const row = this.dom.spawn(this.form, "DIV", ["row"]);
    this.dom.spawn(row, "SPAN", "Affected events: ");
    this.dom.spawn(row, "SPAN", ["eventCount"], "?");
    this.dom.spawn(row, "SPAN", ` / ${this.song.events.length}`);
    this.dom.spawn(this.form, "INPUT", { type: "submit", value: "OK" });
  }
  
  /* Model.
   *********************************************************************************/
   
  readModelFromUi() {
    const model = {
      chids: [],
      types: [],
      timelo: 0,
      timehi: 0,
      noteidlo: 0,
      noteidhi: 128,
      velocitylo: 0,
      velocityhi: 128,
      granularity: 0,
      transpose: 0,
      velta: 0,
    };
    for (const input of this.element.querySelectorAll("input")) {
      if (input.type === "checkbox") {
        if (!input.checked) continue;
        if (input.name.startsWith("channel-")) model.chids.push(+input.name.substring(8));
        else if (input.name.startsWith("type-")) model.types.push(input.name.substring(5));
        else console.error(`EventFilterModal.readModelFromUi: Unexpected checkbox ${JSON.stringify(input.name)}`);
      } else if (input.type === "number") {
        const v = +input.value;
        if (isNaN(v)) return null;
        model[input.name] = v;
      } else if (input.type === "submit") {
      } else {
        console.error(`EventFilterModal.readModelFromUi: Unexpected input type ${JSON.stringify(input.type)}`);
      }
    }
    // Most modes match just one type and don't ask. Fill those in so the model is generic.
    // "filter" does ask.
    switch (this.mode) {
      case "transpose": model.types = ["note"]; break;
      case "reduceWheels": model.types = ["wheel"]; break;
      case "adjustVelocities": mode.types = ["note"]; break;
    }
    return model;
  }
  
  // Returns events from (song.events) that match the intake criteria from (model).
  // We don't care what mode is in play.
  findEvents(song, model) {
    return song.events.filter(event => {
      if (!model.chids.includes(event.chid)) return false;
      if (!model.types.includes(event.type)) return false;
      if (event.time < model.timelo) return false;
      if (event.time >= model.timehi) return false;
      if (event.type === "note") {
        if (event.noteid < model.noteidlo) return false;
        if (event.noteid >= model.noteidhi) return false;
        if (event.velocity < model.velocitylo) return false;
        if (event.velocity >= model.velocityhi) return false;
      }
      return true;
    });
  }
  
  /* (allEvents) from the song untouched, (matchedEvents) from this.findEvents.
   * Returns a new event list with respect to (model)'s modification params and (this.mode).
   */
  applyFilter(allEvents, matchedEvents, model) {
    const matchedIds = new Set(matchedEvents.map(e => e.id));
    switch (this.mode) {
      case "transpose": return this.doTranspose(allEvents, matchedIds, model.transpose);
      case "filter": return this.doFilter(allEvents, matchedIds);
      case "reduceWheels": return this.doReduceWheels(allEvents, matchedIds, model.granularity);
      case "adjustVelocities": return this.doAdjustVelocities(allEvents, matchedIds, model.velta);
    }
    return allEvents;
  }
  
  doTranspose(events, ids, d) {
    if (!d) return events;
    return events.map(event => {
      if (!ids.has(event.id)) return event;
      const nevent = new SongEvent(event);
      nevent.id = event.id;
      event = nevent;
      event.noteid += d;
      if (event.noteid < 1) event.noteid = 1;
      else if (event.noteid > 127) event.noteid = 127;
      return event;
    });
  }
  
  doFilter(events, ids) {
    return events.filter(event => !ids.has(event.id));
  }
  
  doReduceWheels(events, ids, granularity) {
    if (granularity < 1) return events;
    const wheelByChid = this.song.channelsByChid.map(channel => 512);
    const nevents = [];
    for (const event of events) {
      // Keep all unmatched events.
      if (!ids.has(event.id)) {
        nevents.push(event);
        continue;
      }
      // If they're the same value, even zero, drop it.
      if (wheelByChid[event.chid] === event.wheel) continue;
      // Emit if it's to or from zero, or if the delta from current state is at least +-granularity.
      // Note that wheel values are stored as encoded, so when I say "zero" I mean 512.
      if (
        (wheelByChid[event.chid] === 512) ||
        (event.wheel === 512) ||
        (Math.abs(event.wheel - (wheelByChid[event.chid] || 0)) >= granularity)
      ) {
        wheelByChid[event.chid] = event.wheel;
        nevents.push(event);
      }
      // Drop otherwise.
    }
    return nevents;
  }
  
  doAdjustVelocities(events, ids, d) {
    if (!d) return events;
    return events.map(event => {
      if (!ids.has(event.id)) return event;
      const nevent = new SongEvent(event);
      nevent.id = event.id;
      event = nevent;
      event.velocity += d;
      if (event.velocity < 1) event.velocity = 1;
      else if (event.velocity > 127) event.velocity = 127;
      return event;
    });
  }
  
  /* Events.
   ********************************************************************************/
   
  onInput(event) {
    const model = this.readModelFromUi();
    if (!model) {
      this.element.querySelector(".eventCount").innerText = "ERROR";
      return;
    }
    const events = this.findEvents(this.song, model);
    this.element.querySelector(".eventCount").innerText = events.length;
  }
  
  onSubmit(event) {
    event.stopPropagation();
    event.preventDefault();
    // We could already have model and events from the last onInput but meh.
    const model = this.readModelFromUi();
    if (!model) return;
    const inputEvents = this.findEvents(this.song, model);
    const events = this.applyFilter(this.song.events, inputEvents, model);
    this.resolve(events);
    this.element.remove();
  }
}
