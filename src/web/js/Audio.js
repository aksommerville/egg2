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
    this.sounds = []; // SongPlayer. TODO Maybe something else, once we get printing implemented.
    this.ctx = null; // AudioContext
    eauNotevRequire();
  }
  
  //TODO I think we need a softer concept of "pause", to temporarily stop output eg when the page loses focus.
  
  start() {
    if (!this.ctx) {
      this.ctx = new AudioContext({
        latencyHint: "interactive",
      });
    }
    if (this.ctx.state === "suspended") {
      this.ctx.resume();
    }
  }
  
  stop() {
    for (const sound of this.sounds) sound.stop();
    this.sounds = [];
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
    for (let i=this.sounds.length; i-->0; ) {
      const sound = this.sounds[i];
      if (!sound.update()) {
        sound.stop();
        this.sounds.splice(i, 1);
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
      this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid);
      this.song.play();
      if (playhead > 0) this.song.setPlayhead(playhead);
      this.update(); // Editor updates on a long period; ensure we get one initial priming update.
    }
  }
  
  /* Egg Platform API.
   ********************************************************************************/
   
  egg_play_sound(soundid, trim, pan) {
    if (!this.ctx) return;
    //TODO Print and cache.
    const serial = this.rt.rom.getRes(EGG_TID_sound, soundid);
    if (!serial) return;
    const song = new SongPlayer(this.ctx, serial, trim, pan, false, 0);
    song.play();
    this.sounds.push(song);
  }
  
  egg_play_song(songid, force, repeat) {
    if (!this.ctx) return;
    if (!force && (songid === this.song?.id)) return;
    const serial = this.rt.rom.getRes(EGG_TID_song, songid);
    if (!serial) songid = 0;
    if (this.song) {
      if (this.pvsong) this.pvsong.stop();
      this.pvsong = this.song;
      this.song.stopSoon();
      this.song = null;
    }
    if (!serial) return;
    this.song = new SongPlayer(this.ctx, serial, 1.0, 0.0, repeat, songid);
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
