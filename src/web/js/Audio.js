/* Audio.js
 * Synthesizer, and everything that entails.
 * We own the AudioContext, we implement the Egg Platform API, and we dispatch out to Songs.
 * The more interesting parts belong to Song and SongChannel.
 */
 
import { EGG_TID_song, EGG_TID_sound } from "./Rom.js";
import { Song } from "./Song.js";
import { requireFrequencies } from "./songBits.js";
 
export class Audio {
  constructor(rt) {
    console.log(`Audio.constructor`);
    this.rt = rt;
    this.songid = 0; // 0 if stopped or invalid
    this.song = null;
    this.sounds = [];
    this.ctx = null; // AudioContext
    requireFrequencies();
  }
  
  //TODO I think we need a softer concept of "pause", to temporarily stop output eg when the page loses focus.
  
  start() {
    console.log(`Audio.start`);
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
    console.log(`Audio.stop`);
    for (const sound of this.sounds) sound.stop();
    this.sounds = [];
    if (this.song) {
      this.song.stop();
      this.song = null;
      this.songid = 0;
    }
    if (this.ctx) {
      this.ctx.suspend();
    }
  }
  
  update() {
    for (let i=this.sounds.length; i-->0; ) {
      if (!sound.update()) {
        sound.stop();
        this.sounds.splice(i, 1);
      }
    }
    if (this.song) {
      if (!this.song.update()) {
        this.song = null;
        this.songid = 0;
      }
    }
  }
  
  /* Egg Platform API.
   ********************************************************************************/
   
  egg_play_sound(soundid, trim, pan) {
    console.log(`Audio.egg_play_sound ${soundid}`);
    if (!this.ctx) return;
    //TODO Print and cache.
    const serial = this.rt.rom.getRes(EGG_TID_sound, soundid);
    if (!serial) return;
    const song = new Song(serial, trim, pan, false);
    song.play(this.ctx);
    this.sounds.push(song);
  }
  
  egg_play_song(songid, force, repeat) {
    console.log(`Audio.egg_play_song ${songid}`);
    if (!this.ctx) return;
    if (!force && (songid === this.songid)) return;
    const serial = this.rt.rom.getRes(EGG_TID_song, songid);
    if (!serial) songid = 0;
    if (this.song) {
      this.song.stop();
      this.song = null;
    }
    this.songid = songid;
    if (!serial) return;
    this.song = new Song(serial, 1.0, 0.0, repeat);
    this.song.play(this.ctx);
  }
  
  egg_song_get_id() {
    return this.songid;
  }
  
  egg_song_get_playhead() {
    if (this.song) return this.song.getPlayhead();
    return 0.0;
  }
  
  egg_song_set_playhead(ph) {
    if (this.song) this.song.setPlayhead(ph);
  }
}
