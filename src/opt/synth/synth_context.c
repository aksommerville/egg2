#include "synth_internal.h"

/* Delete.
 */
 
static void synth_res_cleanup(struct synth_res *res) {
  if (res->pcm) synth_pcm_del(res->pcm);
}

void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->qbuf) free(synth->qbuf);
  if (synth->resv) {
    while (synth->resc-->0) synth_res_cleanup(synth->resv+synth->resc);
    free(synth->resv);
  }
  if (synth->printerv) {
    while (synth->printerc-->0) synth_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  if (synth->pcmplayv) {
    while (synth->pcmplayc-->0) synth_pcmplay_cleanup(synth->pcmplayv+synth->pcmplayc);
    free(synth->pcmplayv);
  }
  synth_song_del(synth->song);
  synth_song_del(synth->pvsong);
  free(synth);
}

/* New.
 */

struct synth *synth_new(int rate,int chanc) {
  if ((rate<SYNTH_RATE_MIN)||(rate>SYNTH_RATE_MAX)) return 0;
  if ((chanc<SYNTH_CHANC_MIN)||(chanc>SYNTH_CHANC_MAX)) return 0;
  struct synth *synth=calloc(1,sizeof(struct synth));
  if (!synth) return 0;
  synth->rate=rate;
  synth->chanc=chanc;
  //TODO Precalculate rates.
  return synth;
}

/* Resource list primitives.
 */
 
static int synth_resv_search(const struct synth *synth,int id) {
  int lo=0,hi=synth->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct synth_res *q=synth->resv+ck;
         if (id<q->id) hi=ck;
    else if (id>q->id) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct synth_res *synth_resv_insert(struct synth *synth,int p,int id) {
  if ((p<0)||(p>synth->resc)) return 0;
  if (synth->resc>=synth->resa) {
    int na=synth->resa+32;
    if (na>INT_MAX/sizeof(struct synth_res)) return 0;
    void *nv=realloc(synth->resv,sizeof(struct synth_res)*na);
    if (!nv) return 0;
    synth->resv=nv;
    synth->resa=na;
  }
  struct synth_res *res=synth->resv+p;
  memmove(res+1,res,sizeof(struct synth_res)*(synth->resc-p));
  synth->resc++;
  memset(res,0,sizeof(struct synth_res));
  res->id=id;
  return res;
}

/* Resource installation, public.
 */

int synth_install_song(struct synth *synth,int songid,const void *v,int c) {
  if ((songid<1)||(songid>0xffff)) return -1;
  if (!v||(c<1)) return -1;
  int id=SYNTH_ID_SONG|songid;
  int resp=synth_resv_search(synth,id);
  if (resp>=0) return -1;
  resp=-resp-1;
  struct synth_res *res=synth_resv_insert(synth,resp,id);
  if (!res) return -1;
  res->v=v;
  res->c=c;
  return 0;
}

int synth_install_sound(struct synth *synth,int soundid,const void *v,int c) {
  if ((soundid<1)||(soundid>0xffff)) return -1;
  if (!v||(c<1)) return -1;
  int id=SYNTH_ID_SOUND|soundid;
  int resp=synth_resv_search(synth,id);
  if (resp>=0) return -1;
  resp=-resp-1;
  struct synth_res *res=synth_resv_insert(synth,resp,id);
  if (!res) return -1;
  res->v=v;
  res->c=c;
  return 0;
}

void *synth_uninstall_song(struct synth *synth,int songid) {
  if (synth->framec_in_progress) return 0;
  if ((songid<1)||(songid>0xffff)) return 0;
  int id=SYNTH_ID_SONG|songid;
  int resp=synth_resv_search(synth,id);
  if (resp<0) return 0;
  if (synth->song&&(synth->song->songid==songid)) {
    synth_song_del(synth->song);
    synth->song=0;
  }
  if (synth->pvsong&&(synth->pvsong->songid==songid)) {
    synth_song_del(synth->pvsong);
    synth->pvsong=0;
  }
  struct synth_res *res=synth->resv+resp;
  void *result=(void*)res->v;
  synth_res_cleanup(res);
  synth->resc--;
  memmove(res,res+1,sizeof(struct synth_res)*(synth->resc-resp));
  return result;
}

void *synth_uninstall_sound(struct synth *synth,int soundid) {
  if (synth->framec_in_progress) return 0;
  if ((soundid<1)||(soundid>0xffff)) return 0;
  int id=SYNTH_ID_SOUND|soundid;
  int resp=synth_resv_search(synth,id);
  if (resp<0) return 0;
  struct synth_res *res=synth->resv+resp;
  if (res->pcm) { // Eliminate any playing instances of this pcm.
    int i=synth->pcmplayc; while (i-->0) {
      struct synth_pcmplay *pcmplay=synth->pcmplayv+i;
      if (pcmplay->pcm!=res->pcm) continue;
      synth_pcmplay_cleanup(pcmplay);
      synth->pcmplayc--;
      memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(synth->pcmplayc-i));
    }
  }
  void *result=(void*)res->v;
  synth_res_cleanup(res);
  synth->resc--;
  memmove(res,res+1,sizeof(struct synth_res)*(synth->resc-resp));
  return result;
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int songid,int force,int repeat) {
  if (synth->framec_in_progress) return;
  if ((songid<1)||(songid>0xffff)) songid=0; // Anything invalid is equivalent to "not found" or zero.
  
  /* Locate the resource.
   * If not found, we keep going but songid becomes zero.
   */
  struct synth_res *res=0;
  int id=SYNTH_ID_SONG|songid;
  int resp=synth_resv_search(synth,id);
  if (resp>=0) {
    res=synth->resv+resp;
  } else {
    songid=0;
  }
  
  /* No force and already playing? We can stop.
   */
  if (!force) {
    if (!songid&&!res) return;
    if (res&&synth->song&&((res->id&0xffff)==songid)) return;
    //TODO Should we consider swapping (song,pvsong) when the new request matches (pvsong)? Seems logical, but would take some awkward gymnastics.
  }
  
  /* If there's a song playing, tell it to wind down and reassign it to (pvsong).
   * And if we already have a (pvsong), stop it cold.
   * If we have (pvsong) but not (song), leave it be.
   */
  if (synth->song) {
    if (synth->pvsong) {
      synth_song_del(synth->pvsong);
      synth->pvsong=0;
    }
    synth_song_terminate(synth->song);
    synth->pvsong=synth->song;
    synth->song=0;
  }
  
  /* If we have a resource, wrap it in a song and start up.
   */
  if (res) {
    if (synth->song=synth_song_new(synth,res->v,res->c,repeat,synth->chanc)) {
      synth->song->songid=songid;
    }
  }
}

