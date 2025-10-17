/* Audio.js
 * Synthesizer, and everything that entails.
 * We own the AudioContext, we implement the Egg Platform API, and we dispatch out to Songs.
 * The more interesting parts belong to Song and SongChannel.
 */
 
import { EGG_TID_song, EGG_TID_sound } from "./Rom.js";
import { SongPlayer } from "./SongPlayer.js";
import { eauNotevRequire } from "./songBits.js";
 
export class Audio {
  constructor(rt) {
    this.rt = rt; // Will be undefined when loaded in editor.
    this.song = null; // SongPlayer, target for songid and playhead.
    this.pvsong = null; // SongPlayer, winding down.
    this.sounds = []; // Sparse AudioBuffer, indexed by soundid.
    this.soundPlayers = []; // { node, endTime }
    this.ctx = null; // AudioContext
    this.musicEnabled = true;
    this.soundEnabled = true;
    this.noise = null; // AudioBuffer
    eauNotevRequire();
  }
  
  pause() {
    if (!this.ctx) return;
    this.ctx.suspend();
  }
  
  resume() {
    if (!this.ctx) return;
    this.ctx.resume();
  }
  
  start() {
    if (!this.ctx) {
      this.ctx = new AudioContext({
        latencyHint: "interactive",
      });
      this.requireNoise();
    }
    if (this.ctx.state === "suspended") {
      this.ctx.resume();
    }
  }
  
  stop() {
    for (const sound of this.soundPlayers) {
      sound.node?.stop();
      sound.node.disconnect();
    }
    this.soundPlayers = [];
    if (this.song) {
      this.song.stop();
      this.song = null;
    }
    if (this.pvsong) {
      this.pvsong.stop();
      this.pvsong = null;
    }
    if (this.ctx) {
      this.ctx.suspend();
    }
  }
  
  update() {
    if (!this.ctx) return;
    for (let i=this.soundPlayers.length; i-->0; ) {
      const sp = this.soundPlayers[i];
      if (this.ctx.currentTime > sp.endTime) {
        sp.node.stop?.();
        sp.node.disconnect();
        this.soundPlayers.splice(i, 1);
      }
    }
    if (this.song && !this.song.update()) this.song = null;
    if (this.pvsong && !this.pvsong.update()) this.pvsong = null;
  }
  
