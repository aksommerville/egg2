/* SongToolbarUi.js
 * Top ribbon of SongEditor.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";

export class SongToolbarUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
    
    this.buildUi();
  }
  
  /* UI.
   *********************************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    
    // Playback controls.
    this.dom.spawn(this.element, "INPUT", { type: "button", value: "!", "on-click": () => this.onPanic() });
    this.dom.spawn(this.element, "INPUT", { type: "button", value: ">", "on-click": () => this.onPlay() });
    
    this.dom.spawn(this.element, "CANVAS", ["playhead"], { "on-click": () => this.onClickPlayhead() });
    
    // Visibility toggles.
    // Older versions had Track and Event filters, but since we're EAU only, really only Channel matters.
    this.dom.spawn(this.element, "SELECT", { name: "visChannel", "on-change": () => this.onVisChannelChange() });
    
    const actionsMenu = this.dom.spawn(this.element, "SELECT", { name: "actions", "on-change": () => this.onActionsChange() },
      this.dom.spawn(null, "OPTION", { value: "", disabled: "disabled" }, "Actions..."),
      this.dom.spawn(null, "OPTION", { value: "example1" }, "Example Action 1"),
      this.dom.spawn(null, "OPTION", { value: "example2" }, "Example Action 2")
      //TODO actions
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
    ctx.fillStyle = "#630";
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    ctx.moveTo(0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, 0.5);
    ctx.lineTo(bounds.width - 0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, bounds.height - 0.5);
    ctx.lineTo(0.5, 0.5);
    ctx.strokeStyle = "#aaa";
    ctx.stroke();
    //TODO playhead
  }
  
  /* Events.
   **************************************************************************************/
   
  onPanic() {
    console.log(`TODO SongToolbarUi.onPanic`); //TODO Stop playing.
  }
  
  onPlay() {
    console.log(`TODO SongToolbarUi.onPlay`); // TODO Start playing.
  }
  
  onClickPlayhead() {
    console.log(`TODO SongToolbarUi.onClickPlayhead`); // TODO Start playing at a given time.
  }
  
  onVisChannelChange() {
    const value = this.element.querySelector("select[name='visChannel']")?.value;
    const chid = value ? +value : null;
    this.songService.visChid = chid;
    this.songService.broadcast("visibility");
  }
  
  onAddChannel() {
    console.log(`TODO SongToolbarUi.onAddChannel`);
  }
  
  onAddEvent() {
    console.log(`TODO SongToolbarUi.onAddEvent`);
  }
  
  onActionsChange() {
    const select = this.element.querySelector("select[name='actions']");
    const value = select.value;
    select.value = "";
    console.log(`TODO SongToolbarUi.onActionsChange: ${value}`);
  }
}
