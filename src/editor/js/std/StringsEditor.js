/* StringsEditor.js
 * Show two strings resources as side-by-side lists.
 * We're unusual among editors, in that there's usually two resources in play at once.
 * User may select a reference language, to aid translation.
 */
 
import { Dom } from "../Dom.js";
import { Data } from "../Data.js";

export class StringsEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Navigator];
  }
  constructor(element, dom, data, navigator) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.navigator = navigator;
    
    this.res = null;
    this.ref = null;
  }
  
  static checkResource(res) {
    if (res.type === "strings") return 2;
    return 0;
  }
  
  setup(res) {
    this.res = res;
    
    /* Try to find another strings resource of the same sub-rid but different language.
     * Prefer languages in the order of (navigator.languages), or fallback to English, or take whatever we can find.
     * If we find one, they are both editable.
     */
    this.ref = null;
    const candidates = this.data.resv.filter(r => ((r.type === "strings") && (r.rid === res.rid) && (r.lang !== res.lang)));
    if (candidates.length) {
      let prefs = navigator?.languages?.map(l => l.substring(0, 2));
      if (!prefs?.length) prefs = ["en"];
      for (const lang of prefs) {
        if (this.ref = candidates.find(r => r.lang === lang)) break;
      }
      if (!this.ref) this.ref = candidates[0];
    }
    
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const table = this.dom.spawn(this.element, "TABLE");
    this.spawnHeader(table);
    const resStrings = this.decode(this.res.serial);
    const refStrings = this.decode(this.ref?.serial);
    const len = Math.max(resStrings.length, refStrings.length);
    for (let ix=1; ix<len; ix++) {
      this.spawnRow(table, ix, resStrings[ix] || "", refStrings[ix] || "");
    }
    this.dom.spawn(this.element, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "+", "on-click": () => this.onAdd() })
    );
  }
  
  spawnHeader(table) {
    const tr = this.dom.spawn(table, "TR");
    this.dom.spawn(tr, "TH", "Index");
    this.dom.spawn(tr, "TH", this.res.lang);
    if (this.ref) {
      const td = this.dom.spawn(tr, "TD");
      const select = this.dom.spawn(td, "SELECT", { name: "reflang", "on-change": () => this.onReflangChange() });
      for (const res of this.data.resv) {
        if (res.type !== "strings") continue;
        if (res.rid !== this.res.rid) continue;
        if (res.lang === this.res.lang) continue;
        this.dom.spawn(select, "OPTION", { value: res.lang }, res.lang);
      }
      select.value = this.ref.lang;
    }
  }
  
  spawnRow(table, ix, resString, refString) {
    const tr = this.dom.spawn(table, "TR", { "data-ix": ix });
    let result = null;
    this.dom.spawn(tr, "TD", ix);
    this.dom.spawn(tr, "TD",
      result = this.dom.spawn(null, "TEXTAREA", { name: `res:${ix}`, "on-input": () => this.onResInput() })
    );
    result.value = resString;
    if (this.ref) {
      let element;
      this.dom.spawn(tr, "TD",
        element = this.dom.spawn(null, "TEXTAREA", { name: `ref:${ix}`, "on-input": () => this.onRefInput() })
      );
      element.value = refString;
    }
    return result;
  }
  
  /* Return a contiguous array of strings for an encoded resource.
   * Includes an empty zero, so array index is the same as string index.
   * Input may be string, Uint8Array, or anything false.
   */
  decode(src) {
    const dst = [""];
    if (src) {
      if (src instanceof Uint8Array) src = new TextDecoder("utf-8").decode(src);
      for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
        let nlp = src.indexOf("\n", srcp);
        if (nlp < 0) nlp = src.length;
        const line = src.substring(srcp, nlp).trim();
        srcp = nlp + 1;
        if (!line || line.startsWith("#")) continue;
        const sepp = line.indexOf(" ");
        if (sepp < 0) throw new Error(`strings:${this.res.rid}:${lineno}: Missing separator.`);
        const ix = +line.substring(0, sepp);
        if (isNaN(ix) || (ix < 1) || (ix > 1023)) throw new Error(`strings:${this.res.rid}:${lineno}: Invalid index.`);
        const pre = line.substring(sepp).trim();
        let string;
        if (pre.startsWith("\"")) {
          try {
            string = JSON.parse(pre);
          } catch (e) {
            throw new Error(`strings:${this.res.rid}:${lineno}: Malformed JSON string.`);
          }
        } else {
          string = pre;
        }
        while (dst.length <= ix) dst.push(""); // (dst) must be contiguous.
        dst[ix] = string;
      }
    }
    return dst;
  }
  
  // (pfx) is "res:" or "ref:", what the <input [name]> start with.
  encode(pfx) {
    let dst = "";
    for (let ix=1; ; ix++) {
      const input = this.element.querySelector(`textarea[name='${pfx}${ix}']`);
      if (!input) break; // Inputs are created contiguously from 1, so the first missing input is the end.
      let v = input.value || "";
      if (!v) continue; // No need to encode empties.
      // Must use JSON if there's leading space, trailing space, newlines, or a leading quote.
      if (v.match(/^\s|\s$|^"|\n/)) {
        v = JSON.stringify(v);
      }
      dst += `${ix} ${v}\n`;
    }
    return new TextEncoder("utf-8").encode(dst);
  }
  
  onReflangChange() {
    const lang = this.element.querySelector("select[name='reflang']").value;
    this.ref = this.data.resv.find(r => ((r.type === "strings") && (r.rid === this.res.rid) && (r.lang === lang)));
    this.buildUi();
  }
  
  onResInput() {
    this.data.dirty(this.res.path, () => this.encode("res:"));
  }
  
  onRefInput() {
    if (!this.ref) return;
    this.data.dirty(this.ref.path, () => this.encode("ref:"));
  }
  
  onAdd() {
    const table = this.element.querySelector("table");
    let ix = 1;
    for (const tr of table.querySelectorAll("tr[data-ix]")) {
      const trix = +tr.getAttribute("data-ix");
      if (trix >= ix) ix = trix + 1;
    }
    const input = this.spawnRow(table, ix, "", "");
    input.select();
    input.focus();
    // Don't dirty at this point; a new string at the end doesn't actually change the resource.
  }
}
