/* Audio.js
 * Synthesizer, and everything that entails.
 */
 
export class Audio {
  constructor(rt) {
    this.rt = rt;
  }
  
  /* Egg Platform API.
   ********************************************************************************/
   
  egg_play_sound(soundid, trim, pan) {
    console.log(`TODO Audio.egg_play_sound ${soundid},${trim},${pan}`);
  }
  
  egg_play_song(songid, force, repeat) {
    console.log(`TODO Audio.egg_play_song ${songid},${force},${repeat}`);
  }
  
  egg_song_get_id() {
    console.log(`TODO Audio.egg_song_get_id`);
    return 0;
  }
  
  egg_song_get_playhead() {
    console.log(`TODO Audio.egg_song_get_playhead`);
    return 0.0;
  }
  
  egg_song_set_playhead(ph) {
    console.log(`TODO Audio.egg_song_set_playhead ${ph}`);
  }
}
