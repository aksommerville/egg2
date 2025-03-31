/* CommandList.js
 * Data model for a command-list resource.
 * Just line-oriented text where the first whitespace-delimited token on each line is a keyword.
 */
 
export class CommandList {
  constructor(src) {
    if (!src) this._init();
    else if (src instanceof CommandList) this._copy(src);
    else if (typeof(src) === "string") this._decode(src);
    else if (src instanceof Uint8Array) this._decode(new TextDecoder("utf8").decode(src));
    else throw new Error("Inappropriate input to CommandList");
  }
  
  _init() {
    this.commands = []; // Each is an array of string.
  }
  
  _copy(src) {
    this.commands = src.commands.map(c => [...c]);
  }
  
  // From string.
  _decode(src) {
    this.commands = [];
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      const tokens = line.split(/\s+/g);
      this.commands.push(tokens);
    }
  }
  
  // To Uint8Array
  encode() {
    return new TextEncoder("utf8").encode(this.encodeText());
  }
  
  // To string
  encodeText() {
    let dst = "";
    for (const command of this.commands) {
      dst += command.join(" ") + "\n";
    }
    return dst;
  }
  
  /* Replace an existing command with a space-delimited string.
   * If it winds up identical after sanitization, do nothing and return false.
   * We do allow commands to become empty. Beware that empties won't survive a save and reload.
   * If (p) OOB, do nothing and return false. Existing commands only.
   */
  replaceCommand(p, src) {
    if ((p < 0) || (p >= this.commands.length)) return false;
    if (!src) src = "";
    const tokens = src.split(/\s+/g).filter(v => v);
    const prev = this.commands[p];
    if (tokens.length !== prev.length) {
      this.commands[p] = tokens;
      return true;
    }
    for (let i=tokens.length; i-->0; ) {
      if (tokens[i] !== prev[i]) {
        this.commands[p] = tokens;
        return true;
      }
    }
    return false;
  }
  
  /* Get the remainder of a command whose first token is (kw).
   * If there's more than one, returns the first.
   * If it has more than 2 tokens, they get joined with space.
   */
  getFirstArg(kw) {
    const cmd = this.commands.find(c => c[0] === kw);
    if (!cmd) return "";
    if (cmd.length > 2) return cmd.slice(1).join(" ");
    return cmd[1];
  }
}
