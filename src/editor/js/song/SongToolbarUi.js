/* SongToolbarUi.js
 * Top ribbon of SongEditor.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";
import { SongChannel } from "./Song.js";
import { EventModal } from "./EventModal.js";
import { EventFilterModal } from "./EventFilterModal.js";

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
    this.playheadBackground = null; // Canvas
    
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
      this.dom.spawn(null, "OPTION", { value: "tempo" }, "Tempo..."),
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
    // Rendering the playhead determines channel colors by searching for the channel header elements.
    // That's stupid and hacky, but it's the best I could do to not have to repeat the CSS channel color rules.
    // We are usually created before the channel header cards, so defer the first playhead render by a smidgeon.
    this.window.setTimeout(() => {
      this.renderPlayhead(true);
    }, 20);
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
  
  renderPlayhead(refreshEvents) {
    const canvas = this.element.querySelector(".playhead");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    if (refreshEvents || !this.playheadBackground || (this.playheadBackground.width !== bounds.width) || (this.playheadBackground.height !== bounds.height)) {
      this.renderPlayheadBackground(bounds.width, bounds.height, this.songService.song);
    }
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = this.songService.playing ? "#fff" : "#aaa";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    ctx.drawImage(this.playheadBackground, 0, 0, bounds.width, bounds.height, 0, 0, bounds.width, bounds.height);
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
  
  renderPlayheadBackground(w, h, song) {
    if (!this.playheadBackground) {
      this.playheadBackground = this.dom.document.createElement("CANVAS");
    }
    this.playheadBackground.width = w;
    this.playheadBackground.height = h;
    const ctx = this.playheadBackground.getContext("2d");
    ctx.clearRect(0, 0, w, h);
    if ((song?.channels?.length || 0) < 1) return;
    const durs = song.calculateDuration();
    if (durs < 0.250) return; // Allow low durations, tho it won't be helpful, but stop if it's zero.
    const pxPerMs = w / (durs * 1000);
    const timeFuzz = 3 / pxPerMs; // How far apart must events be, in ms, to break the line?
    const yspacing = h / (song.channels.length + 1);
    ctx.lineWidth = 2;
    for (let i=0, y=yspacing; i<song.channels.length; i++, y+=yspacing) {
      const channel = song.channels[i];
      let color = "";
      const element = this.dom.document.querySelector(`.chid-${channel.chid}`);
      if (element) {
        const style = this.window.getComputedStyle(element);
        color = style.backgroundColor;
      }
      ctx.strokeStyle = color || "#fff";
      const yq = Math.floor(y);
      ctx.beginPath();
      
      // The interesting part: Scan all events for this channel and break them into horizontal lines.
      let startTime=null, recentTime=null;
      for (let evtp=0; evtp<song.events.length; evtp++) {
        const event = song.events[evtp];
        if (event.chid !== channel.chid) continue;
        if (startTime === null) {
          startTime = recentTime = event.time;
        } else if (event.time > recentTime + timeFuzz) {
          let xa=startTime*pxPerMs, xz=recentTime*pxPerMs;
          if (xz < xa + 3) xz = xa + 3;
          ctx.moveTo(xa, yq);
          ctx.lineTo(xz, yq);
          startTime = recentTime = event.time;
        } else {
          recentTime = event.time;
        }
      }
      if (recentTime) {
        let xa=startTime*pxPerMs, xz=recentTime*pxPerMs;
        if (xz < xa + 3) xz = xa + 3;
        ctx.moveTo(xa, yq);
        ctx.lineTo(xz, yq);
      }
      
      ctx.stroke();
    }
    ctx.lineWidth = 1;
  }
  
  requirePlayhead(refreshEvents) {
    if (this.animationFrame) return;
    this.animationFrame = this.window.requestAnimationFrame(() => {
      this.animationFrame = null;
      this.renderPlayhead(refreshEvents);
      this.requirePlayhead(false);
    });
  }
  
  cancelPlayhead() {
    if (!this.animationFrame) return;
    this.window.cancelAnimationFrame(this.animationFrame);
    this.animationFrame = null;
    this.renderPlayhead(false); // Once more to draw it neutral.
  }
  
  /* Events.
   **************************************************************************************/
   
  onPanic() {
    this.songService.playSong(null);
    this.cancelPlayhead();
  }
  
  onPlay() {
    this.songService.playSong(this.songService.song);
    this.requirePlayhead(true);
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
   
  action_tempo() {
    if (!this.songService.song) return;
    this.dom.modalText("Tempo, ms/qnote. Affects LFOs and Post, not actual note timing.", this.songService.song.tempo).then(rsp => {
      if (!rsp) return;
      rsp = +rsp;
      if ((rsp < 1) || (rsp > 0x7fff)) throw "Unreasonable tempo. Try again.";
      if (rsp === this.songService.song.tempo) return;
      this.songService.song.tempo = rsp;
      this.songService.broadcast("dirty");
    }).catch(e => this.dom.modalError(e));
  }
  
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
      let hasDelay = !!this.songService.song.channels.find((channel) => {
        for (let srcp=0; srcp<channel.post.length; ) {
          if (channel.post[srcp++] === 0x01) {
            return true;
          }
          const len = channel.post[srcp++];
          srcp += len;
        }
        return false;
      });
      if (hasDelay) {
        this.dom.toast("Delay tails not accounted for in auto end time.", "#ff0");
      }
    }
  }
  
  action_transpose() {
    if (!this.songService.song) return;
    const modal = this.dom.spawnModal(EventFilterModal);
    modal.setup("transpose", this.songService.song);
    modal.result.then(rsp => {
      if (!rsp) return;
      this.songService.song.events = rsp;
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
    }).catch(e => this.dom.modalError(e));
  }
  
  action_filter() {
    if (!this.songService.song) return;
    const modal = this.dom.spawnModal(EventFilterModal);
    modal.setup("filter", this.songService.song);
    modal.result.then(rsp => {
      this.songService.song.events = rsp;
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
    }).catch(e => this.dom.modalError(e));
  }
  
  action_reduceWheels() {
    if (!this.songService.song) return;
    const modal = this.dom.spawnModal(EventFilterModal);
    modal.setup("reduceWheels", this.songService.song);
    modal.result.then(rsp => {
      this.songService.song.events = rsp;
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
    }).catch(e => this.dom.modalError(e));
  }
  
  action_adjustVelocities() {
    if (!this.songService.song) return;
    const modal = this.dom.spawnModal(EventFilterModal);
    modal.setup("adjustVelocities", this.songService.song);
    modal.result.then(rsp => {
      this.songService.song.events = rsp;
      this.songService.broadcast("dirty");
      this.songService.broadcast("eventsChanged");
    }).catch(e => this.dom.modalError(e));
  }
}
