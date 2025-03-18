/* egg-stdlib.h
 * Selections from newlib for Egg games, or a pass-thru to the build host's stdlib.
 */
 
#ifndef EGG_STDLIB_H
#define EGG_STDLIB_H

#if USE_real_stdlib
//TODO Include the real headers.
#else

typedef void FILE;
#define stdout 0
#define stderr 0
#define stdin 0
static inline int fprintf(FILE *f,const char *fmt,...) { return 0; }
static inline void *malloc(long unsigned int c) { return 0; }

#endif
#endif
