#include "synth_internal.h"

struct synth synth={0};

/* Cleanup rseource.
 */
 
static void synth_res_cleanup(struct synth_res *res) {
  synth_pcm_del(res->pcm);
}

/* Quit.
 */
 
void synth_quit() {
  if (synth.bufl) synth_free(synth.bufl);
  if (synth.bufr) synth_free(synth.bufr);
  if (synth.rom) synth_free(synth.rom);
  if (synth.songv) {
    while (synth.songc-->0) synth_song_del(synth.songv[synth.songc]);
    synth_free(synth.songv);
  }
  if (synth.pcmplayv) {
    while (synth.pcmplayc-->0) synth_pcmplay_cleanup(synth.pcmplayv+synth.pcmplayc);
    synth_free(synth.pcmplayv);
  }
  if (synth.printerv) {
    while (synth.printerc-->0) synth_printer_del(synth.printerv[synth.printerc]);
    synth_free(synth.printerv);
  }
  if (synth.resv) {
    while (synth.resc-->0) synth_res_cleanup(synth.resv+synth.resc);
    synth_free(synth.resv);
  }
  __builtin_memset(&synth,0,sizeof(struct synth));
}

/* Generate rate tables.
 * The "cents" ones are constant, just implemented as tables for performance's sake.
 * (fratev,iratev) depend on the master output rate.
 */
  
static void synth_generate_rate_tables() {
  const float TWELFTH_ROOT_TWO=1.0594630943592953f;
  const float ONE_TWENTIETH_ROOT_TWO=1.0057929410678534f;
  const float TWELVE_HUNDREDTH_ROOT_TWO=1.0005777895065548f;
  const uint8_t REF_NOTEID=0x45; // A4; can place anywhere but must have a full octave available above.
  const float REF_RATE=440.0; // hz
  int i;
  
  // Precompute bend tables. These are constants, same result every time.
  // For the 100-entry (cents_halfstep), do the first ten straight, then read back from ten earlier, to mitigate rounding error.
  synth.cents_octave[0]=1.0f;
  for (i=1;i<12;i++) synth.cents_octave[i]=synth.cents_octave[i-1]*TWELFTH_ROOT_TWO;
  synth.cents_halfstep[0]=1.0f;
  for (i=1;i<10;i++) synth.cents_halfstep[i]=synth.cents_halfstep[i-1]*TWELVE_HUNDREDTH_ROOT_TWO;
  for (;i<100;i++) synth.cents_halfstep[i]=synth.cents_halfstep[i-10]*ONE_TWENTIETH_ROOT_TWO;
  for (i=0;i<12;i++) synth.invcents_octave[i]=1.0f/synth.cents_octave[i];
  for (i=0;i<100;i++) synth.invcents_halfstep[i]=1.0f/synth.cents_halfstep[i];
  
  // Compute the octave upward from (REF_NOTEID), a halfstep at a time.
  // Then fill in the rest by octave intervals.
  synth.fratev[REF_NOTEID]=REF_RATE/(float)synth.rate;
  for (i=REF_NOTEID+1;i<REF_NOTEID+12;i++) synth.fratev[i]=synth.fratev[i-1]*TWELFTH_ROOT_TWO;
  for (;i<128;i++) synth.fratev[i]=synth.fratev[i-12]*2.0f;
  for (i=REF_NOTEID;i-->0;) synth.fratev[i]=synth.fratev[i+12]*0.5f;
  
  // Clamp floating-point rates to 0.5, then quantize to 32 bits.
  float *fv=synth.fratev;
  uint32_t *iv=synth.iratev;
  for (i=128;i-->0;fv++,iv++) {
    if (*fv>0.5f) *fv=0.5f;
    *iv=(int32_t)((*fv)*4294967296.0f);
  }
}

/* Init.
 */
 
