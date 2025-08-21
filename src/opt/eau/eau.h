/* eau.h
 * Helpers for processing and converting EAU, EAU-Text, and MIDI files.
 * Our "synth" unit does not use this; it decodes EAU on its own.
 */
 
#ifndef EAU_H
#define EAU_H

#include <stdint.h>

struct sr_encoder;

/* High-level format detection and conversion.
 * In general, we'll log errors and warnings if a path is provided.
 * We don't convert to WAV, but you can: Convert to EAU, spawn a synthesizer, and run it.
 * (we're not involving synth, in the interest of separation of duties).
 ***************************************************************/
 
#define EAU_FORMAT_UNKNOWN 0
#define EAU_FORMAT_EAU 1
#define EAU_FORMAT_EAUT 2
#define EAU_FORMAT_MIDI 3

int eau_guess_format(const void *src,int srcc,const char *path);

/* Callback to fetch premade instruments from the SDK.
 * Return the entire CHDR chunk payload, ie first byte is (chid).
 * In general, caller will ignore (chid,trim,pan) and get those from other sources.
 * (*dstpp) should point to callee-owned memory; caller will not modify it.
 */
typedef int (*eau_get_chdr_fn)(void *dstpp,int fqpid);

/* Converts among any of our formats, including identities and intermediaries.
 * If (srcfmt) is UNKNOWN, we'll guess or fail.
 * If (dstfmt) is UNKNOWN, EAU converts to MIDI and everything else to EAU.
 * (an edge case: If both formats are unknown, we fail, we don't consider those equivalent).
 */
int eau_convert(
  struct sr_encoder *dst,int dstfmt,
  const void *src,int srcc,int srcfmt,
  const char *path,
  eau_get_chdr_fn get_chdr,
  int strip_names
);

int eau_cvt_eau_eaut(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names);
int eau_cvt_eau_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names);
int eau_cvt_eaut_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names);
int eau_cvt_midi_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names);

/* EAU files.
 **********************************************************************/

/* Read EAU file piecemeal, if you need >16 channels or you're using extension chunks or something.
 */
struct eau_file_reader {
  const void *v;
  int c,p;
};
struct eau_file_chunk {
  uint8_t id[4];
  const void *v;
  int c;
};
int eau_file_reader_next(struct eau_file_chunk *chunk,struct eau_file_reader *reader);

/* "CHDR" chunk.
 * Decoding fills in defaults if short. But not modecfg defaults, obviously.
 */
struct eau_file_channel {
  uint8_t chid;
  uint8_t trim; // 0..255
  uint8_t pan; // 0..128..255
  uint8_t mode;
  const void *modecfg;
  int modecfgc;
  const void *post;
  int postc;
};
int eau_file_channel_decode(struct eau_file_channel *channel,const void *src,int srcc);

/* Split and validate an EAU file in one call.
 * Channels >=16 are quietly ignored.
 */
struct eau_file {
  int tempo; // ms/qnote
  int loopp; // bytes into (eventv). Validated.
  struct eau_file_channel channelv[16];
  int channelc; // 0..16 indicating the highest; may be sparse.
  const uint8_t *eventv;
  int eventc;
  const uint8_t *textv;
  int textc;
};
int eau_file_decode(struct eau_file *file,const void *src,int srcc);

/* Iterate an event stream.
 * Adjacent delay events get lumped together and reported as one.
 * Zero-length delays are discarded.
 */
struct eau_event_reader {
  const void *v;
  int c,p;
};
struct eau_event {
  char type; // 'd','n','w' = delay,note,wheel
  union {
    int delay; // ms
    struct { uint8_t chid,noteid,velocity; int durms; } note; // durms is ms, expanded.
    struct { uint8_t chid; int v; } wheel; // (v) is signed: -512..0..511
  };
};
int eau_event_reader_next(struct eau_event *event,struct eau_event_reader *reader);

/* Determine the duration of an encoded song, in milliseconds.
 * We can go into extensive detail, at the limit we'll calculate the level envelope for each note.
 * These never include any estimate of tails introduced by DELAY (or any other post stage).
 * They can't, because mathematically DELAY has an infinite tail.
 * If you're printing a PCM file for single playback, VOICE_TAIL is the most appropriate, and much more expensive than the others.
 * If you want reasonable odds that it will loop correctly (assuming loopp==0), use ROUND_UP (that's the default).
 */
int eau_calculate_duration(const void *src,int srcc,int method);
#define EAU_DURATION_METHOD_DEFAULT    0 /* Currently set to ROUND_UP. */
#define EAU_DURATION_METHOD_DELAY      1 /* Sum of delays only. */
#define EAU_DURATION_METHOD_RELEASE    2 /* Longer of delays or final release time. */
#define EAU_DURATION_METHOD_VOICE_TAIL 3 /* Painstakingly calculate envelope release times for known channel modes. */
#define EAU_DURATION_METHOD_ROUND_UP   4 /* RELEASE, and then round up to the next multiple of tempo. */

/* MIDI files.
 ************************************************************************/
 
struct midi_file {
  void *keepalive;
  
  int format;
  int track_count; // From MThd; not necessarily the count of tracks.
  int division; // ticks/qnote
  
  double ph; // Current playhead in seconds.
  double spertick; // Current effective tempo.
  
  struct midi_track {
    const uint8_t *v;
    int c;
    int p;
    int delay; // ticks; <0 if we need to read it
    uint8_t status;
    uint8_t chpfx; // We're doing MIDI Channel Prefix per track. Not sure what the spec says.
  } *trackv;
  int trackc,tracka;
};

void midi_file_del(struct midi_file *file);

/* Succeeds if the file is framed correctly and has a valid MThd.
 * Misencodes are still possible.
 * We copy (src) to be on the safe side; you can free it immediately after construction.
 */
struct midi_file *midi_file_new(const void *src,int srcc);

/* Return to time zero.
 * Cheap and error-proof.
 */
void midi_file_reset(struct midi_file *file);

/* Read the next event from the file, treating delay as an event.
 * Delays are reported both in seconds and ticks. I expect seconds are far more useful.
 * Ticks are more exact, but to make sense of them you also have to track the tempo, which is allowed to change mid-song.
 * "block" events are Meta or Sysex. These may have a (chid) if Meta 0x20 MIDI Channel Prefix was used. Otherwise 0xff.
 * "cv" ("Channel Voice") events are notes, control change, etc.
 */
struct midi_event {
  int trackid; // Zero-based track index, just in case you care (but you shouldn't).
  char type; // 'd','c','b' = delay,channel-voice,block
  union {
    struct { double s; int ticks; } delay;
    struct { uint8_t opcode,chid,a,b; } cv;
    struct { uint8_t opcode,chid,type; const void *v; int c; } block;
  };
};
int midi_file_next(struct midi_event *event,struct midi_file *file);

#endif
