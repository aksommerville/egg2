/* eau.h
 * Generic helpers for our EAU format.
 */
 
#ifndef EAU_H
#define EAU_H

#define EAU_CHANNEL_MODE_NOOP 0
#define EAU_CHANNEL_MODE_DRUM 1
#define EAU_CHANNEL_MODE_FM 2
#define EAU_CHANNEL_MODE_SUB 3

struct eau_file {
  int tempo; // ms/qnote
  int loopp; // byte offset in (evtv)
  const uint8_t *chhdrv;
  int chhdrc;
  const uint8_t *evtv;
  int evtc;
};

/* Dirt cheap, just splits the file into two dumps: Channel headers and events.
 * Output will point into (src), no cleanup necessary.
 */
int eau_file_decode(struct eau_file *file,const void *src,int srcc);

/* Initialize a channel reader (v,c) with (p) zero, then call eau_channel_reader_next() until it returns <=0.
 * Decoding (payload) is too complicated to do here, and (post) too simple. See etc/doc/eau-format.md.
 */
struct eau_channel_reader {
  const uint8_t *v;
  int c,p;
};
struct eau_channel_entry {
  uint8_t chid; // 0..254, but only 0..15 should be useful.
  uint8_t trim; // 0..255=silent..verbatim
  uint8_t pan; // 0..128..255=left..center..right
  uint8_t mode; // EAU_CHANNEL_MODE_*
  const uint8_t *payload;
  int payloadc;
  const uint8_t *post;
  int postc;
};
int eau_channel_reader_next(struct eau_channel_entry *entry,struct eau_channel_reader *reader);

/* No structured reader for events, just pop them off one at a time.
 * Returns 0 for explicit EOF, <0 for any error, or the length consumed.
 */
struct eau_event {
  char type; // ('d','n','w')=delay,note,wheel
  int delay; // delay, or duration for note, ms
  uint8_t chid; // note, wheel
  uint8_t noteid; // note, 0..0x7f
  uint8_t velocity; // note, 0..0xf. Or 0..0xff for wheel.
};
int eau_event_decode(struct eau_event *event,const void *src,int srcc);

#endif
