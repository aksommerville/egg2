/* Umenu.js
 * Universal Menu.
 * Let's use regular DOM facilities, it would be so much easier.
 * Nobody says this out-of-band stuff needs to match exactly across platforms.
 */
 
export class Umenu {
  constructor(rt) {
    this.rt = rt;
    
    this.pvmode = this.rt.input.mode;
  }
  
  // This won't get called from outside, because we've stopped input.
  stop() {
    this.rt.input.egg_input_set_mode(this.pvmode);
    const container = document.querySelector("#umenu");
    if (container) container.remove();
    this.rt.umenu = null; // Destroy self.
  }
  
  // Owner will always call after instantiating us.
  start() {
    this.rt.input.zeroAllStates();
    this.rt.input.egg_input_set_mode(0); // GAMEPAD
    const fbouter = document.querySelector("#fbouter");
    const umenu = this.spawn(fbouter, "DIV", { id: "umenu" });
    
    // Peg umenu's position to overlay the canvas.
    // TODO If the canvas moves or resizes we won't know. Does it matter?
    const canvas = this.rt.video.canvas;
    const cbounds = canvas.getBoundingClientRect();
    umenu.style.left = cbounds.x + "px";
    umenu.style.top = cbounds.y + "px";
    umenu.style.width = cbounds.width + "px";
    umenu.style.height = cbounds.height + "px";
    
    this.spawn(umenu, "DIV", ["row"],
      this.spawn(null, "DIV", ["keyIcon"], "\u266c"), // U+266c BEAMED SIXTEENTH NOTES
      this.spawn(null, "INPUT", { type: "range", min: 0, max: 99, value: this.rt.audio.musicTrim, name: "music", "on-input": e => this.onMusic(e) })
    );
    this.spawn(umenu, "DIV", ["row"],
      this.spawn(null, "DIV", ["keyIcon"], "\ud83d\udd0a"), // U+1f50a SPEAKER WITH THREE SOUND WAVES
      this.spawn(null, "INPUT", { type: "range", min: 0, max: 99, value: this.rt.audio.soundTrim, name: "sound", "on-input": e => this.onSound(e) })
    );
    
    let langSelect;
    this.spawn(umenu, "DIV", ["row"],
      this.spawn(null, "DIV", ["keyIcon"], "A\u2f42"), // U+2f42 KANGXI RADICAL SCRIPT
      langSelect = this.spawn(null, "SELECT", { name: "lang", "on-input": e => this.onLang(e) })
    );
    const langs = this.rt.rom.getMeta("lang").split(",").map(v => v.trim());
    for (const lang of langs) {
      this.spawn(langSelect, "OPTION", { value: this.rt.langEval(lang) }, lang);
    }
    langSelect.value = this.rt.lang;
    
    this.spawn(umenu, "DIV", ["row"],
      this.spawn(null, "INPUT", { type: "button", value: "\u2328", "on-click": e => this.onInput(e) }), // U+2328 KEYBOARD
      this.spawn(null, "INPUT", { type: "button", value: "\u25b6", "on-click": e => this.onResume(e) }), // U+25b6 BLACK RIGHT-POINTING TRIANGLE
      this.spawn(null, "INPUT", { type: "button", value: "\u23fb", "on-click": e => this.onQuit(e) }) // U+23fb POWER SYMBOL
    );
  }
  
  update(elapsed) {
  }
  
  /* DOM helpers.
   ************************************************************************/
   
  spawn(parent, tagName, ...args) {
    const element = document.createElement(tagName);
    for (const arg of args) {
      if (arg && (typeof(arg) === "object")) {
        if (arg instanceof Array) {
          for (const cls of arg) {
            element.classList.add(cls);
          }
        } else if (arg instanceof HTMLElement) {
          element.appendChild(arg);
        } else {
          for (const k of Object.keys(arg)) {
            if (k.startsWith("on-")) {
              element.addEventListener(k.substring(3), arg[k]);
            } else {
              element.setAttribute(k, arg[k]);
            }
          }
        }
      } else {
        element.innerText = arg;
      }
    }
    if (parent) parent.appendChild(element);
    return element;
  }
  
  /* Input events.
   ***********************************************************************/
   
  onMusic(event) {
    const v = Math.max(0, Math.min(99, +event.target.value || 0));
    this.rt.audio.setMusicTrim(v);
  }
  
  onSound(event) {
    const v = Math.max(0, Math.min(99, +event.target.value || 0));
    this.rt.audio.setSoundTrim(v);
  }
  
  onLang(event) {
    this.rt.egg_prefs_set(1, +event.target.value);
  }
  
  onInput(event) {
    console.log(`Umenu.onInput`, event);
  }
  
  onResume(event) {
    this.stop();
  }
  
  onQuit(event) {
    this.rt.egg_terminate(0);
  }
}