int synth_init(int rate,int chanc,int buffer_frames) {
  if (synth.rate) return -1;
  if ((rate<200)||(rate>200000)) return -1;
  if ((chanc<1)||(chanc>8)) return -1;
  if ((buffer_frames<1)||(buffer_frames>SYNTH_UPDATE_LIMIT_FRAMES)) return -1;
  
  if (synth_malloc_init()<0) return -1;
  
  synth.rate=rate;
  synth.chanc=chanc;
  synth.buffer_frames=buffer_frames;
  
  if (!(synth.bufl=synth_malloc(sizeof(float)*buffer_frames))) { synth_quit(); return -1; }
  if (chanc>=2) {
    if (!(synth.bufr=synth_malloc(sizeof(float)*buffer_frames))) { synth_quit(); return -1; }
  }
  
  synth_wave_generate_sine(&synth.sine);
  synth_generate_rate_tables();
  
  return 0;
}

/* Drop everything immediately, in preparation for replacing the ROM.
 * Keep rate tables, sine table, and buffers.
 */
 
static void synth_drop_everything() {
  
  while (synth.songc>0) {
    synth.songc--;
    synth_song_del(synth.songv[synth.songc]);
  }
  while (synth.pcmplayc>0) {
    synth.pcmplayc--;
    synth_pcmplay_cleanup(synth.pcmplayv+synth.pcmplayc);
  }
  while (synth.printerc>0) {
    synth.printerc--;
    synth_printer_del(synth.printerv[synth.printerc]);
  }
  while (synth.resc>0) {
    synth.resc--;
    synth_res_cleanup(synth.resv+synth.resc);
  }
  
  if (synth.rom) synth_free(synth.rom);
  synth.rom=0;
  synth.romc=0;
}

/* Allocate ROM buffer.
 */
 
void *synth_get_rom(int len) {
  if (len<0) return 0;
  if (synth.framec_in_progress) return 0;
  if (!synth.rate) return 0;
  synth_drop_everything();
  if (len) {
    if (!(synth.rom=synth_calloc(1,len))) return 0;
    synth.romc=len;
  }
  return synth.rom;
}

/* Trivial accessors.
 */

int synth_get_rate() {
  return synth.rate;
}

int synth_get_chanc() {
  return synth.chanc;
}

int synth_get_buffer_size_frames() {
  return synth.buffer_frames;
}

float *synth_get_buffer(int chan) {
  switch (chan) {
    case 0: return synth.bufl;
    case 1: return synth.bufr;
  }
  return 0;
}

/* Update, public entry point.
 */
 
void synth_update(int framec) {
  if (framec<1) return;
  if (framec>synth.buffer_frames) return;
  if (synth.framec_in_progress) return; // Reentry! Something is horribly amiss.
  synth.framec_in_progress=framec;
  __builtin_memset(synth.bufl,0,sizeof(float)*framec);
  if (synth.bufr) __builtin_memset(synth.bufr,0,sizeof(float)*framec);
  
  { // Printers.
    int i=synth.printerc;
    struct synth_printer **p=synth.printerv+i-1;
    for (;i-->0;p--) {
      struct synth_printer *printer=*p;
      int err=synth_printer_update(printer,framec);
      if (err<=0) {
        synth.printerc--;
        __builtin_memmove(p,p+1,sizeof(void*)*(synth.printerc-i));
        synth_printer_del(printer);
      }
    }
  }
  
  { // Songs.
    int i=synth.songc;
    struct synth_song **p=synth.songv+i-1;
    for (;i-->0;p--) {
      struct synth_song *song=*p;
      int err=synth_song_update(synth.bufl,synth.bufr,song,framec);
      if (err<=0) {
        synth.songc--;
        __builtin_memmove(p,p+1,sizeof(void*)*(synth.songc-i));
        synth_song_del(song);
      }
    }
  }
  
  { // PCM players.
    int i=synth.pcmplayc;
    struct synth_pcmplay *pcmplay=synth.pcmplayv+i-1;
    for (;i-->0;pcmplay--) {
      int err=synth_pcmplay_update(synth.bufl,synth.bufr,pcmplay,framec);
      if (err<=0) {
        synth_pcmplay_cleanup(pcmplay);
        synth.pcmplayc--;
        __builtin_memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(synth.pcmplayc-i));
      }
    }
  }
  
  synth.framec_in_progress=0;
}