  /* For editor.
   * If (songid) nonzero and matches the current song, we'll start at the prior playhead.
   */
  playEauSong(serial, songid, repeat) {
    let playhead = 0;
    if (this.song) {
      if (songid && (songid === this.song.id)) playhead = this.song.getPlayhead();
      this.song.stop();
      this.song = null;
    }
    if (serial) {
      if (!this.ctx) this.start();
      this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid, this.noise);
      this.song.play();
      if (playhead > 0) this.song.setPlayhead(playhead);
      this.update(); // Editor updates on a long period; ensure we get one initial priming update.
    }
  }
  
  playSoundBuffer(buffer, trim, pan) {
    if (!this.ctx) return;
    if (this.ctx.state === "suspended") return;
    if (trim <= 0) return;
    const node = new AudioBufferSourceNode(this.ctx, { buffer });
    let tail = node;
    if (trim < 1) {
      const gain = new GainNode(this.ctx, { gain: trim });
      tail.connect(gain);
      tail = gain;
    }
    if (pan) {
      const panner = new StereoPannerNode(this.ctx, { pan });
      tail.connect(panner);
      tail = panner;
    }
    tail.connect(this.ctx.destination);
    const endTime = this.ctx.currentTime + buffer.duration + 0.010;
    this.soundPlayers.push({ node, endTime });
    if (tail !== node) this.soundPlayers.push({ node: tail, endTime });
    node.start();
  }
  
  requireNoise() {
    if (!this.noise) {
      if (!this.ctx) throw new Error(`Requesting noise without an AudioContext`);
      // Generate a noise buffer the same way the native implementation does.
      this.noise = new AudioBuffer({
        length: this.ctx.sampleRate,
        sampleRate: this.ctx.sampleRate,
      });
      const v = this.noise.getChannelData(0);
      let state = 0x12345678;
      for (let i=0; i<v.length; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        v[i] = state / 2147483648.0 - 1.0;
      }
    }
    return this.noise;
  }
  
  /* Egg Platform API.
   ********************************************************************************/
   
  enableMusic(enable) {
    if (enable) {
      if (this.musicEnabled) return;
      this.musicEnabled = true;
      if (this.songParams) {
        const ph = this.songParams[3];
        this.egg_play_song(this.songParams[0], this.songParams[1], this.songParams[2]);
        if (this.song) this.song.setPlayhead(ph);
      }
    } else {
      if (!this.musicEnabled) return;
      this.musicEnabled = false;
      if (this.song) {
        if (this.songParams) this.songParams[3] = this.song.getPlayhead();
        if (this.pvsong) this.pvsong.stop();
        this.pvsong = this.song;
        this.song.stopSoon();
        this.song = null;
      }
    }
  }
  
  enableSound(enable) {
    if (enable) {
      if (this.soundEnabled) return;
      this.soundEnabled = true;
    } else {
      if (!this.soundEnabled) return;
      this.soundEnabled = false;
      for (const sound of this.soundPlayers) {
        sound.node.stop?.();
        sound.node.disconnect();
      }
      this.soundPlayers = [];
    }
  }
   
  egg_play_sound(soundid, trim, pan) {
    if (!this.ctx) return;
    if (!this.soundEnabled) return;
    
    const buffer = this.sounds[soundid];
    if (buffer) {
      if (buffer === "pending") return; // Drop it; the printer will also play it, once ready.
      return this.playSoundBuffer(buffer, trim, pan);
    }
    this.sounds[soundid] = "pending";
    
    const serial = this.rt.rom.getRes(EGG_TID_sound, soundid);
    if (!serial) return;
    const durms = Math.min(5000, Math.max(1, SongPlayer.calculateDuration(serial)));
    const framec = Math.ceil((durms * this.ctx.sampleRate) / 1000);
    const ctx = new OfflineAudioContext(1, framec, this.ctx.sampleRate);
    const song = new SongPlayer(ctx, serial, 1.0, 0.0, false, 0, this.noise);
    song.play();
    song.update(5.0);
    ctx.startRendering().then(buffer => {
      this.sounds[soundid] = buffer;
      this.playSoundBuffer(buffer, trim, pan);
    });
  }
  
  egg_play_song(songid, force, repeat) {
    if (!this.ctx) return;
    if (!this.musicEnabled) return;
    if (!force && (songid === this.song?.id)) return;
    this.songParams = [songid, force, repeat, 0]; // To restore when prefs change.
    const serial = this.rt.rom.getRes(EGG_TID_song, songid);
    if (!serial) songid = 0;
    if (this.song) {
      if (this.pvsong) this.pvsong.stop();
      this.pvsong = this.song;
      this.song.stopSoon();
      this.song = null;
    }
    if (!serial) return;
    this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid, this.noise);
    this.song.play();
  }
  
  egg_play_note(chid, noteid, velocity, durms) {
    if (!this.song) return 0;
    return this.song.playNote(chid, noteid, velocity, durms);
  }
  
  egg_release_note(holdid) {
    if (!this.song) return;
    this.song.releaseNote(holdid);
  }
  
  egg_adjust_wheel(chid, v) {
    if (!this.song) return;
    this.song.adjustWheel(chid, v);
  }
  
  egg_song_get_id() {
    return this.song?.id || 0;
  }
  
  egg_song_get_playhead() {
    if (this.song) return this.song.getPlayhead();
    return 0.0;
  }
  
  egg_song_set_playhead(ph) {
    if (this.song) this.song.setPlayhead(ph);
  }
}

Audio.singleton = true; // Not required by Egg Runtime, but necessary for the editor.
