#include "synth_internal.h"

/* Delete.
 */
 
static void synth_res_cleanup(struct synth_res *res) {
  if (res->pcm) synth_pcm_del(res->pcm);
}
 
void synth_del(struct synth *synth) {
  if (!synth) return;
  if (synth->resv) {
    while (synth->resc-->0) synth_res_cleanup(synth->resv+synth->resc);
    free(synth->resv);
  }
  if (synth->qbuf) free(synth->qbuf);
  if (synth->scratch) free(synth->scratch);
  synth_wave_del(synth->sine);
  while (synth->channelc>0) synth_channel_del(synth->channelv[--(synth->channelc)]);
  while (synth->pcmplayc>0) synth_pcmplay_cleanup(synth->pcmplayv+(--(synth->pcmplayc)));
  if (synth->printerv) {
    while (synth->printerc-->0) synth_printer_del(synth->printerv[synth->printerc]);
    free(synth->printerv);
  }
  free(synth);
}

/* Calculate rate tables.
 */
 
static void synth_rates_init(struct synth *synth) {

  /* Calculate one octave in the middle entirely from scratch at each note.
   * The "440.0" here is the frequency of MIDI note 69 (A4), and it's safe to change arbitrarily*.
   * [*] Unless you want songs to sound correct.
   */
  float frate=(float)synth->rate;
  int start_noteid=0x40,i,noteid;
  for (i=12,noteid=start_noteid;i-->0;noteid++) {
    synth->ratefv[noteid]=(440.0f*powf(2.0f,(noteid-69)/12.0f))/frate;
    synth->rateiv[noteid]=(uint32_t)(synth->ratefv[noteid]*4294976295.0f);
  }
  
  /* Walk down from there, dividing by two.
   * Then up, multiplying by two.
   */
  for (noteid=start_noteid;noteid-->0;) {
    synth->ratefv[noteid]=synth->ratefv[noteid+12]*0.5f;
    synth->rateiv[noteid]=synth->rateiv[noteid+12]>>1;
  }
  for (noteid=start_noteid+12;noteid<0x80;noteid++) {
    synth->ratefv[noteid]=synth->ratefv[noteid-12]*2.0f;
    synth->rateiv[noteid]=synth->rateiv[noteid-12]<<1;
  }
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
  if (!(synth->scratch=malloc(sizeof(float)*SYNTH_UPDATE_LIMIT_FRAMES*chanc))) {
    synth_del(synth);
    return 0;
  }
  synth_rates_init(synth);
  return synth;
}

/* Resource list.
 */
 