/* ROM reader, with an extra quirk:
 * If it doesn't start with the ROM signature, assume it starts at song:1.
 */
 
struct synth_rom_reader {
  const uint8_t *v;
  int c,p;
  int tid,rid;
};
struct synth_rom_entry {
  int tid,rid;
  const void *v;
  int c;
};

static int synth_rom_reader_init(struct synth_rom_reader *reader,const uint8_t *src,int srcc) {
  reader->v=src;
  reader->c=srcc;
  reader->rid=1;
  if ((srcc>=4)&&!__builtin_memcmp(src,"\0ERM",4)) {
    reader->p=4;
    reader->tid=1;
  } else {
    reader->p=0;
    reader->tid=EGG_TID_song;
  }
  return 0;
}

static int synth_rom_reader_next(struct synth_rom_entry *entry,struct synth_rom_reader *reader) {
  for (;;) {
    if (reader->p>=reader->c) return 0; // Technically an error but whatever.
    if (!reader->v[reader->p]) return 0; // EOF
    uint8_t lead=reader->v[reader->p++];
    switch (lead&0xc0) {
      case 0x00: reader->tid+=lead; reader->rid=1; break;
      case 0x40: {
          if (reader->p>reader->c-1) return -1;
          int d=(lead&0x3f)<<8;
          d|=reader->v[reader->p++];
          d+=1;
          reader->rid+=d;
        } break;
      case 0x80: {
          if (reader->p>reader->c-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=reader->v[reader->p++]<<8;
          len|=reader->v[reader->p++];
          len+=1;
          if (reader->p>reader->c-len) return -1;
          entry->tid=reader->tid;
          entry->rid=reader->rid;
          entry->v=reader->v+reader->p;
          entry->c=len;
          reader->p+=len;
          reader->rid++;
        } return 1;
      case 0xc0: return -1;
    }
  }
}

/* Get resource.
 * If we haven't yet, read the ROM TOC.
 * (rid) may have SYNTH_RID_SOUND set.
 */
 
static struct synth_res *synth_res_get(int rid) {
  
  // Build up TOC if we don't have it.
  if (synth.romc&&!synth.resc) {
    struct synth_rom_reader reader;
    if (synth_rom_reader_init(&reader,synth.rom,synth.romc)<0) {
      synth.romc=0; // d'oh
    } else {
      struct synth_rom_entry entry;
      while (synth_rom_reader_next(&entry,&reader)>0) {
        if (entry.tid<EGG_TID_song) continue;
        if (entry.tid>EGG_TID_sound) break;
        int rid=entry.rid;
        if (entry.tid==EGG_TID_sound) rid|=SYNTH_RID_SOUND;
        if (synth.resc>=synth.resa) {
          int na=synth.resa+32;
          if (na>INT_MAX/sizeof(struct synth_res)) break;
          void *nv=synth_realloc(synth.resv,sizeof(struct synth_res)*na);
          if (!nv) break;
          synth.resv=nv;
          synth.resa=na;
        }
        struct synth_res *res=synth.resv+synth.resc++;
        __builtin_memset(res,0,sizeof(struct synth_res));
        res->rid=rid;
        res->serial=entry.v;
        res->serialc=entry.c;
      }
    }
  }
  
  // Search resources.
  int lo=0,hi=synth.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct synth_res *q=synth.resv+ck;
         if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return q;
  }
  return 0;
}

/* Spawn a printer and return its output PCM (STRONG).
 */
 
struct synth_pcm *synth_begin_print(const void *src,int srcc) {
  if (synth.printerc>=synth.printera) {
    int na=synth.printera+8;
    if (na>INT_MAX/sizeof(void*)) return 0;
    void *nv=synth_realloc(synth.printerv,sizeof(void*)*na);
    if (!nv) return 0;
    synth.printerv=nv;
    synth.printera=na;
  }
  struct synth_printer *printer=synth_printer_new(src,srcc);
  if (!printer) return 0;
  if (synth_pcm_ref(printer->pcm)<0) {
    synth_printer_del(printer);
    return 0;
  }
  if (synth.framec_in_progress) {
    synth_printer_update(printer,synth.framec_in_progress);
    // If it fails or completes, whatever, leave it on the pile until the next update cleans it up.
  }
  synth.printerv[synth.printerc++]=printer;
  return printer->pcm;
}

