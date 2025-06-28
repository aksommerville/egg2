#include "synth_internal.h"

/* PCM object.
 */

void synth_pcm_del(struct synth_pcm *pcm) {
  if (!pcm) return;
  if (pcm->refc-->1) return;
  free(pcm);
}

int synth_pcm_ref(struct synth_pcm *pcm) {
  if (!pcm) return -1;
  if (pcm->refc<1) return -1;
  if (pcm->refc>=INT_MAX) return -1;
  pcm->refc++;
  return 0;
}

struct synth_pcm *synth_pcm_new(int c) {
  if (c<0) return 0;
  if (c>SYNTH_PCM_LIMIT_SAMPLES) return 0;
  struct synth_pcm *pcm=calloc(1,sizeof(struct synth_pcm)+sizeof(float)*c);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}

/* Wave object.
 */

void synth_wave_del(struct synth_wave *wave) {
  if (!wave) return;
  if (wave->refc-->1) return;
  free(wave);
}

int synth_wave_ref(struct synth_wave *wave) {
  if (!wave) return -1;
  if (wave->refc<1) return -1;
  if (wave->refc>=INT_MAX) return -1;
  wave->refc++;
  return 0;
}

struct synth_wave *synth_wave_new() {
  struct synth_wave *wave=calloc(1,sizeof(struct synth_wave));
  if (!wave) return 0;
  wave->refc=1;
  return wave;
}

/* Calculate a simple ramp.
 */
 
static void synth_wave_ramp(float *v,int c,float a,float z) {
  if (c<1) return;
  float d=(z-a)/c;
  for (;c-->0;v++,a+=d) *v=a;
}

/* New wave with primitive shapes.
 */
 
struct synth_wave *synth_wave_new_square() {
  struct synth_wave *wave=synth_wave_new();
  if (!wave) return 0;
  int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  int p=0;
  for (;p<halflen;p++) wave->v[p]=1.0f;
  for (;p<SYNTH_WAVE_SIZE_SAMPLES;p++) wave->v[p]=-1.0f;
  return wave;
}

struct synth_wave *synth_wave_new_saw() {
  struct synth_wave *wave=synth_wave_new();
  if (!wave) return 0;
  synth_wave_ramp(wave->v,SYNTH_WAVE_SIZE_SAMPLES,1.0f,-1.0f);
  return wave;
}

struct synth_wave *synth_wave_new_triangle() {
  struct synth_wave *wave=synth_wave_new();
  if (!wave) return 0;
  int halflen=SYNTH_WAVE_SIZE_SAMPLES>>1;
  // We kick the triangle out of phase to make it two neat ramps. Proper phase would start at zero and proceed positive.
  synth_wave_ramp(wave->v,halflen,-1.0f,1.0f);
  synth_wave_ramp(wave->v+halflen,SYNTH_WAVE_SIZE_SAMPLES-halflen,1.0f,-1.0f);
  return wave;
}

/* New wave from harmonics.
 */
 
static void synth_wave_add_harmonic(float *dst,const float *src,int step,float level) {
  if (step<1) return;
  if (step>=SYNTH_WAVE_SIZE_SAMPLES) return;
  int i=SYNTH_WAVE_SIZE_SAMPLES,srcp=0;
  for (;i-->0;srcp+=step,dst++) {
    if (srcp>=SYNTH_WAVE_SIZE_SAMPLES) srcp-=SYNTH_WAVE_SIZE_SAMPLES;
    (*dst)+=src[srcp]*level;
  }
}
 
struct synth_wave *synth_wave_new_harmonics(const float *ref,const float *v,int c) {
  struct synth_wave *wave=synth_wave_new();
  if (!wave) return 0;
  int i=0; for (;i<c;i++,v++) {
    if (*v<=0.0f) continue;
    synth_wave_add_harmonic(wave->v,ref,i+1,*v);
  }
  return wave;
}

/* Printer object.
 */

void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_song_del(printer->song);
  synth_pcm_del(printer->pcm);
  free(printer);
}

struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc) {
  if (!synth) return 0;
  struct synth_printer *printer=calloc(1,sizeof(struct synth_printer));
  if (!printer) return 0;
  if (!(printer->song=synth_song_new(synth,src,srcc,0,1))) {
    synth_printer_del(printer);
    return 0;
  }
  if (!(printer->pcm=synth_pcm_new(synth_song_measure_frames(printer->song)))) {
    synth_printer_del(printer);
    return 0;
  }
  return printer;
}

int synth_printer_update(struct synth_printer *printer,int c) {
  int updc=printer->pcm->c-printer->p;
  if (updc>c) updc=c;
  if (updc>0) {
    synth_song_update(printer->pcm->v+printer->p,updc,printer->song);
    printer->p+=updc;
  }
  return (printer->p>=printer->pcm->c)?0:1;
}

/* PCM player.
 */

void synth_pcmplay_cleanup(struct synth_pcmplay *pcmplay) {
  synth_pcm_del(pcmplay->pcm);
  pcmplay->pcm=0;
}

int synth_pcmplay_init(struct synth_pcmplay *pcmplay,struct synth_pcm *pcm,int chanc,float trim,float pan) {
  if (synth_pcm_ref(pcm)<0) return -1;
  pcmplay->pcm=pcm;
  pcmplay->chanc=chanc;
  pcmplay->p=0;
  if (chanc==1) pcmplay->triml=pcmplay->trimr=trim;
  else synth_apply_pan(&pcmplay->triml,&pcmplay->trimr,trim,pan);
  return 0;
}

int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay) {
  int updc=pcmplay->pcm->c-pcmplay->p;
  if (updc>framec) updc=framec;
  int i=updc;
  const float *src=pcmplay->pcm->v+pcmplay->p;
  pcmplay->p+=updc;
  if (pcmplay->chanc>1) {
    for (;i-->0;v+=pcmplay->chanc,src++) {
      v[0]+=(*src)*pcmplay->triml;
      v[1]+=(*src)*pcmplay->trimr;
    }
  } else {
    for (;i-->0;v++,src++) {
      (*v)+=(*src)*pcmplay->triml;
    }
  }
  return (pcmplay->p>=pcmplay->pcm->c)?0:1;
}
