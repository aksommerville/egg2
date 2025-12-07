/* eau.h
 * Convert between EAU and MIDI.
 */
 
#ifndef EAU_H
#define EAU_H

struct sr_encoder;
struct sr_convert_context;

/* Give us a Channel Header for the given fqpid ((BankMsb<<14)|(BankLsb<<7)|Program).
 * This must be at least 8 bytes, beginning with (chid).
 * The first three bytes (chid,trim,pan), we replace according to the song.
 * You must hold it constant. We won't modify or free it.
 * If you return empty or <0, we'll use an undesirable default instead.
 * We will not do any fancy defaulting; basically, any fqpid we ask for was explicitly called out by the song.
 * We do a little default: Channels with no Program Change at all, we'll try fqpid zero.
 */
typedef int (*eau_get_chdr_fn)(void *dstpp,int fqpid);

// Ugly hack to supply the instruments, because sr_convert_context doesn't have a place for arbitrary pointers.
extern eau_get_chdr_fn eau_get_chdr;

/* Null if valid, or a canned failure message.
 * Validates framing aggressively.
 * But we don't guarantee that it's completely valid, eg we don't visit modecfg or post.
 */
const char *eau_validate(const void *src,int srcc);

int eau_cvt_eau_midi(struct sr_convert_context *ctx);
int eau_cvt_midi_eau(struct sr_convert_context *ctx);

/* Helpers for EAU file dissection.
 ******************************************************************/
 
struct eau_file {
  int tempo; // ms/qnote
  const void *chdr; int chdrc;
  const void *evts; int evtsc;
  const void *text; int textc;
};

int eau_file_decode(struct eau_file *file,const void *src,int srcc);

struct eau_chdr_reader {
  const void *v;
  int p,c;
};
struct eau_chdr_entry {
  unsigned char chid,trim,pan,mode;
  const void *modecfg; int modecfgc;
  const void *post; int postc;
  const void *v; int c; // The entire encoded chdr, starting at (chid).
};
int eau_chdr_reader_next(struct eau_chdr_entry *entry,struct eau_chdr_reader *reader);

//TODO event and text reader, are we going to need them?

#endif
