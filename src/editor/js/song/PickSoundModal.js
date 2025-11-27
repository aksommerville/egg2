/* PickSoundModal.js
 * Prompts user with all known sounds, including drum channels in songs.
 * That means the SDK drum kits (some of which are just general sound effects dumps),
 * and also everything in the user's project.
 * Resolves with an encoded EAU file.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";
import { SdkInstrumentsService } from "./SdkInstrumentsService.js";
import { Song } from "./Song.js";
import { decodeDrumModecfg } from "./EauDecoder.js";

export class PickSoundModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data, SdkInstrumentsService];
  }
  constructor(element, dom, data, sdkInstrumentsService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.sdkInstrumentsService = sdkInstrumentsService;
    
    this.noteid = null; // null if we're not asking for it
    this.name = ""; // We put the chosen sound's name here before resolving.
    this.sources = []; // {name,sounds:{name,serial}[]} ready for presentation.
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  /* Call if you want us also to ask for a noteid to assign to.
   * (drumsModel) optional decoded modecfg, so we can select a sensible default.
   * You can read (this.noteid) and (this.name) when we resolve.
   */
  setupForDrums(drumsModel) {
    this.noteid = this.defaultNoteidFromModel(drumsModel);
    this.discoverSources().then(sources => {
      this.sources = sources;
      this.buildUi();
    });
  }
  
  /* Basic setup, just the EAU file.
   * We'll populate (this.name) before resolving but not (this.noteid).
   */
  setupForSound() {
    this.noteid = null;
    this.discoverSources().then(sources => {
      this.sources = sources;
      this.buildUi();
    });
  }
  
  /* UI.
   *************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    if (typeof(this.noteid) === "number") {
      const row = this.dom.spawn(this.element, "DIV", ["row"]);
      this.dom.spawn(row, "DIV", "Assign to note:");
      this.dom.spawn(row, "INPUT", { type: "number", name: "noteid", min: 0, max: 255, value: this.noteid, "on-input": e => this.onNoteidInput(e) });
    }
    
    for (const source of this.sources) {
      const details = this.dom.spawn(this.element, "DETAILS");
      this.dom.spawn(details, "SUMMARY", source.name);
      const list = this.dom.spawn(details, "UL", ["sounds"]);
      for (const sound of source.sounds) {
        const li = this.dom.spawn(list, "LI");
        this.dom.spawn(li, "INPUT", { type: "button", value: sound.name, "on-click": () => this.onChooseSound(sound.name, sound.serial) });
      }
    }
  }
  
  /* Model.
   ***********************************************************************/
   
  defaultNoteidFromModel(model) {
    if (!model?.drums) return 35; // GM drums are 35..81, good a default as any.
    const noteidInUse = new Set();
    let lowestEmptyNoteid = 256; // If <256, it's an extant drum with empty serial. Likely candidates for importing to.
    for (const drum of model.drums) {
      noteidInUse.add(drum.noteid);
      if (!drum.serial?.length) {
        if (drum.noteid < lowestEmptyNoteid) {
          lowestEmptyNoteid = drum.noteid;
        }
      }
    }
    // If there's a drum with empty serial, the lowest of them is the preferred default.
    if (lowestEmptyNoteid < 256) return lowestEmptyNoteid;
    // If there's a gap in 35..81, take the lowest.
    for (let noteid=35; noteid<=81; noteid++) {
      if (!noteidInUse.has(noteid)) return noteid;
    }
    // Then 1..34 and 82..255.
    for (let noteid=1; noteid<=34; noteid++) {
      if (!noteidInUse.has(noteid)) return noteid;
    }
    for (let noteid=82; noteid<=255; noteid++) {
      if (!noteidInUse.has(noteid)) return noteid;
    }
    // And finally zero, even if it's already in use.
    return 0;
  }
  
  /* Resolves with an array ready for (this.sources) but does not assign on its own.
   */
  discoverSources() {
    return this.sdkInstrumentsService.getInstruments().catch(e => {
      return null;
    }).then(sdkSong => {
      const sources = []; // {name,sounds:{name,serial}[]}
      let soundsSource = null;
      
      // If the SDK Instruments exist, they're a container just like a song resource.
      if (sdkSong) this.addSourcesFromSong(sources, "SDK Instruments", sdkSong);
    
      for (const res of this.data.resv) {
        if (!res?.serial?.length) continue;
        
        let isContainer = false;
        if (res.type === "song") {
          isContainer = true;
        } else if (res.type === "sound") {
        } else {
          // It would be OK to let other types thru, but we'd have to know whether they're containers or discrete sounds.
          continue;
        }
        
        // EAU and MIDI are distinguishable by the first four bytes.
        const signature = (res.serial[0] << 24) | (res.serial[1] << 16) | (res.serial[2] << 8) | res.serial[3];
        
        // If we're not a container (ie "sound"), and the serial is EAU, we can save some effort and add it immediately.
        // This case is likely enough to warrant the special short-circuit.
        if (!isContainer && (signature === 0x00454155)) {
          if (!soundsSource) sources.push(soundsSource = { name: "Sounds", sounds: [] });
          soundsSource.sounds.push({
            name: res.path.replace(/^.*\//, ""),
            serial: res.serial,
          });
          continue;
        }
        
        // Container or non-EAU format, we need to decode it.
        let song = null;
        try {
        
          if (signature === 0x00454155) song = isContainer ? Song.withoutEvents(res.serial) : new Song(res.serial);
          else if (signature === 0x4d546864) song = isContainer ? Song.fromMidiWithoutEvents(res.serial) : null;
          // I'm not supporting MIDI for sound resources here, tho in general we do.
          // If we supported it, we'd have to bounce off the server for every sound, every time this modal opens.
          // I guess if that case does come up, we could use some placeholder here (the "null" above), and decode after the user selects it.
          
          if (!song) continue;
          const name = res.path.replace(/^.*\//, "");
          
          if (isContainer) {
            this.addSourcesFromSong(sources, name, song);
          } else {
            if (!soundsSource) sources.push(soundsSource = { name: "Sounds", sounds: [] });
            soundsSource.sounds.push({
              name,
              serial: song.encode(),
            });
          }
        } catch (e) {
          console.warn(`Failed to decode ${res.path}.`, e);
        }
      }
      
      return sources;
    });
  }
  
  addSourcesFromSong(sources, name, song) {
    for (const channel of song.channels) {
      if (channel.mode !== 4) continue; // Only interested in DRUM channels.
      const source = {
        name: name + ": " + song.getNameForce(channel.chid, 0xff),
        sounds: [],
      };
      const drums = decodeDrumModecfg(channel.modecfg);
      for (const drum of drums) {
        if (!drum.serial?.length) continue;
        source.sounds.push({
          name: song.getName(channel.chid, drum.noteid) || drum.noteid.toString(), // Don't use force, it adds a prefix to extant names.
          serial: drum.serial,
        });
      }
      if (source.sounds.length) {
        sources.push(source);
      }
    }
  }
  
  /* Events.
   ************************************************************************/
   
  onNoteidInput(event) {
    const noteid = +event.target.value;
    if (isNaN(noteid) || (noteid < 0) || (noteid > 0xff)) return;
    this.noteid = noteid;
  }
  
  onChooseSound(name, serial) {
    this.name = name;
    this.resolve(serial);
    this.element.remove();
  }
}
