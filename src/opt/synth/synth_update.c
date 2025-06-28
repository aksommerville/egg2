#include "synth_internal.h"

/* Update signal graph, adding to (v).
 */
 
static void synth_update_signal(float *v,int framec,struct synth *synth) {

  // Update song players.
  if (synth->song) {
    if (synth_song_update(v,framec,synth->song)<=0) {
      synth_song_del(synth->song);
      synth->song=0;
    }
  }
  if (synth->pvsong) {
    if (synth_song_update(v,framec,synth->pvsong)<=0) {
      synth_song_del(synth->pvsong);
      synth->pvsong=0;
    }
  }
  
  // Update PCM players.
  int i=synth->pcmplayc;
  struct synth_pcmplay *pcmplay=synth->pcmplayv+i;
  while (i-->0) {
    pcmplay--;
    if (synth_pcmplay_update(v,framec,pcmplay)<=0) {
      synth_pcmplay_cleanup(pcmplay);
      synth->pcmplayc--;
      memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(synth->pcmplayc-i));
    }
  }
}

/* Quantize float to int16_t.
 */
 
static void synth_quantize(int16_t *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) {
    int v=(int)((*src)*32000.0f);
    if (v<=-32768) *dst=-32768;
    else if (v>=32767) *dst=32767;
    else *dst=v;
  }
}

/* Public entry points.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  memset(v,0,sizeof(float)*c);
  if (synth->framec_in_progress) return;
  int framec=c/synth->chanc;
  synth->framec_in_progress=framec;
  
  // Update all printers.
  int i=synth->printerc;
  while (i-->0) {
    struct synth_printer *printer=synth->printerv[i];
    if (synth_printer_update(printer,framec)<=0) {
      synth->printerc--;
      memmove(synth->printerv+i,synth->printerv+i+1,sizeof(void*)*(synth->printerc-i));
      synth_printer_del(printer);
    }
  }
  
  // Update no more than SYNTH_UPDATE_LIMIT at a time.
  while (framec>0) {
    int updframec=framec;
    if (updframec>SYNTH_UPDATE_LIMIT) updframec=SYNTH_UPDATE_LIMIT;
    int samplec=updframec*synth->chanc;
    synth_update_signal(v,updframec,synth);
    v+=samplec;
    c-=samplec;
    framec-=updframec;
  }
  
  synth->framec_in_progress=0;
}

void synth_updatei(int16_t *v,int c,struct synth *synth) {
  if (!synth->qbuf) {
    synth->qbufa=1024*synth->chanc;
    if (!(synth->qbuf=malloc(sizeof(float)*synth->qbufa))) {
      synth->qbufa=0;
      memset(v,0,sizeof(int16_t)*c);
      return;
    }
  }
  while (c>=synth->qbufa) {
    synth_updatef(synth->qbuf,synth->qbufa,synth);
    synth_quantize(v,synth->qbuf,synth->qbufa);
    v+=synth->qbufa;
    c-=synth->qbufa;
  }
  if (c>0) {
    synth_updatef(synth->qbuf,c,synth);
    synth_quantize(v,synth->qbuf,c);
  }
}
