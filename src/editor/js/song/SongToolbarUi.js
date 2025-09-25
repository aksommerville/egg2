/* SongToolbarUi.js
 * Top ribbon of SongEditor.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";
import { SongChannel } from "./Song.js";
import { EventModal } from "./EventModal.js";

export class SongToolbarUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService, Window];
  }
  constructor(element, dom, songService, window) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    this.window = window;
    
    this.animationFrame = null;
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.animationFrame) {
      this.window.cancelAnimationFrame(this.animationFrame);
      this.animationFrame = null;
    }
  }
  
  /* UI.
   *********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    // Playback controls.
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "!", "on-click": () => this.onPanic() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay() });
    
    this.dom.spawn(this.element, "CANVAS", ["playhead"], { "on-click": e => this.onClickPlayhead(e) });
    
    // Visibility toggles.
    // Older versions had Track and Event filters, but since we're EAU only, really only Channel matters.
    this.dom.spawn(this.element, "SELECT", { name: "visChannel", "on-change": () => this.onVisChannelChange() });
    
    const actionsMenu = this.dom.spawn(this.element, "SELECT", { name: "actions", "on-change": () => this.onActionsChange() },
      this.dom.spawn(null, "OPTION", { value: "", disabled: "disabled" }, "Actions..."),
      this.dom.spawn(null, "OPTION", { value: "dropUnusedNames" }, "Drop Unused Names"),
      this.dom.spawn(null, "OPTION", { value: "dropAllNames" }, "Drop All Names"),
      this.dom.spawn(null, "OPTION", { value: "autoStartTime" }, "Auto Start Time"),
      this.dom.spawn(null, "OPTION", { value: "autoEndTime" }, "Auto End Time"),
      this.dom.spawn(null, "OPTION", { value: "transpose" }, "Transpose..."),
      this.dom.spawn(null, "OPTION", { value: "filter" }, "Filter..."),
      this.dom.spawn(null, "OPTION", { value: "reduceWheels" }, "Reduce Wheels..."),
      this.dom.spawn(null, "OPTION", { value: "adjustVelocities" }, "Adjust Velocities..."),
    );
    actionsMenu.value = "";
    
    // Simple actions.
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "+Channel", "on-click": () => this.onAddChannel() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "+Event", "on-click": () => this.onAddEvent() });
    
    this.populateVisibility();
    this.renderPlayhead();
  }
  
  populateVisibility() {
    let select;
    if (select = this.element.querySelector("select[name='visChannel']")) {
      select.innerHTML = "";
      this.dom.spawn(select, "OPTION", { value: "" }, "All Channels");
      this.dom.spawn(select, "OPTION", { value: "-1" }, "Channelless Events");
      if (this.songService.song) {
        for (let chid=0; chid<this.songService.song.channelsByChid.length; chid++) {
          if (this.songService.song.channelsByChid[chid]) {
            this.dom.spawn(select, "OPTION", { value: chid }, `Channel ${chid}`);
          }
        }
      }
      select.value = this.songService.visChid ?? "";
    }
  }
  
  renderPlayhead() {
    const canvas = this.element.querySelector(".playhead");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = this.songService.playing ? "#630" : "#555";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    ctx.moveTo(0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, 0.5);
    ctx.strokeStyle = "#aaa";
    ctx.stroke();
    if (!this.songService.playing) return;
    const x = ~~(this.songService.getNormalizedPlayhead() * bounds.width) + 0.5;
    ctx.moveTo(x, 0);
    ctx.lineTo(x, bounds.height);
    ctx.stroke();
  }
  
  requirePlayhead() {
    if (this.animationFrame) return;
    this.animationFrame = this.window.requestAnimationFrame(() => {
      this.animationFrame = null;
      this.renderPlayhead();
      this.requirePlayhead();
    });
  }
  
  cancelPlayhead() {
    if (!this.animationFrame) return;
    this.window.cancelAnimationFrame(this.animationFrame);
    this.animationFrame = null;
    this.renderPlayhead(); // Once more to draw it neutral.
  }
  
  /* Events.
   **************************************************************************************/
   
  onPanic() {
    this.songService.playSong(null);
    this.cancelPlayhead();
  }
  
  onPlay() {
    this.songService.playSong(this.songService.song);
    this.requirePlayhead();
  }
  
  onClickPlayhead(event) {
    if (!this.songService.playing) return;
    const canvas = this.element.querySelector(".playhead");
    const bounds = canvas.getBoundingClientRect();
    const x = event.x - bounds.x;
    this.songService.setNormalizedPlayhead(x / bounds.width);
  }
  
  onVisChannelChange() {
    const value = this.element.querySelector("select[name='visChannel']")?.value;
    const chid = value ? +value : null;
    this.songService.visChid = chid;
    this.songService.broadcast("visibility");
  }
  
  onAddChannel() {
    const song = this.songService.song;
    if (!song) return;
    const chid = song.unusedChid();
    if (chid < 0) {
      this.dom.modalError(`All channels in use.`);
      return;
    }
    const channel = new SongChannel(chid);
    song.channels.push(channel);
    song.channelsByChid[chid] = channel;
    this.songService.broadcast("dirty");
    this.songService.broadcast("channelSetChanged");
  }
  
  onAddEvent() {
    const modal = this.dom.spawnModal(EventModal);
    modal.setup(null);
    modal.result.then(rsp => {
      if (!rsp) return;
      this.songService.song.insertEvent(rsp);
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
    });
  }
  
  onActionsChange() {
    const select = this.element.querySelector("select[name='actions']");
    const value = select.value;
    select.value = "";
    const fn = this["action_" + value];
    if (!fn) {
      this.dom.modalError(`SongToolbarUi: Unknown action ${JSON.stringify(value)}`);
      return;
    }
    fn.bind(this)();
  }
  
  /* Actions.
   ***********************************************************************************/
  
  action_dropUnusedNames() {
    if (!this.songService.song) return;
    let changed = false;
    for (let i=this.songService.song.text.length; i-->0; ) {
      const label = this.songService.song.text[i];
      if (label[1]) {
        if (this.songService.song.noteInUse(label[0], label[1])) continue;
      } else {
        if (this.songService.song.channelsByChid[label[0]]) continue;
      }
      this.songService.song.text.splice(i, 1);
      changed = true;
    }
    if (!changed) return;
    this.songService.broadcast("dirty");
  }
  
  action_dropAllNames() {
    if (!this.songService.song?.text.length) return;
    this.songService.song.text = [];
    this.songService.broadcast("dirty");
    this.songService.broadcast("channelSetChanged");
  }
  
  action_autoStartTime() {
    if (!this.songService.song) return;
    if (!this.songService.song.events.length) return;
    if (!this.songService.song.events[0].time) return; // Already aligned.
    const d = -this.songService.song.events[0].time;
    for (const event of this.songService.song.events) {
      event.time -= d;
    }
    this.songService.broadcast("dirty");
    this.songService.broadcast("eventsChanged");
  }
  
  action_autoEndTime() {
    if (!this.songService.song) return;
    if (this.songService.song.forceMinimumEndTime()) {
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
      //TODO If the song contains any Delay stages, can we show a gentle toast or something to remind the user that delay tails are not accounted for?
    }
  }
  
  action_transpose() {
    console.log(`TODO SongToolbarUi.transpose`);
  }
  
  action_filter() {
    console.log(`TODO SongToolbarUi.filter`);
  }
  
  action_reduceWheels() {
    console.log(`TODO SongToolbarUi.reduceWheels`);
  }
  
  action_adjustVelocities() {
    console.log(`TODO SongToolbarUi.adjustVelocities`);
  }
}
