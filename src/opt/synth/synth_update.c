#include "synth_internal.h"

/* If the song is repeating, return playhead to the start and fake a tiny delay.
 * Otherwise uninstall it.
 */
 
static void synth_loop_or_terminate(struct synth *synth,struct synth_song *song) {
  if (song->repeat) {
    if (song->loopp) {
      song->p=song->loopp;
      song->phframes=synth_song_frames_for_bytes(song,song->loopp);
    } else {
      song->p=0;
      song->phframes=0;
    }
    song->delay=1;
  } else if (song==synth->song) {
    synth_song_terminate(song);
    if (synth->pvsong) synth_song_del(synth->pvsong);
    synth->pvsong=song;
    synth->song=0;
  }
}

/* Fire any song events at time zero, advance until we acquire a delay.
 * (limit) in frames and must be >0.
 * Never returns <1 or >limit.
 * Enforces SYNTH_UPDATE_LIMIT too.
 * Song player will acquire a delay but will not actually advance time. The signal updates do that.
 * Fine if no song is playing.
 */
 
static int synth_update_song(struct synth *synth,int limit) {
  if (limit>SYNTH_UPDATE_LIMIT) limit=SYNTH_UPDATE_LIMIT;
  for (;;) {
  
    if (!synth->song) return limit;
    
    if (synth->song->delay>0) {
      if (synth->song->delay<limit) return synth->song->delay;
      return limit;
    }
    
    if (synth->song->p>=synth->song->c) {
      synth_loop_or_terminate(synth,synth->song);
      continue;
    }
    
    uint8_t lead=synth->song->v[synth->song->p++];
    if (!(lead&0x80)) { // Both delay events have the high bit unset, and no others do. Collect multiple delays.
      for (;;) {
        if (lead&0x40) synth->song->delay+=synth_frames_from_ms(synth,((lead&0x3f)+1)<<6);
        else synth->song->delay+=synth_frames_from_ms(synth,lead);
        if ((synth->song->p>=synth->song->c)||(synth->song->v[synth->song->p]&0x80)) {
          if (synth->song->delay<limit) return synth->song->delay;
          return limit;
        }
        lead=synth->song->v[synth->song->p++];
      }
    }
    switch (lead&0xc0) {
      case 0x80: {
          if (synth->song->p>synth->song->c-3) {
            synth->song->repeat=0;
            synth_loop_or_terminate(synth,synth->song);
            break;
          }
          uint8_t a=synth->song->v[synth->song->p++];
          uint8_t b=synth->song->v[synth->song->p++];
          uint8_t c=synth->song->v[synth->song->p++];
          uint8_t chid=(lead>>2)&15;
          uint8_t noteid=((lead&3)<<5)|(a>>3);
          float velocity=(((a&7)<<4)|(b>>4))/127.0f;
          int dur=synth_frames_from_ms(synth,(((b&15)<<8)|c)<<2);
          synth_song_note(synth->song,chid,noteid,velocity,dur);
        } break;
      case 0xc0: {
          if (synth->song->p>synth->song->c-1) {
            synth->song->repeat=0;
            synth_loop_or_terminate(synth,synth->song);
            break;
          }
          int w=synth->song->v[synth->song->p++];
          w|=(lead&3)<<8;
          w+=512;
          uint8_t chid=(lead>>2)&15;
          synth_song_wheel(synth->song,chid,w);
        } break;
    }
  }
}

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
  
  // Let the song and SYNTH_UPDATE_LIMIT drive slicing of the buffer.
  while (framec>0) {
    int updframec=synth_update_song(synth,framec);
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
