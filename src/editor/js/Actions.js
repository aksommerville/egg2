/* Actions.js
 * Repository of global actions and editors.
 * Manages editor selection.
 */
 
import { Override } from "../Override.js";
import { Dom } from "./Dom.js";
import { LaunchService } from "./LaunchService.js";
import { TextEditor } from "./std/TextEditor.js";
import { HexEditor } from "./std/HexEditor.js";
import { CommandListEditor } from "./std/CommandListEditor.js";
import { ImageEditor } from "./std/ImageEditor.js";
import { TilesheetEditor } from "./std/TilesheetEditor.js";
import { DecalsheetEditor } from "./std/DecalsheetEditor.js";
import { SpriteEditor } from "./std/SpriteEditor.js";
import { MapEditor } from "./map/MapEditor.js";
import { SongEditor } from "./song/SongEditor.js";
import { StringsEditor } from "./std/StringsEditor.js";
import { MissingResourcesService } from "./std/MissingResourcesService.js";
import { WorldMapModal } from "./map/WorldMapModal.js";
import { SdkInstrumentsService } from "./song/SdkInstrumentsService.js";
 
export class Actions {
  static getDependencies() {
    return [Override, Dom, Window, LaunchService, MissingResourcesService, SdkInstrumentsService];
  }
  constructor(override, dom, window, launchService, missingResourcesService, sdkInstrumentsService) {
    this.override = override;
    this.dom = dom;
    this.window = window;
    this.launchService = launchService;
    this.missingResourcesService = missingResourcesService;
    this.sdkInstrumentsService = sdkInstrumentsService;
    
    this.actions = [
      ...this.override.actions,
      { name: "launch", label: "Launch", fn: () => this.launchService.launch() },
      { name: "missingResources", label: "Missing Resources...", fn: () => this.missingResourcesService.detectAndReport() },
      { name: "worldMap", label: "World Map...", fn: () => this.dom.spawnModal(WorldMapModal) },
      { name: "editSdkInstruments", label: "Edit SDK Instruments...", fn: () => this.sdkInstrumentsService.edit() },
    ];
    
    this.editors = [
      ...this.override.editors,
      TilesheetEditor,
      DecalsheetEditor,
      SpriteEditor,
      MapEditor,
      SongEditor,
      ImageEditor,
      StringsEditor,
      CommandListEditor,
      TextEditor,
      HexEditor,
    ];
    
    this.selectedPath = ""; // RootUi populates when it loads an editor. Not 100% reliable.
  }
  
  /* Returns the preferred editor class, synchronously.
   * If ambiguous, we return the first candidate.
   */
  pickEditorForResourceSync(res) {
    let fallback = null;
    for (const editor of this.editors) {
      switch (editor.checkResource(res)) {
        case 2: return editor;
        case 1: if (!fallback) fallback = editor; break;
      }
    }
    return fallback;
  }
  
  /* Resolves with an editor class, or rejects.
   * If there's a preferred editor, and (usePreferred), we resolve with that without asking.
   * Otherwise we'll present a modal for the user to select one.
   */
  pickEditorForResourceAsync(res, usePreferred) {
    const candidates = [];
    for (const editor of this.editors) {
      switch (editor.checkResource(res)) {
        case 2: if (usePreferred) return Promise.resolve(editor); // pass
        case 1: candidates.push(editor); break;
      }
    }
    if (!candidates.length) return Promise.reject("No suitable editor found.");
    if (candidates.length === 1) return Promise.resolve(candidates[0]);
    return this.dom.modalPickOne("Pick editor:", candidates.map(c => c.name))
      .then(name => candidates.find(c => c.name === name))
      .then(editor => {
        if (!editor) throw new Error("Cancelled.");
        return editor;
      });
  }
  
  editResource(path, editorName) {
    let url = "#path=" + encodeURIComponent(path).replace(/%2[fF]/g, "/"); // Slashes are OK in the fragment, and there will always be some. Keep it legible.
    if (editorName) url += "&editor=" + encodeURIComponent(editorName);
    this.window.location = url;
  }
  
  // From the URL fragment.
  getCurrentResourcePath() {
    const hash = this.window.location.hash;
    if (!hash) return "";
    for (const field of hash.substring(1).split("&")) {
      const split = field.split("=");
      if (split.length < 2) continue;
      if (split[0] !== "path") continue;
      return decodeURIComponent(split[1]);
    }
    return ""
  }
}

Actions.singleton = true;
