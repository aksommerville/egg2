/* synth_pcm.h
 * PCM, Printer, and Wave.
 * Also let's dump Envelope and the little filter bits here, feels right.
 */
 
#ifndef SYNTH_PCM_H
#define SYNTH_PCM_H

/* PCM dump.
 */

// Sanity limit in samples, well below the overflow point and well above any sane use case.
#define SYNTH_PCM_LENGTH_LIMIT 0x10000000
 
struct synth_pcm {
  int refc;
  int c;
  float v[];
};

void synth_pcm_del(struct synth_pcm *pcm);
int synth_pcm_ref(struct synth_pcm *pcm);
struct synth_pcm *synth_pcm_new(int c);

//TODO printer
//TODO wave
//TODO env
//TODO ring
//TODO iir

#endif
