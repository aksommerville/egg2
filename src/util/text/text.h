/* text.h
 * Client-side helpers for text processing.
 * Requires our "res" unit and nothing else.
 */
 
#ifndef TEXT_H
#define TEXT_H

/* You must initialize text with a client-side copy of the ROM.
 * Get it from egg_rom_get(), then call us, and hold it constant.
 */
void text_set_rom(const void *src,int srcc);

/* Calls (cb) for each nonzero language that has at least one strings resource.
 * Stops when you return nonzero, and returns the same.
 * Owing to the way ROMs are laid out, these will naturally be in alphabetical order (by ISO 639 code).
 * Language zero is reported like any other, if there's a strings with rid<64. Beware that those rid are hard to reach!
 */
int text_for_each_language(int (*cb)(int lang,void *userdata),void *userdata);

/* Get one member of a strings resource.
 * For (rid) 1..63, we'll query EGG_PREF_LANG and adjust accordingly.
 * Anything >=64 is the exact rid as encoded in the ROM.
 * Returns a pointer into the ROM you provided at text_set_rom().
 * Beware that the string is not nul-terminated.
 */
int text_get_string(void *dstpp,int rid,int strix);

/* Compose a string with structured insertions.
 * Your format string may contain:
 *  - "%%" => "%"
 *  - "%0".."%9" => insv[n]
 * Each insertion is an integer, literal string, or strings resource reference.
 * Insertions each have a "mode" describing how to fetch and display the content:
 *  - 'i': (ins.i) as a signed decimal integer.
 *  - 's': (ins.s.(v,c)) as a literal string. (c<0) if nul-terminated.
 *  - 'r': (ins.r.(rid,strix)) for a strings resource.
 *  - 't': (ins.t) as a time in seconds. See text_time_repr().
 * We'll stop processing at the end of the output buffer, and never return >dsta.
 */
struct text_insertion {
  char mode;
  union {
    int i;
    struct { const char *v; int c; } s;
    struct { int rid,strix; } r;
    struct { double s; int wholec,fractc; } t;
  };
};
int text_format(
  char *dst,int dsta,
  const char *fmt,int fmtc,
  const struct text_insertion *insv,int insc
);
int text_format_res(
  char *dst,int dsta,
  int rid,int strix,
  const struct text_insertion *insv,int insc
);

/* Represent signed decimal integer.
 */
int text_decsint_repr(char *dst,int dsta,int src);

/* Represent time.
 * Input is floating-point seconds.
 * Output is "H:MM:SS.FFF".
 * (fractc) in 0..3, how many fractional digits.
 * (wholec) in 1..7, how many whole digits.
 * (wholec) in -7..-1 to treat as a maximum; we'll trim leading zeroes up to -n.
 * Negative input is effectively zero.
 * Input that doesn't fit in (wholec) prints as all nines.
 */
int text_time_repr(char *dst,int dsta,double s,int wholec,int fractc);

#endif
