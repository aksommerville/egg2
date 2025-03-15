/* egg-stdlib.h
 * Selections from newlib for Egg games, or a pass-thru to the build host's stdlib.
 */
 
#ifndef EGG_STDLIB_H
#define EGG_STDLIB_H

#define stdout 0
#define stderr 0
#define stdin 0
static inline int fprintf(FILE *f,const char *fmt,...) { return 0; }
static inline void *malloc(int c) { return 0; }

#endif