/* Song ID and playhead.
 */

int synth_get_songid(const struct synth *synth) {
  if (synth->song) return synth->song->songid;
  return 0;
}

float synth_get_playhead(const struct synth *synth) {
  if (synth->song) return synth_song_get_playhead(synth->song);
  return 0.0f;
}

void synth_set_playhead(struct synth *synth,float s) {
  if (synth->framec_in_progress) return;
  if (synth->song) synth_song_set_playhead(synth->song,s);
}

/* Begin printing PCM from encoded EAU song.
 * Returns STRONG pcm object on success.
 */
 
static struct synth_pcm *synth_begin_print(struct synth *synth,const void *v,int c) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct synth_printer *printer=synth_printer_new(synth,v,c);
  if (!printer) return 0;
  if (synth_pcm_ref(printer->pcm)<0) {
    synth_printer_del(printer);
    return 0;
  }
  synth_printer_update(printer,synth->framec_in_progress);
  synth->printerv[synth->printerc++]=printer;
  return printer->pcm;
}

/* Play sound.
 */

void synth_play_sound(struct synth *synth,int soundid,float trim,float pan) {
  if (synth->framec_in_progress) return;
  if ((soundid<1)||(soundid>0xffff)) return;
  if (trim<=0.0f) return;
  int id=SYNTH_ID_SOUND|soundid;
  int resp=synth_resv_search(synth,id);
  if (resp<0) return;
  struct synth_res *res=synth->resv+resp;
  
  if (!res->pcm) {
    if (!(res->pcm=synth_begin_print(synth,res->v,res->c))) return;
  }
  
  if (synth->pcmplayc>=synth->pcmplaya) {
    int na=synth->pcmplaya+16;
    if (na>INT_MAX/sizeof(struct synth_pcmplay)) return;
    void *nv=realloc(synth->pcmplayv,sizeof(struct synth_pcmplay)*na);
    if (!nv) return;
    synth->pcmplayv=nv;
    synth->pcmplaya=na;
  }
  struct synth_pcmplay *pcmplay=synth->pcmplayv+synth->pcmplayc++;
  if (synth_pcmplay_init(pcmplay,res->pcm,synth->chanc,trim,pan)<0) {
    synth->pcmplayc--;
    return;
  }
}

/* Frames from milliseconds.
 */
 
int synth_frames_from_ms(const struct synth *synth,int ms) {
  if (ms<=0) return 0;
  int framec=(int)(((double)ms*(double)synth->rate)/1000.0);
  if (framec<1) return 1;
  return framec;
}

/* Stereo trims.
 */
 
void synth_apply_pan(float *triml,float *trimr,float trim,float pan) {
  if (pan<=-1.0f) {
    *triml=trim;
    *trimr=0.0f;
  } else if (pan>=1.0f) {
    *triml=0.0f;
    *trimr=trim;
  } else if (pan<0.0f) {
    *triml=trim;
    *trimr=trim*(1.0f+pan);
  } else if (pan>0.0f) {
    *triml=trim*(1.0f-pan);
    *trimr=trim;
  } else {
    *triml=trim;
    *trimr=trim;
  }
}