/* Play sound.
 * Begins print if necessary.
 */

void synth_play_sound(int rid,float trim,float pan) {
  if (synth.framec_in_progress) return;
  if ((rid<1)||(rid>0xffff)) return;
  struct synth_res *res=synth_res_get(SYNTH_RID_SOUND|rid);
  if (!res) return;
  
  // Start printing if we need to.
  // If it fails to start, eg malformed data, try to create a single-sample pcm as a marker, so we don't try to decode again next time.
  if (!res->pcm) {
    if (!(res->pcm=synth_begin_print(res->serial,res->serialc))) {
      if (!(res->pcm=synth_pcm_new(1))) return;
    }
  }
  if (res->pcm->c<=1) return; // Not worth playing.
  
  // Install a new pcmplay.
  if (synth.pcmplayc>=synth.pcmplaya) {
    int na=synth.pcmplaya+16;
    if (na>INT_MAX/sizeof(struct synth_pcmplay)) return;
    void *nv=synth_realloc(synth.pcmplayv,sizeof(struct synth_pcmplay)*na);
    if (!nv) return;
    synth.pcmplayv=nv;
    synth.pcmplaya=na;
  }
  struct synth_pcmplay *pcmplay=synth.pcmplayv+synth.pcmplayc++;
  if (synth_pcmplay_init(pcmplay,res->pcm,trim,pan)<0) synth.pcmplayc--;
}

/* Begin song.
 */

int synth_play_song(int songid,int rid,int repeat,float trim,float pan) {
  if (synth.framec_in_progress) return -1;
  if (songid<=0) return -1;
  
  // Stop any song already using this songid.
  struct synth_song **p=synth.songv;
  int i=synth.songc;
  for (;i-->0;p++) {
    struct synth_song *song=*p;
    if (song->songid==songid) {
      synth_song_stop(song);
    }
  }
  
  // Find the resource. Not found is a zero, not a hard error.
  if ((rid<1)||(rid>0xffff)) rid=0;
  struct synth_res *res=synth_res_get(rid);
  if (!res) return 0;
  
  if (synth.songc>=synth.songa) {
    int na=synth.songa+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=synth_realloc(synth.songv,sizeof(void*)*na);
    if (!nv) return -1;
    synth.songv=nv;
    synth.songa=na;
  }
  struct synth_song *song=synth_song_new(synth.chanc,res->serial,res->serialc,trim,pan);
  if (!song) return -1;
  song->songid=songid;
  song->repeat=repeat;
  song->rid=rid;
  synth.songv[synth.songc++]=song;

  return song->songid;
}

/* Stop all songs.
 */

void synth_stop_all_songs() {
  if (synth.framec_in_progress) return;
  int i=synth.songc;
  while (i-->0) {
    synth_song_stop(synth.songv[synth.songc]);
  }
}

/* Get song by id.
 */
 
static struct synth_song *synth_song_by_songid(int songid) {
  if (songid<1) return 0;
  struct synth_song **p=synth.songv;
  int i=synth.songc;
  for (;i-->0;p++) {
    struct synth_song *song=*p;
    if (song->songid==songid) return song;
  }
  return 0;
}

/* Get enumerated property.
 */

float synth_get(int songid,int chid,int prop) {
  struct synth_song *song=synth_song_by_songid(songid);
  if (!song) return 0.0f;
  struct synth_channel *channel=0;
  if ((chid>=0)&&(chid<0x10)) channel=song->channel_by_chid[chid];
  switch (prop) {
    case SYNTH_PROP_EXISTENCE:  {
        if ((chid>=0)&&(chid<0x10)) return channel?1.0f:0.0f;
        return 1.0f; // Song exists, yes.
      }
    case SYNTH_PROP_TEMPO: return song->tempo;
    case SYNTH_PROP_PLAYHEAD: return synth_song_get_playhead(song);
    case SYNTH_PROP_TRIM: {
        if ((chid>=0)&&(chid<0x10)) return channel?channel->trim:0.0f;
        return song->trim;
      }
    case SYNTH_PROP_PAN: {
        if ((chid>=0)&&(chid<0x10)) return channel?channel->pan:0.0f;
        return song->pan;
      }
    case SYNTH_PROP_WHEEL: {
        if (!channel) return 0.0f;
        return channel->wheelf;
      }
  }
  return 0.0f;
}

