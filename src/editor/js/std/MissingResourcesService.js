/* MissingResourcesService.js
 * Scan the resource set and try to identify anything missing.
 * Actually, we'll do a smorgasbord of resource-level validation, why not.
 */
 
import { Data } from "../Data.js";
import { Dom } from "../Dom.js";
import { SharedSymbols } from "../SharedSymbols.js";
import { decodeStrings } from "./StringsEditor.js";

export class MissingResourcesService {
  static getDependencies() {
    return [Data, Dom, SharedSymbols];
  }
  constructor(data, dom, sharedSymbols) {
    this.data = data;
    this.dom = dom;
    this.sharedSymbols = sharedSymbols;
  }
  
  detectAndReport() {
    console.log(`MissingResourcesService.detectAndReport`, { resv: this.data.resv });
    const warnings = []; // { path, note }
    let metadataReport = null; // See validateMetadata().
    const strings = []; // res
    const imageRids = new Set();
    for (const res of this.data.resv) {
      
      /* Zero-length resources trigger a warning immediately.
       * They disappear during compilation, so there is no good reason for one to exist.
       * I do this a lot with placeholder sound effects. In that case, a warning is super helpful.
       */
      if (!res.serial?.length) {
        warnings.push({ path: res.path, note: "empty" });
        continue;
      }
      
      switch (res.type) {
      
        // Some types, we know there's nothing to validate.
        case "sound":
        case "song":
          continue;
          
        // image, capture all the rids so we can validate tilesheet and decalsheet.
        case "image": imageRids.add(res.rid); break;
        
        /* metadata:1 goes thru a validator and also we note its existence.
         * Any other rid is illegal.
         */
        case "metadata": {
            if (res.rid === 1) {
              metadataReport = this.validateMetadata(warnings, res);
            } else {
              warnings.push({ path: res.path, note: "metadata can only use rid 1" });
            }
          } break;
        
        // code is just not allowed.
        case "code": warnings.push({ path: res.path, note: "code does not belong among data" }); break;
        
        // strings, we will check them against each other and also against metadata. Store for now.
        case "strings": strings.push(res); break;
        
        /* tilesheet and decalsheet require an image of the same rid.
         * EGG_TID_image is less than both tilesheet and decalsheet, so we'll have the full set already.
         */
        case "tilesheet":
        case "decalsheet": {
            if (!imageRids.has(res.rid)) {
              warnings.push({ path: res.path, note: `image:${res.rid} does not exist` });
            }
          } break;
        
        // map and sprite are both command lists, we can validate them generically.
        case "map":
        case "sprite": this.validateCommandList(warnings, res); break;
        
        /* Unknown types, if sharedSymbols indicates it's a command list, do that.
         */
        default: {
            if (this.sharedSymbols.typeNameIsCommandList(res.type)) {
              this.validateCommandList(warnings, res);
            }
          }
      }
    }
    this.validateStrings(warnings, strings, metadataReport);
    
    // Post validation for metadata.
    if (!metadataReport) {
      warnings.push({ path: "", note: "metadata:1 is required" });
    } else {
      if (!metadataReport.iconImage) {
        warnings.push({ path: metadataReport.res.path, note: "Recommend including iconImage." });
      } else if (!imageRids.has(metadataReport.iconImage)) {
        warnings.push({ path: metadataReport.res.path, note: `Missing image:${metadataReport.iconImage} (iconImage)` });
      }
      if (metadataReport.postImage && !imageRids.has(metadataReport.postImage)) {
        warnings.push({ path: metadataReport.res.path, note: `Missing image:${metadataReport.postImage} (posterImage)` });
      }
      for (const k of metadataReport.missingDefaultKey) {
        warnings.push({ path: metadataReport.res.path, note: `No default for key ${JSON.stringify(k)}` });
      }
    }
    console.log({ warnings, metadataReport, imageRids });
  }
  
  validateMetadata(warnings, res) {
    const report = {
      res,
      lang: [],
      iconImage: 0,
      posterImage: 0,
      strings: [], // index in strings:1
      missingDefaultKey: [],
    };
    const nakedKeys = [];
    const dollarKeys = [];
    const textDecoder = new TextDecoder("utf8");
    for (let srcp=0; srcp<res.serial.length; ) {
      const startp = srcp;
      while ((srcp < res.serial.length) && (res.serial[srcp++] !== 0x0a)) ;
      const line = textDecoder.decode(new Uint8Array(res.serial.buffer, res.serial.byteOffset + startp, srcp - startp)).trim();
      if (!line || line.startsWith("#")) continue;
      const eqp = line.indexOf("=");
      if (eqp < 0) continue;
      const k = line.substring(0, eqp).trim();
      const v = line.substring(eqp + 1).trim();
      switch (k) {
        case "lang": report.lang = v.split(",").map(v => v.trim()); break;
        case "iconImage": report.iconImage = +v; break;
        case "posterImage": report.posterImage = +v; break;
      }
      if (k.endsWith("$")) {
        dollarKeys.push(k.substring(0, k.length - 1));
        report.strings.push(+v);
      } else {
        nakedKeys.push(k);
      }
    }
    for (const k of dollarKeys) {
      if (nakedKeys.indexOf(k) < 0) {
        report.missingDefaultKey.push(k);
      }
    }
    return report;
  }
  
