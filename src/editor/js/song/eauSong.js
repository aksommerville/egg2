/* eauSong.js
 * Encode and decode Song objects to/from EAU Binary.
 */
 
import { SongChannel, SongEvent } from "./Song.js";

/* Encode.
 ******************************************************************************/

export function eauSongEncode(song) {
  console.log(`TODO eauSongEncode`);
  return new Uint8Array(0); // TODO
}

/* Decode.
 *****************************************************************************/
 
export function eauSongDecode(song, src) {
  song.format = "eau";
  console.log(`TODO eauSongDecode`);//TODO
}
