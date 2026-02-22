/* rand.c
 * Using George Marsaglia's Xorshift algorithm, as described here: https://en.wikipedia.org/wiki/Xorshift
 * This file gets built even when USE_real_stdlib, it's the only such file.
 * Reason for that is I want a deterministic PRNG that builds for both native (real_stdlib) and web (egg_stdlib).
 */

#include "egg-stdlib.h"
#include "egg/egg.h"

static struct {
  unsigned int state;
} grand={0};

#if USE_real_stdlib
  #define NAME_OF_RAND egg_rand
  #define NAME_OF_SRAND egg_srand
#else
  #define NAME_OF_RAND rand
  #define NAME_OF_SRAND srand
#endif

int NAME_OF_RAND() {
  grand.state^=grand.state<<13;
  grand.state^=grand.state>>17;
  grand.state^=grand.state<<5;
  return grand.state&0x7fffffff;
}

void NAME_OF_SRAND(int seed) {
  grand.state=seed;
}

void srand_auto() {
  double now=egg_time_real();
  int seed=(int)now;
  int mixer=65521;
  for (;;) {
    int onec=0;
    unsigned int q=seed;
    for (;q;q>>=1) if (q&1) onec++;
    if ((onec>=10)&&(onec<=20)) {
      srand(seed);
      return;
    }
    seed^=mixer;
    mixer+=65521;
  }
}

uint32_t get_rand_seed() {
  return grand.state;
}
