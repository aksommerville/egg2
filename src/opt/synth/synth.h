/* synth.h 
 */
 
#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>
#include "egg/egg.h" /* We borrow its WASM_EXPORT macro. */

/* Regardless of the sample rate, we impose this sanity limit on the main buffer size.
 * It's small enough that if you're doing just a bulk print, you'll have to run it incrementally.
 * But large enough that if you're running realtime, you can pretend it doesn't exist.
 */
#define SYNTH_UPDATE_LIMIT_FRAMES 0x00010000
 
/* Global lifecycle.
 *************************************************************************/

WASM_EXPORT("synth_quit") void synth_quit();

/* (rate) in hz, 2000..200000.
 * (chanc) 1 or 2. You may declare higher values, but we'll only use the first two.
 * (buffer_frames) is the length of each output buffer in frames.
 */
WASM_EXPORT("synth_init") int synth_init(int rate,int chanc,int buffer_frames);

/* Inform synth that the ROM has length (len) bytes; synth will allocate a buffer for it and return that to you.
 * You must immediately copy the rom there, and don't touch it again.
 * It's OK to copy the entire ROM, or you can trim it to start at (tid==EGG_TID_song,rid==1).
 * Do this right after synth_init.
 * You should not normally do this while running but it's legal. Anything currently playing will be immediately dropped.
 */
WASM_EXPORT("synth_get_rom") void *synth_get_rom(int len);

/* Trivial accessors. (rate,chanc,buffer_frames) were provided by you, so you shouldn't need these.
 * synth_get_buffer() returns our output, which we will reuse at each update.
 * (chan) (0,1) = (left or mono,right).
 */
int synth_get_rate();
int synth_get_chanc();
int synth_get_buffer_size_frames();
WASM_EXPORT("synth_get_buffer") float *synth_get_buffer(int chan);

/* Playback and such.
 ***********************************************************************/

/* Generate so many frames of output, advancing internal state.
 * (framec) must be in 0..(buffer_frames).
 * After this returns, the new PCM is present on our buffers (synth_get_buffer()).
 */
WASM_EXPORT("synth_update") void synth_update(int framec);

/* Begin playing a "sound" resource.
 * Strictly fire-and-forget. You can't address sounds once running, and you'll never even know whether they play or not.
 * Sounds are EAU files, same as songs, but they're limited in length and we print each to flat PCM the first time they play.
 */
WASM_EXPORT("synth_play_sound") void synth_play_sound(int rid,float trim,float pan);

/* Begin playing a "song" resource.
 * Multiple songs may run at once.
 * Songs always synthesize on the fly, and may be arbitrarily long.
 * It is unusual to play a song at nonzero pan. Individual channels may ignore it.
 * Caller must provide a positive (songid) for later addressing. We fail quickly if you give <=0.
 * If you call this with a (songid) already in use, we gracefully stop the old one first.
 * So the single-song semantics of older synthesizers can easily be achieved: Just use 1 as songid every time.
 *
 * There's two equally valid ways to stop a song:
 *  - synth_play_song(songid,0,...)
 *  - synth_set(songid,SYNTH_PROP_EXISTENCE,0.0f)
 * I think the first reads neater.
 */
WASM_EXPORT("synth_play_song") int synth_play_song(int songid,int rid,int repeat,float trim,float pan);

WASM_EXPORT("synth_stop_all_songs") void synth_stop_all_songs();

/* Access to running songs.
 * You can adjust individual channel trims if you know their ids, eg for Yoshi's bongo track.
 * Multiple properties are rolled up into generic 'get' and 'set' functions, to keep the API size down.
 * Valid (chid) are 0..15. Anything else addresses the song as a whole.
 */
float synth_get(int songid,int chid,int prop);
WASM_EXPORT("synth_set") void synth_set(int songid,int chid,int prop,float v);
#define SYNTH_PROP_EXISTENCE 1 /* (chid) optional, readonly if present. Value 0.0 or 1.0. Set a song's existence to zero to end it. */
#define SYNTH_PROP_TEMPO     2 /* (songid) only, readonly, constant per song. */
#define SYNTH_PROP_PLAYHEAD  3 /* (songid) only, value in seconds. Setting is messy; we don't try to catch up earlier tails or anything, and LFOs will go out of whack. */
#define SYNTH_PROP_TRIM      4 /* (chid) optional, value 0..1. There's trim per channel and per song, they multiply. */
#define SYNTH_PROP_PAN       5 /* (chid) optional, value -1..0..1 = left..center..right. Channel and song pans combine by simple clamping addition. */
#define SYNTH_PROP_WHEEL     6 /* (chid) required, value -1..0..1. Arguably should be an "event" function, implemented as "prop" just to reduce API size. */

/* Inject events into a song.
 * If your events conflict with the song's, that's your own problem.
 * Events happen at the next available moment. We don't provide for precise timing.
 */
WASM_EXPORT("synth_event_note_off") void synth_event_note_off(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity);
WASM_EXPORT("synth_event_note_on") void synth_event_note_on(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity);
WASM_EXPORT("synth_event_note_once") void synth_event_note_once(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity,int durms);

#endif
