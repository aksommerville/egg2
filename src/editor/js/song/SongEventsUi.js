/* SongEventsUi.js
 * Right side of SongEditor, shows all the events.
 */
 
import { Dom } from "../Dom.js";
import { SongService } from "./SongService.js";

export class SongEventsUi {
  static getDependencies() {
    return [HTMLElement, Dom, SongService];
  }
  constructor(element, dom, songService) {
    this.element = element;
    this.dom = dom;
    this.songService = songService;
  }
}
