/* synth_pcm.h
 * PCM, Printer, and Wave.
 * Also let's dump Envelope and the little filter bits here, feels right.
 */
 
#ifndef SYNTH_PCM_H
#define SYNTH_PCM_H

/* PCM dump.
 *******************************************************************************/

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

/* Wave.
 *******************************************************************************/

#define SYNTH_WAVE_SIZE_BITS 10
#define SYNTH_WAVE_SIZE_SAMPLES (1<<SYNTH_WAVE_SIZE_BITS)
#define SYNTH_WAVE_SHIFT (32-SYNTH_WAVE_SIZE_BITS)

struct synth_wave {
  int refc;
  float v[SYNTH_WAVE_SIZE_SAMPLES];
};

void synth_wave_del(struct synth_wave *wave);
int synth_wave_ref(struct synth_wave *wave);

/* Decode an EAU wave into a new object.
 * It's possible you'll get an existing object retained from (synth).
 */
struct synth_wave *synth_wave_new(struct synth *synth,const void *src,int srcc);

/* PCM Printer.
 ****************************************************************************/
 
struct synth_printer {
  struct synth_pcm *pcm;
  struct synth *synth; // Private context that does the printing.
  int p;
};

void synth_printer_del(struct synth_printer *printer);

/* New printer for some EAU file.
 * On success, (printer->pcm) is fully allocated but all zeroes.
 */
struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc);

/* Print up to (framec) more frames of PCM.
 * Returns 0 if complete, >0 if still running, no errors.
 * It's legal to call with (framec<1) if you just want to test completion.
 */
int synth_printer_update(struct synth_printer *printer,int framec);

/* PCM Player.
 *******************************************************************************/
 
struct synth_pcmplay {
  struct synth_pcm *pcm;
  float gainl,gainr;
  int p;
  int chanc;
};

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay);

int synth_pcmplay_setup(struct synth_pcmplay *pcmplay,int chanc,struct synth_pcm *pcm,double trim,double pan);

/* Add my signal, multi-channel, to (v), up to (framec) frames.
 * Returns 0 if complete, >0 if still running, no errors.
 */
int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay);

/* Envelope config and runner.
 *******************************************************************************/

//TODO env

/* Ring buffer.
 ********************************************************************************/
 
//TODO ring

/* IIR runner.
 ********************************************************************************/
 
//TODO iir

#endif