static int synth_resv_search(const struct synth *synth,int qid) {
  // Beyond end of list comes up a lot during load, and it would otherwise be worst-case for the search.
  if (!synth->resc||(qid>synth->resv[synth->resc-1].qid)) return -synth->resc-1;
  int lo=0,hi=synth->resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int q=synth->resv[ck].qid;
         if (qid<q) hi=ck;
    else if (qid>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static struct synth_res *synth_resv_insert(struct synth *synth,int p,int id) {
  if ((p<0)||(p>synth->resc)) return 0;
  if (p&&(id<=synth->resv[p-1].qid)) return 0;
  if ((p<synth->resc)&&(id>=synth->resv[p].qid)) return 0;
  if (synth->resc>=synth->resa) {
    int na=synth->resa+64;
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
  res->qid=id;
  return res;
}

/* Load resources.
 */
 
static int synth_load_res(struct synth *synth,int qualifier,int id,const void *src,int srcc) {
  if (!synth||(id<1)||(id>0xffff)||(srcc<0)||(srcc&&!src)) return -1;
  if (synth->framec_total) return -1; // No adding resources once running.
  id|=qualifier;
  int p=synth_resv_search(synth,id);
  if (p>=0) return -1; // No replacing resources.
  p=-p-1;
  struct synth_res *res=synth_resv_insert(synth,p,id);
  if (!res) return -1;
  res->v=src;
  res->c=srcc;
  return 0;
}

int synth_load_song(struct synth *synth,int id,const void *src,int srcc) {
  return synth_load_res(synth,0x10000,id,src,srcc);
}

int synth_load_sound(struct synth *synth,int id,const void *src,int srcc) {
  return synth_load_res(synth,0,id,src,srcc);
}

/* Update, public entry points.
 */

void synth_updatef(float *v,int c,struct synth *synth) {
  memset(v,0,sizeof(float)*c);
  if (synth->framec_in_progress) return;
  int framec=c/synth->chanc;
  while (framec>SYNTH_UPDATE_LIMIT_FRAMES) {
    synth_update_internal(v,SYNTH_UPDATE_LIMIT_FRAMES,synth);
    v+=SYNTH_UPDATE_LIMIT_FRAMES*synth->chanc;
    framec-=SYNTH_UPDATE_LIMIT_FRAMES;
  }
  if (framec>0) {
    synth_update_internal(v,framec,synth);
  }
}

static void synth_quantize(int16_t *dst,const float *src,int c) {
  for (;c-->0;dst++,src++) {
    if (*src<=-1.0f) *dst=-32768;
    else if (*src>=1.0f) *dst=32767;
    else *dst=(int16_t)((*src)*32767.0f);
  }
}

void synth_updatei(int16_t *v,int c,struct synth *synth) {
  if (synth->framec_in_progress) {
    memset(v,0,c<<1);
    return;
  }
  if (!synth->qbuf) {
    synth->qbufa=synth->chanc*SYNTH_QBUF_SIZE_FRAMES;
    if (!(synth->qbuf=malloc(sizeof(float)*synth->qbufa))) {
      memset(v,0,c<<1);
      return;
    }
  }
  while (c>synth->qbufa) {
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

/* End song.
 */
 
void synth_end_song(struct synth *synth) {
  synth->songid=0;
  synth->songrepeat=0;
  synth->song=0;
  synth->songc=0;
  synth->songp=0;
  synth->songloopp=0;
  synth->songdelay=0;
  synth->tempo_frames=0;
  int i=synth->channelc;
  while (i-->0) {
    synth_channel_terminate(synth->channelv[i]);
  }
  memset(synth->channel_by_chid,0,sizeof(synth->channel_by_chid));
}

/* Create new channels for song.
 */
 
int synth_prepare_song_channels(struct synth *synth,const struct eau_file *file) {
  struct eau_channel_reader reader={.v=file->chhdrv,.c=file->chhdrc};
  struct eau_channel_entry entry;
  while (eau_channel_reader_next(&entry,&reader)>0) {
  
    // Invalid chid, silent trim, or channel already configured, skip it.
    if (entry.chid>=0x10) continue;
    if (!entry.trim) continue;
    if (entry.mode==EAU_CHANNEL_MODE_NOOP) continue;
    if (synth->channel_by_chid[entry.chid]) continue;
    
    // If the channel doesn't create, we must fail.
    struct synth_channel *channel=synth_channel_new(synth,&entry);
    if (!channel) {
      synth_end_song(synth);
      return -1;
    }
    
    // If we don't have room, evict the first channel in the list.
    // That presumably belongs to some earlier song, hopefully nobody will miss it.
    if (synth->channelc>=SYNTH_CHANNEL_LIMIT) {
      fprintf(stderr,"%s:%d: Forcible eviction of channel.\n",__FILE__,__LINE__);//TODO Confirm that this situation can arise, when changing song too fast.
      synth_channel_del(synth->channelv[0]);
      synth->channelc--;
      memmove(synth->channelv,synth->channelv+1,sizeof(void*)*synth->channelc);
    }
    
    // Add to both channel lists.
    synth->channelv[synth->channelc++]=channel;
    synth->channel_by_chid[entry.chid]=channel;
  }
  return 0;
}

/* Play song.
 */

void synth_play_song(struct synth *synth,int songid,int force,int repeat) {

  // Any out of range ID is effectively zero (which can't exist).
  if ((songid<0)||(songid>0xffff)) songid=0;
  
  // If it's already playing and not forced, just update the repeat flag and return.
  if ((synth->songid==songid)&&!force) {
    synth->songrepeat=repeat?1:0;
    return;
  }
  
  // Acquire serial. If none, adjust the id to zero, and get out if we're not playing anything.
  struct eau_file file={0};
  int resp=synth_resv_search(synth,0x10000|songid);
  if (resp<0) {
    if (!synth->songid) return;
    songid=0;
  } else {
    if (eau_file_decode(&file,synth->resv[resp].v,synth->resv[resp].c)<0) {
      if (!synth->songid) return;
      songid=0;
    }
  }
  
  // End the current song. This cuts sustained notes short and updates context state.
  // If the new song is zero, we're done.
  synth_end_song(synth);
  if (!songid) return;
  
  // Update context state for the new song.
  synth->tempo_frames=(int)(((double)file.tempo*(double)synth->rate)/1000.0);
  if (synth_prepare_song_channels(synth,&file)<0) return;
  synth->song=file.evtv;
  synth->songc=file.evtc;
  synth->songp=0;
  if ((synth->songloopp=file.loopp)>synth->songc) synth->songloopp=0;
  synth->songid=songid;
  synth->songrepeat=repeat?1:0;
  //TODO Consider forcing a blackout period if there was a previous song.
}

/* Begin printing an EAU sound effect and return the PCM object it will print to.
 * Returns STRONG on success.
 */
 
static struct synth_pcm *synth_begin_print(struct synth *synth,const void *src,int srcc) {
  if (synth->printerc>=synth->printera) {
    int na=synth->printera+16;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=realloc(synth->printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth->printerv=nv;
    synth->printera=na;
  }
  struct synth_printer *printer=synth_printer_new(synth,src,srcc);
  if (!printer) return 0;
  synth->printerv[synth->printerc++]=printer;
  synth_printer_update(printer,synth->framec_in_progress);
  if (synth_pcm_ref(printer->pcm)<0) return 0;
  return printer->pcm;
}

/* Play sound.
 */
 
void synth_play_sound(struct synth *synth,int soundid,double trim,double pan) {
  if ((soundid<1)||(soundid>0xffff)) return;
  if (trim<=0.0) return;
  int resp=synth_resv_search(synth,soundid);
  if (resp<0) return;
  struct synth_res *res=synth->resv+resp;
  
  // Begin printing PCM if we don't have it yet.
  if (!res->pcm) {
    //TODO Are we going to support PCM sound formats too? I'm inclined not to.
    if (!(res->pcm=synth_begin_print(synth,res->v,res->c))) {
      res->pcm=synth_pcm_new(1);
      return;
    }
  }
  
  // Create a PCM player and attach (res->pcm) to it.
  struct synth_pcmplay *pcmplay;
  if (synth->pcmplayc<SYNTH_PCMPLAY_LIMIT) {
    pcmplay=synth->pcmplayv+synth->pcmplayc++;
  } else {
    pcmplay=synth->pcmplayv;
    struct synth_pcmplay *q=pcmplay;
    int i=SYNTH_PCMPLAY_LIMIT;
    for (;i-->0;q++) {
      if (!q->pcm||(q->p>=q->pcm->c)) { pcmplay=q; break; }
      if (q->p>pcmplay->p) pcmplay=q;
    }
    synth_pcmplay_cleanup(pcmplay);
    if (synth_pcmplay_setup(pcmplay,synth->chanc,res->pcm,trim,pan)) {
      memset(pcmplay,0,sizeof(struct synth_pcmplay));
    }
  }
}

/* Trivial accessors.
 */

int synth_get_song_id(const struct synth *synth) {
  return synth->songid;
}

double synth_get_playhead(const struct synth *synth) {
  return 0.0;//TODO
}

/* Set playhead.
 */

void synth_set_playhead(struct synth *synth,double s) {
  //TODO
}
