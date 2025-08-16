/* Instruments.js
 * Model for the EAU-Text file containing the SDK's default instruments.
 * In general, the editor only uses EAU, not EAU-Text or MIDI. And we call the server to convert for us.
 * But standard instruments are a special case: We really want to preserve names and such.
 * TODO It would also be great if we can edit the instruments here in the editor. What would that take?
 */
 
export class Instruments {
  constructor(src) {
    console.log(`Instruments.ctor, ${src.length} bytes input`);
    if (src) {
      this.recentName = "";
      const tokenizer = new EautTokenizer(src);
      for (;;) {
        let token = tokenizer.next();
        if (!token) break;
        
        /* Functions.
         */
        switch (token) {
          case "u8": this.compileScalar(1, tokenizer); continue;
          case "u16": this.compileScalar(2, tokenizer); continue;
          case "u24": this.compileScalar(3, tokenizer); continue;
          case "u32": this.compileScalar(4, tokenizer); continue;
          case "len": this.compileLen(tokenizer); continue;
          case "name": this.compileName(tokenizer); continue;
          case "delay": this.compileDelay(tokenizer); continue;
          case "note": this.compileNote(tokenizer); continue;
          case "wheel": this.compileWheel(tokenizer); continue;
        }
        
        /* Substitutions.
         */
        const sub = Instruments.SUBSTITUTIONS[token];
        if (sub) token = sub;
        
        /* Strings.
         */
        if (token[0] === '"') {
          try {
            const text = JSON.parse(token);
            this.appendText(text);
          } catch (e) {
            throw new Error(`${tokenizer.lineno()}: Malformed JSON string: ${token}`);
          }
          continue;
        }
        
        /* Hex dumps.
         */
        try {
          this.appendHex(token);
        } catch (e) {
          throw new Error(`${tokenizer.lineno()}: Expected hex dump, found ${JSON.stringify(token)}`);
        }
      }
    }
  }
  
  compileScalar(size, tokenizer) {
    console.log(`Instruments.compileScalar(${size})`);
  }
  
  compileLen(tokenizer) {
    console.log(`Instruments.compileLen`);
  }
  
  compileName(tokenizer) {
    if (tokenizer.next() !== "(") throw new Error(`${tokenizer.lineno()}: Expected '(' after 'name'`);
    this.recentName = tokenizer.next();
    if (this.recentName[0] !== '"') throw new Error(`${tokenizer.lineno()}: Expected quoted string for 'name'`);
    this.recentName = JSON.parse(this.recentName);
    if (tokenizer.next() !== ")") throw new Error(`${tokenizer.lineno()}: Expected ')' to terminate 'name'`);
  }
  
  compileDelay(tokenizer) {
    console.log(`Instruments.compileDelay`);
  }
  
  compileNote(tokenizer) {
    console.log(`Instruments.compileNote`);
  }
  
  compileWheel(tokenizer) {
    console.log(`Instruments.compileWheel`);
  }
  
  appendText(src) {
    console.log(`Instruments.appendText: ${JSON.stringify(src)}`);//TODO
    
    if (src === "CHDR") {
      if (!this.recentName) throw new Error(`${tokenizer.lineno()}: CHDR must be preceded by name`);
      const channelName = this.recentName;
      this.recentName = "";
      if (tokenizer.next() !== "len") throw new Error(`${tokenizer.lineno()}: CHDR must begin with "len(4) {"`);
      if (tokenizer.next() !== "(") throw new Error(`${tokenizer.lineno()}: CHDR must begin with "len(4) {"`);
      if (tokenizer.next() !== "4") throw new Error(`${tokenizer.lineno()}: CHDR must begin with "len(4) {"`);
      if (tokenizer.next() !== ")") throw new Error(`${tokenizer.lineno()}: CHDR must begin with "len(4) {"`);
      if (tokenizer.next() !== "{") throw new Error(`${tokenizer.lineno()}: CHDR must begin with "len(4) {"`);
    }
  }
  
  appendHex(src) {
    console.log(`Instruments.appendHex: ${JSON.stringify(src)}`);//TODO
  }
}

Instruments.SUBSTITUTIONS = {
  MODE_NOOP: "00",
  MODE_DRUM: "01",
  MODE_FM: "02",
  MODE_HARSH: "03",
  MODE_HARM: "04",
  POST_NOOP: "00",
  POST_DELAY: "01",
  POST_WAVESHAPER: "02",
  POST_TREMOLO: "03",
  DEFAULT_ENV: "0000",
  SHAPE_SINE: "00",
  SHAPE_SQUARE: "01",
  SHAPE_SAW: "02",
  SHAPE_TRIANGLE: "03",
};

export class EautTokenizer {
  constructor(src) {
    this.src = src;
    this.srcp = 0;
  }
  
  /* Skips whitespace and comments.
   * Returns empty string at EOF.
   */
  next() {
    const isident = ch => {
      if ((ch >= 0x30) && (ch <= 0x39)) return true;
      if ((ch >= 0x41) && (ch <= 0x5a)) return true;
      if ((ch >= 0x61) && (ch <= 0x7a)) return true;
      if (ch === 0x5f) return true;
      return false;
    };
    for (;;) {
    
      // EOF?
      if (this.srcp >= this.src.length) return "";
      
      // Peek next char.
      const ch = this.src.charCodeAt(this.srcp);
      
      // Line comment?
      if (ch === 0x23) {
        const nlp = this.src.indexOf("\n", this.srcp);
        if (nlp <= this.srcp) this.srcp = this.src.length;
        else this.srcp = nlp + 1;
        continue;
      }
      
      // Whitespace?
      if (ch <= 0x20) {
        this.srcp++;
        continue;
      }
      
      // Quoted string? It's ok to choke on embedded quotes, our spec says not to do that.
      if (ch === 0x22) {
        const closep = this.src.indexOf('"', this.srcp + 1);
        if (closep < 0) throw new Error(`${this.lineno()}: Unclosed string in EAU-Text`);
        const token = this.src.substring(this.srcp, closep + 1);
        this.srcp = closep + 1;
        return token;
      }
      
      // Identifier or integer?
      if (isident(ch)) {
        const startp = this.srcp++;
        while ((this.srcp < this.src.length) && isident(this.src.charCodeAt(this.srcp))) this.srcp++;
        return this.src.substring(startp, this.srcp);
      }
      
      // Everything else is one character of punctuation.
      this.srcp++;
      return this.src[this.srcp-1];
    }
  }
  
  lineno() {
    let lineno = 1;
    for (let i=this.srcp; i-->0; ) if (this.src.charCodeAt(i) === 0x0a) lineno++;
    return lineno;
  }
}
