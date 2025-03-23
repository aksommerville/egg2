/* synth_pcm.c
 * synth_pcm, synth_pcmplay, and synth_printer are all pretty simple, so I'm lumping them together here.
 * Nothing particularly interesting happens here.
 */

#include "synth_internal.h"

/* Plain PCM dump.
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
  if (c<1) return 0;
  if (c>SYNTH_PCM_LENGTH_LIMIT) return 0;
  struct synth_pcm *pcm=calloc(1,sizeof(struct synth_pcm)+sizeof(float)*c);
  if (!pcm) return 0;
  pcm->refc=1;
  pcm->c=c;
  return pcm;
}

/* PCM printer.
 */
 
void synth_printer_del(struct synth_printer *printer) {
  if (!printer) return;
  synth_pcm_del(printer->pcm);
  synth_del(printer->synth);
  free(printer);
}

struct synth_printer *synth_printer_new(struct synth *synth,const void *src,int srcc) {
  if (!synth||!src) return 0;
  int durms=eau_estimate_duration(src,srcc);
  if (durms<0) return 0;
  int framec=(int)(((double)durms*(double)synth->rate)/1000.0);
  if (framec<1) framec=1;
  struct synth_printer *printer=calloc(1,sizeof(struct synth_printer));
  if (!printer) return 0;
  if (
    !(printer->pcm=synth_pcm_new(framec))||
    !(printer->synth=synth_new(synth->rate,1))||
    (synth_load_song(printer->synth,1,src,srcc)<0)
  ) {
    synth_printer_del(printer);
    return 0;
  }
  synth_play_song(printer->synth,1,0,0);
  return printer;
}

int synth_printer_update(struct synth_printer *printer,int framec) {
  if (!printer||!printer->pcm) return 0;
  int updc=printer->pcm->c-printer->p;
  if (updc>framec) updc=framec;
  if (updc>0) {
    synth_updatef(printer->pcm->v+printer->p,updc,printer->synth);
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

int synth_pcmplay_setup(struct synth_pcmplay *pcmplay,int chanc,struct synth_pcm *pcm,double trim,double pan) {
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return -1;
  if (synth_pcm_ref(pcm)<0) return -1;
  pcmplay->pcm=pcm;
  pcmplay->chanc=chanc;
  pcmplay->p=0;
  if (chanc==1) {
    pcmplay->gainl=pcmplay->gainr=trim;
  } else if (pan<=-1.0) {
    pcmplay->gainl=trim;
    pcmplay->gainr=0.0f;
  } else if (pan>=1.0) {
    pcmplay->gainl=0.0f;
    pcmplay->gainr=trim;
  } else if (pan<0.0) {
    pcmplay->gainl=trim;
    pcmplay->gainr=(1.0+pan)*trim;
  } else if (pan>0.0) {
    pcmplay->gainl=(1.0-pan)*trim;
    pcmplay->gainr=trim;
  } else {
    pcmplay->gainl=pcmplay->gainr=trim;
  }
  return 0;
}

int synth_pcmplay_update(float *v,int framec,struct synth_pcmplay *pcmplay) {
  if (!pcmplay||!pcmplay->pcm) return 0;
  int updc=pcmplay->pcm->c-pcmplay->p;
  if (updc>framec) updc=framec;
  if (updc>0) {
    int i=updc;
    const float *src=pcmplay->pcm->v+pcmplay->p;
    pcmplay->p+=updc;
    if (pcmplay->chanc==1) {
      for (;i-->0;v++,src++) (*v)+=(*src)*pcmplay->gainl;
    } else {
      for (;i-->0;v+=pcmplay->chanc,src++) {
        v[0]+=(*src)*pcmplay->gainl;
        v[1]+=(*src)*pcmplay->gainr;
      }
    }
  }
  return (pcmplay->p>=pcmplay->pcm->c)?0:1;
}