/* Set enumerated property.
 */
 
void synth_set(int songid,int chid,int prop,float v) {
  if (synth.framec_in_progress) return;
  struct synth_song *song=synth_song_by_songid(songid);
  if (!song) return;
  struct synth_channel *channel=0;
  if ((chid>=0)&&(chid<0x10)) channel=song->channel_by_chid[chid];
  switch (prop) {
    case SYNTH_PROP_EXISTENCE: {
        if ((chid>=0)&&(chid<0x10)) return; // Can't unexist a channel.
        if (v<=0.0f) synth_song_stop(song);
      } break;
    case SYNTH_PROP_TEMPO: break; // readonly
    case SYNTH_PROP_PLAYHEAD: {
        synth_song_set_playhead(song,v);
      } break;
    case SYNTH_PROP_TRIM: {
        if ((chid>=0)&&(chid<0x10)) {
          if (channel) synth_channel_set_trim(channel,v);
        } else {
          synth_song_set_trim(song,v);
        }
      } break;
    case SYNTH_PROP_PAN: {
        if ((chid>=0)&&(chid<0x10)) {
          if (channel) synth_channel_set_pan(channel,v);
        } else {
          synth_song_set_pan(song,v);
        }
      } break;
    case SYNTH_PROP_WHEEL: {
        if (channel) synth_channel_set_wheel(channel,v);
      } break;
  }
}

/* Dispatch event.
 */
 
static struct synth_channel *synth_event_get_channel(int songid,uint8_t chid) {
  if (synth.framec_in_progress) return 0;
  if (chid>=0x10) return 0;
  struct synth_song *song=synth_song_by_songid(songid);
  if (!song) return 0;
  return song->channel_by_chid[chid];
}
 
void synth_event_note_off(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=synth_event_get_channel(songid,chid);
  if (!channel) return;
  synth_channel_note_off(channel,noteid,velocity);
}

void synth_event_note_on(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  struct synth_channel *channel=synth_event_get_channel(songid,chid);
  if (!channel) return;
  synth_channel_note_on(channel,noteid,velocity);
}

void synth_event_note_once(int songid,uint8_t chid,uint8_t noteid,uint8_t velocity,int durms) {
  struct synth_channel *channel=synth_event_get_channel(songid,chid);
  if (!channel) return;
  synth_channel_note_once(channel,noteid,velocity,durms);
}

/* Frames from milliseconds.
 * Negative or zero returns zero; anything positive guarantees to return positive.
 */
 
int synth_frames_from_ms(int ms) {
  if (ms<=0) return 0;
  int framec=(int)(((float)ms*(float)synth.rate)/1000.0f);
  if (framec<1) framec=1;
  return framec;
}

/* Bend from cents.
 */
 
float synth_bend_from_cents(int cents) {
  float bend=1.0f;
  if (cents>0) {
    if (cents>SYNTH_BEND_LIMIT_CENTS) cents=SYNTH_BEND_LIMIT_CENTS;
    while (cents>=1200) { cents-=1200; bend*=2.0f; }
    bend*=synth.cents_octave[cents/100]*synth.cents_halfstep[cents%100];
  } else {
    if (cents<-SYNTH_BEND_LIMIT_CENTS) cents=-SYNTH_BEND_LIMIT_CENTS;
    cents=-cents;
    while (cents>=1200) { cents-=1200; bend*=0.5f; }
    bend*=synth.invcents_octave[cents/100]*synth.invcents_halfstep[cents%100];
  }
  return bend;
}
