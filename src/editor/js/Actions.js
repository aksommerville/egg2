/* Actions.js
 * Repository of global actions and editors.
 * Manages editor selection.
 */
 
import { Override } from "../Override.js";
import { Dom } from "./Dom.js";
import { TextEditor } from "./TextEditor.js";
import { HexEditor } from "./HexEditor.js";
 
export class Actions {
  static getDependencies() {
    return [Override, Dom, Window];
  }
  constructor(override, dom, window) {
    this.override = override;
    this.dom = dom;
    this.window = window;
    
    this.actions = [
      ...this.override.actions,
      //TODO standard actions
      //{ name: "exampleAction1", label: "Example Action 1", fn: () => this.exampleAction1() },
    ];
    
    this.editors = [
      ...this.override.editors,
      //TODO standard editors
      TextEditor,
      HexEditor,
    ];
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
    return this.dom.modalPickOne(candidates.map(c => c.name))
      .then(name => candidates.find(c => c.name === name))
      .then(editor => {
        if (!editor) throw new Error("Cancelled.");
        return editor;
      });
  }
  
  editResource(path, editorName) {
    let url = "#path=" + encodeURIComponent(path);
    if (editorName) url += "&editor=" + encodeURIComponent(editorName);
    this.window.location = url;
  }
}

Actions.singleton = true;