  validateStrings(warnings, strings, metadataReport) {
    
    // The declared languages in metadata:1 should exactly match the languages of the string resources.
    const expectLangs = [...metadataReport.lang].sort();
    const actualLangs = Array.from(new Set(strings.map(res => res.lang))).sort();
    for (let ai=0, bi=0; (ai<expectLangs.length) || (bi<actualLangs.length); ) {
      if (ai >= expectLangs.length) {
        warnings.push({ path: metadataReport.res.path, note: `Language ${JSON.stringify(actualLangs[bi])} exists but is not declared.` });
        bi++;
      } else if (bi >= actualLangs.length) {
        warnings.push({ path: metadataReport.res.path, note: `Language ${JSON.stringify(expectLangs[ai])} is declared but no strings exist.` });
        ai++;
      } else {
        if (expectLangs[ai] < actualLangs[bi]) {
          warnings.push({ path: metadataReport.res.path, note: `Language ${JSON.stringify(expectLangs[ai])} is declared but no strings exist.` });
          ai++;
        } else if (expectLangs[ai] > actualLangs[bi]) {
          warnings.push({ path: metadataReport.res.path, note: `Language ${JSON.stringify(actualLangs[bi])} exists but is not declared.` });
          bi++;
        } else {
          ai++;
          bi++;
        }
      }
    }
    
    /* Each rid must exist for each actualLang. (NB res.rid is unqualified)
     * Then among the stringses for each rid, each index that appears at least once must appear everywhere.
     */
    const rids = Array.from(new Set(strings.map(res => res.rid)));
    for (const rid of rids) {
      const stringsForRid = strings.filter(res => (res.rid === rid));
      if (stringsForRid.length < actualLangs.length) {
        let note = `strings:${rid} missing for language:`;
        for (const lang of actualLangs) {
          if (!stringsForRid.find(res => (res.lang === lang))) {
            note += " " + lang;
          }
        }
        warnings.push({ path: "", note });
      }
      this.validateStringsContent(warnings, stringsForRid, metadataReport);
    }
    
    /* If we didn't get any strings:1 but metadata:1 had some dollar keys, that's a warning.
     * If any strings:1 did exist, validateStringsContent has already taken care of it.
     */
    if ((rids.indexOf(1) < 0) && (metadataReport.strings.length > 0)) {
      warnings.push({ path: metadataReport.res.path, note: `strings:1 doesn't exist but metadata:1 contains dollar keys` });
    }
    
    const modal = this.dom.spawnModal(MissingResourcesModal);
    modal.setup(warnings);
  }
  
  /* Given an array of strings resources for one rid (the whole res entry),
   * generate a warning for any index that appears at least once but not everywhere.
   */
  validateStringsContent(warnings, strings, metadataReport) {
    const content = strings.map(res => {
      try {
        return decodeStrings(res.serial);
      } catch (e) {
        warnings.push({ path: res.path, note: e.message });
        return [];
      }
    });
    const c = Math.max(...content.map(c => c.length));
    for (let i=1; i<c; i++) {
      const values = content.map(con => con[i] || "");
      if (!values.find(v => v)) continue; // All empty, no problemo.
      for (let ri=0; ri<content.length; ri++) {
        if (values[ri]) continue; // Not empty, cool.
        warnings.push({ path: strings[ri].path, note: `Missing index ${i}` });
      }
    }
    
    // If it's rid one, validate metadata's dollar keys.
    // We'll check against the first resource only; we've already warned if the sets don't match.
    if (metadataReport && (strings[0].rid === 1)) {
      for (const ix of metadataReport.strings) {
        if (!content[0][ix]) {
          warnings.push({ path: metadataReport.res.path, note: `Some field refers to strings:1:${ix} but that string doesn't exist.` });
        }
      }
    }
  }
  
  validateCommandList(warnings, res) {
    const src = new TextDecoder("utf-8").decode(res.serial);
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      const tokens = line.split(/\s+/g);
      // Maps, the picture will always be single-token lines. Cool! We skip them naturally.
      for (let i=1; i<tokens.length; i++) {
        const token = tokens[i];
        // We're only interested in tokens like "image:myImage", ie starting with a type.
        const match = token.match(/^([0-9a-zA-Z_]+):([0-9a-zA-Z_]+)$/);
        if (!match) continue;
        const type = match[1];
        const name = match[2];
        const rid = +name;
        let remote = null;
        if (isNaN(rid)) {
          remote = this.data.resv.find(r => ((r.type === type) && (r.name === name)));
        } else {
          remote = this.data.resv.find(r => ((r.type === type) && (r.rid === rid)));
        }
        if (!remote) {
          warnings.push({ path: res.path, note: `${lineno}: ${token} not found` });
        }
      }
    }
  }
}

MissingResourcesService.singleton = true;

class MissingResourcesModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) {
    this.element = element;
    this.dom = dom;
  }
  
  setup(warnings) {
    this.element.innerHTML = "";
    if (warnings?.length) {
      const ul = this.dom.spawn(this.element, "UL");
      for (const warning of warnings) {
        if (warning.path) {
          this.dom.spawn(this.element, "LI", `${warning.path}: ${warning.note}`);
        } else {
          this.dom.spawn(this.element, "LI", warning.note);
        }
      }
    } else {
      this.dom.spawn(this.element, "DIV", "Resource set looks good.");
    }
  }
}
