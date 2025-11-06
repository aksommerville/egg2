#include "synth_internal.h"

/* 0x00 NOOP
 */
 
static void _noop_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {}
static void _noop_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {}
 
static struct synth_pipe_stage *synth_pipe_noop_new() {
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_pipe_stage));
  if (!stage) return 0;
  stage->type=0x00;
  stage->update_mono=_noop_update_mono;
  stage->update_stereo=_noop_update_stereo;
  return stage;
}

/* 0x01 GAIN
 */
 
struct synth_stage_gain {
  struct synth_pipe_stage hdr;
  float mlt,clip,gate;
};

#define STAGE ((struct synth_stage_gain*)stage)
 
static void _gain_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {
  float nclip=-STAGE->clip;
  float ngate=-STAGE->gate;
  for (;framec-->0;dst++) {
    (*dst)*=STAGE->mlt;
    if (*dst<nclip) *dst=nclip;
    else if (*dst<ngate) ;
    else if (*dst<STAGE->gate) *dst=0.0f;
    else if (*dst<STAGE->clip) ;
    else *dst=STAGE->clip;
  }
}

static void _gain_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {
  // Gain is strictly LTI, we can get weird about it:
  _gain_update_mono(dstl,stage,framec);
  _gain_update_mono(dstr,stage,framec);
}
 
static struct synth_pipe_stage *synth_pipe_gain_new(struct synth_song *song,const uint8_t *src,int srcc) {
  if (srcc<2) return 0;
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_stage_gain));
  if (!stage) return 0;
  stage->type=0x01;
  stage->update_mono=_gain_update_mono;
  stage->update_stereo=_gain_update_stereo;
  STAGE->mlt=src[0]+src[1]/256.0f;
  if (srcc>=3) {
    STAGE->clip=src[2]/255.0f;
    if (srcc>=4) STAGE->gate=src[3]/255.0f;
  } else {
    STAGE->clip=1.0f;
  }
  return stage;
}

#undef STAGE

/* 0x02 DELAY
 */
 
struct synth_stage_delay {
  struct synth_pipe_stage hdr;
  struct synth_ring ringl,ringr;
  float dry,wet,sto,fbk;
};

#define STAGE ((struct synth_stage_delay*)stage)

static void _delay_del(struct synth_pipe_stage *stage) {
  synth_ring_cleanup(&STAGE->ringl);
  synth_ring_cleanup(&STAGE->ringr);
}

static void _delay_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {
  for (;framec-->0;dst++) {
    float pv=synth_ring_read(STAGE->ringl);
    synth_ring_write(STAGE->ringl,pv*STAGE->fbk+(*dst)*STAGE->sto);
    synth_ring_step(STAGE->ringl);
    *dst=(*dst)*STAGE->dry+pv*STAGE->wet;
  }
}

static void _delay_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {
  if (!STAGE->ringr.c) {
    _delay_update_mono(dstl,stage,framec);
  } else {
    for (;framec-->0;dstl++,dstr++) {
      float pv=synth_ring_read(STAGE->ringl);
      synth_ring_write(STAGE->ringl,pv*STAGE->fbk+(*dstl)*STAGE->sto);
      synth_ring_step(STAGE->ringl);
      *dstl=(*dstl)*STAGE->dry+pv*STAGE->wet;
      pv=synth_ring_read(STAGE->ringr);
      synth_ring_write(STAGE->ringr,pv*STAGE->fbk+(*dstr)*STAGE->sto);
      synth_ring_step(STAGE->ringr);
      *dstr=(*dstr)*STAGE->dry+pv*STAGE->wet;
    }
  }
}

static struct synth_pipe_stage *synth_pipe_delay_new(struct synth_song *song,const uint8_t *src,int srcc) {
  if (srcc<2) return 0;
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_stage_delay));
  if (!stage) return 0;
  stage->type=0x02;
  stage->del=_delay_del;
  stage->update_mono=_delay_update_mono;
  stage->update_stereo=_delay_update_stereo;
  
  STAGE->dry=(srcc>=3)?(src[2]/255.0f):0.5f;
  STAGE->wet=(srcc>=4)?(src[3]/255.0f):0.5f;
  STAGE->sto=(srcc>=5)?(src[4]/255.0f):0.5f;
  STAGE->fbk=(srcc>=6)?(src[5]/255.0f):0.5f;
  uint8_t sparkle=(srcc>=7)?src[6]:0x80;
  
  int period=synth_song_get_tempo_frames(song,src[0]+src[1]/256.0f);
  int periodl=period,periodr=period;
  if ((song->chanc>=2)&&(sparkle!=0x80)) {
    int range=(SYNTH_SPARKLE_TIME_MS*synth.rate)/1000;
    int d=((sparkle-0x80)*range)>>7;
    periodl-=d;
    periodr+=d;
  }
  if (synth_ring_resize(&STAGE->ringl,periodl)<0) {
    synth_free(stage);
    return 0;
  }
  if (song->chanc>=2) {
    if (synth_ring_resize(&STAGE->ringr,periodr)<0) {
      synth_ring_cleanup(&STAGE->ringl);
      synth_free(stage);
      return 0;
    }
  }
  
  return stage;
}

#undef STAGE

/* 0x03 TREMOLO
 */
 
struct synth_stage_tremolo {
  struct synth_pipe_stage hdr;
  uint32_t lp,rp,dp;
  float mlt,add;
};

#define STAGE ((struct synth_stage_tremolo*)stage)

static void _tremolo_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {
  for (;framec-->0;dst++) {
    STAGE->lp+=STAGE->dp;
    float trem=synth.sine.v[STAGE->lp>>SYNTH_WAVE_SHIFT]*STAGE->mlt+STAGE->add;
    (*dst)*=trem;
  }
}

static void _tremolo_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {
  for (;framec-->0;dstl++,dstr++) {
    STAGE->lp+=STAGE->dp;
    STAGE->rp+=STAGE->dp;
    (*dstl)*=synth.sine.v[STAGE->lp>>SYNTH_WAVE_SHIFT]*STAGE->mlt+STAGE->add;
    (*dstr)*=synth.sine.v[STAGE->rp>>SYNTH_WAVE_SHIFT]*STAGE->mlt+STAGE->add;
  }
}

static struct synth_pipe_stage *synth_pipe_tremolo_new(struct synth_song *song,const uint8_t *src,int srcc) {
  // 0x03 TREMOLO [u8.8 qnotes, u0.8 depth=1, u0.8 phase=0, u0.8 sparkle=0.5]
  if (srcc<2) return 0;
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_stage_tremolo));
  if (!stage) return 0;
  stage->type=0x03;
  stage->update_mono=_tremolo_update_mono;
  stage->update_stereo=_tremolo_update_stereo;
  
  float period=src[0]+src[1]/256.0f;
  float depth=1.0f;
  uint32_t phase=0;
  uint8_t sparkle=0x80;
  if (srcc>=3) {
    depth=src[2]/255.0f;
    if (srcc>=4) {
      phase=src[3];
      phase|=phase<<8;
      phase|=phase<<16;
      if (srcc>=5) {
        sparkle=src[4];
      }
    }
  }
  
  STAGE->mlt=depth*0.5;
  STAGE->add=1.0f-STAGE->mlt;
  STAGE->dp=synth_song_get_tempo_step(song,period);
  if (sparkle!=0x80) {
    int32_t d=sparkle<<24;
    STAGE->lp=phase+d;
    STAGE->rp=phase-d;
  } else {
    STAGE->lp=STAGE->rp=phase;
  }
  
  return stage;
}

#undef STAGE

/* 0x04 DETUNE
 */
 
struct synth_stage_detune {
  struct synth_pipe_stage hdr;
  uint32_t lp,rp,dp; // Position in oscillator.
  struct synth_ring ringl;
  struct synth_ring ringr;
  float sinemlt;
  float dry,wet;
};

#define STAGE ((struct synth_stage_detune*)stage)

static void _detune_del(struct synth_pipe_stage *stage) {
  synth_ring_cleanup(&STAGE->ringl);
  synth_ring_cleanup(&STAGE->ringr);
}

static void _detune_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {
  for (;framec-->0;dst++) {
    synth_ring_write(STAGE->ringl,*dst);
    STAGE->lp+=STAGE->dp;
    int p=(int)((synth.sine.v[STAGE->lp>>SYNTH_WAVE_SHIFT]+1.0f)*STAGE->sinemlt);
    p=STAGE->ringl.p-p;
    if (p<0) p+=STAGE->ringl.c;
    if (p>=STAGE->ringl.c) p-=STAGE->ringl.c;
    *dst=STAGE->ringl.v[p]*STAGE->wet+(*dst)*STAGE->dry;
    synth_ring_step(STAGE->ringl);
  }
}

static void _detune_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {
  if (!STAGE->ringr.c) {
    _detune_update_mono(dstl,stage,framec);
    return;
  }
  for (;framec-->0;dstl++,dstr++) {
  
    synth_ring_write(STAGE->ringl,*dstl);
    STAGE->lp+=STAGE->dp;
    int p=(int)((synth.sine.v[STAGE->lp>>SYNTH_WAVE_SHIFT]+1.0f)*STAGE->sinemlt);
    p=STAGE->ringl.p-p;
    if (p<0) p+=STAGE->ringl.c;
    if (p>=STAGE->ringl.c) p-=STAGE->ringl.c;
    *dstl=STAGE->ringl.v[p]*STAGE->wet+(*dstl)*STAGE->dry;
    synth_ring_step(STAGE->ringl);
  
    synth_ring_write(STAGE->ringr,*dstr);
    STAGE->rp+=STAGE->dp;
    p=(int)((synth.sine.v[STAGE->rp>>SYNTH_WAVE_SHIFT]+1.0f)*STAGE->sinemlt);
    p=STAGE->ringr.p-p;
    if (p<0) p+=STAGE->ringr.c;
    if (p>=STAGE->ringr.c) p-=STAGE->ringr.c;
    *dstr=STAGE->ringr.v[p]*STAGE->wet+(*dstr)*STAGE->dry;
    synth_ring_step(STAGE->ringr);
  }
}

static struct synth_pipe_stage *synth_pipe_detune_new(struct synth_song *song,const uint8_t *src,int srcc) {
  // 0x04 DETUNE [u8.8 qnotes, u0.8 mix=0.5, u0.8 depth=0.5, u0.8 phase=0, u0.8 rightphase=0]. Detune by pingponging back and forth in time.
  if (srcc<2) return 0;
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_stage_detune));
  if (!stage) return 0;
  stage->type=0x04;
  stage->del=_detune_del;
  stage->update_mono=_detune_update_mono;
  stage->update_stereo=_detune_update_stereo;
  
  float period=src[0]+src[1]/256.0f;
  uint8_t depth=0x80,phase=0,rphase=0;
  STAGE->wet=0.5f;
  if (srcc>=3) {
    STAGE->wet=src[2]/256.0f;
    if (srcc>=4) {
      depth=src[3];
      if (srcc>=5) {
        phase=src[4];
        if (srcc>=6) {
          rphase=src[5];
        }
      }
    }
  }
  STAGE->dry=1.0f-STAGE->wet;
  
  const int k=16; // Allowing up to (oscframec) seems way too wide, make it finer.
  int oscframec=synth_song_get_tempo_frames(song,period);
  int ringlen=((oscframec*depth)>>8)/k;
  if (synth_ring_resize(&STAGE->ringl,ringlen)<0) {
    synth_free(stage);
    return 0;
  }
  if (synth_ring_resize(&STAGE->ringr,ringlen)<0) {
    _detune_del(stage);
    synth_free(stage);
    return 0;
  }
  STAGE->dp=synth_song_get_tempo_step(song,period);
  STAGE->lp=phase; STAGE->lp|=STAGE->lp<<8; STAGE->lp|=STAGE->lp<<16;
  STAGE->rp=rphase; STAGE->rp|=STAGE->rp<<8; STAGE->rp|=STAGE->rp<<16;
  
  STAGE->sinemlt=((float)STAGE->ringl.c-1.0f)*0.5f;
  
  return stage;
}

#undef STAGE

/* 0x05 WAVESHAPER
 */

struct synth_stage_waveshaper {
  struct synth_pipe_stage hdr;
  int ptc;
  float *ptv;
  float scaleup;
};

#define STAGE ((struct synth_stage_waveshaper*)stage)

static void _waveshaper_del(struct synth_pipe_stage *stage) {
  if (STAGE->ptv) synth_free(STAGE->ptv);
}

static void _waveshaper_update_mono(float *dst,struct synth_pipe_stage *stage,int framec) {
  float scale=STAGE->ptc*0.5f;
  int lastp=STAGE->ptc-1;
  for (;framec-->0;dst++) {
    float vup=((*dst)+1.0f)*scale;
    int plo=(int)vup;
    if (plo<0) {
      *dst=STAGE->ptv[0];
    } else if (plo>=lastp) {
      *dst=STAGE->ptv[lastp];
    } else {
      float lo=STAGE->ptv[plo];
      float hi=STAGE->ptv[plo+1];
      float n=vup-(float)plo;
      if (n<0.0f) n=0.0f; else if (n>1.0f) n=1.0f;
      *dst=lo*(1.0f-n)+hi*n;
    }
  }
}

static void _waveshaper_update_stereo(float *dstl,float *dstr,struct synth_pipe_stage *stage,int framec) {
  // LTI, we can hijack mono.
  _waveshaper_update_mono(dstl,stage,framec);
  _waveshaper_update_mono(dstr,stage,framec);
}

static struct synth_pipe_stage *synth_pipe_waveshaper_new(struct synth_song *song,const uint8_t *src,int srcc) {
  // 0x05 WAVESHAPER [u0.16 ...coefv]. Positive coefficients only. A single 0xffff is noop.
  struct synth_pipe_stage *stage=synth_calloc(1,sizeof(struct synth_stage_waveshaper));
  if (!stage) return 0;
  stage->type=0x05;
  stage->del=_waveshaper_del;
  stage->update_mono=_waveshaper_update_mono;
  stage->update_stereo=_waveshaper_update_stereo;
  
  int explicitc=srcc>>1;
  if (explicitc) STAGE->ptc=1+(explicitc<<1);
  else STAGE->ptc=3;
  if (!(STAGE->ptv=synth_malloc(sizeof(float)*STAGE->ptc))) {
    synth_free(stage);
    return 0;
  }
  
  // An empty waveshaper is legal and noop.
  if (!explicitc) {
    STAGE->ptv[0]=-1.0f;
    STAGE->ptv[1]=0.0f;
    STAGE->ptv[2]=1.0f;
    return stage;
  }
  
  // Decode both sides together.
  float *dsthi=STAGE->ptv+explicitc;
  float *dstlo=dsthi-1;
  *dsthi++=0.0f;
  int i=explicitc,srcp=0;
  for (;i-->0;srcp+=2,dsthi++,dstlo--) {
    *dsthi=((src[srcp]<<8)|src[srcp+1])/65535.0f;
    *dstlo=-*dsthi;
  }
  return stage;
}

#undef STAGE

/* 0x06 LOPASS
 * 0x07 HIPASS
 * 0x08 BPASS
 * 0x09 NOTCH
 *
 * 2025-10-26: These are a pain, and I'm not convinced that we need them. Punt and see if the need arises.
 */
 
struct synth_stage_iir {
  struct synth_pipe_stage hdr;
};

#define STAGE ((struct synth_stage_iir*)stage)

static struct synth_pipe_stage *synth_pipe_lopass_new(struct synth_song *song,const uint8_t *src,int srcc) {
  return synth_pipe_noop_new();//TODO
}

static struct synth_pipe_stage *synth_pipe_hipass_new(struct synth_song *song,const uint8_t *src,int srcc) {
  return synth_pipe_noop_new();//TODO
}

static struct synth_pipe_stage *synth_pipe_bpass_new(struct synth_song *song,const uint8_t *src,int srcc) {
  return synth_pipe_noop_new();//TODO
}

static struct synth_pipe_stage *synth_pipe_notch_new(struct synth_song *song,const uint8_t *src,int srcc) {
  return synth_pipe_noop_new();//TODO
}

#undef STAGE

/* New stage.
 */
 
static struct synth_pipe_stage *synth_pipe_stage_new(struct synth_song *song,uint8_t type,const uint8_t *src,int srcc) {
  switch (type) {
    case 0x00: return synth_pipe_noop_new();
    case 0x01: return synth_pipe_gain_new(song,src,srcc);
    case 0x02: return synth_pipe_delay_new(song,src,srcc);
    case 0x03: return synth_pipe_tremolo_new(song,src,srcc);
    case 0x04: return synth_pipe_detune_new(song,src,srcc);
    case 0x05: return synth_pipe_waveshaper_new(song,src,srcc);
    case 0x06: return synth_pipe_lopass_new(song,src,srcc);
    case 0x07: return synth_pipe_hipass_new(song,src,srcc);
    case 0x08: return synth_pipe_bpass_new(song,src,srcc);
    case 0x09: return synth_pipe_notch_new(song,src,srcc);
  }
  return 0;
}

/* Delete pipe.
 */
 
void synth_pipe_del(struct synth_pipe *pipe) {
  if (!pipe) return;
  if (pipe->stagev) {
    struct synth_pipe_stage **p=pipe->stagev;
    int i=pipe->stagec;
    for (;i-->0;p++) {
      struct synth_pipe_stage *stage=*p;
      if (stage->del) stage->del(stage);
      synth_free(stage);
    }
    synth_free(pipe->stagev);
  }
  synth_free(pipe);
}

/* Add stage to new pipe.
 */
 
static int synth_pipe_add_stage(struct synth_pipe *pipe,uint8_t type,const uint8_t *src,int srcc) {
  if (pipe->stagec>=pipe->stagea) {
    int na=pipe->stagea+4;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=synth_realloc(pipe->stagev,sizeof(void*)*na);
    if (!nv) return -1;
    pipe->stagev=nv;
    pipe->stagea=na;
  }
  struct synth_pipe_stage *stage=synth_pipe_stage_new(pipe->song,type,src,srcc);
  if (!stage) return -1;
  pipe->stagev[pipe->stagec++]=stage;
  return 0;
}

/* New.
 */

struct synth_pipe *synth_pipe_new(struct synth_song *owner,const uint8_t *src,int srcc) {
  if (!owner) return 0;
  struct synth_pipe *pipe=synth_calloc(1,sizeof(struct synth_pipe));
  if (!pipe) return 0;
  pipe->song=owner;
  int srcp=0;
  while (srcp<srcc) {
    uint8_t type=src[srcp++];
    uint8_t len=0xff;
    if (srcp<srcc) len=src[srcp++];
    if (srcp>srcc-len) {
      synth_pipe_del(pipe);
      return 0;
    }
    if (synth_pipe_add_stage(pipe,type,src+srcp,len)<0) {
      synth_pipe_del(pipe);
      return 0;
    }
    srcp+=len;
  }
  return pipe;
}

/* Update.
 */

void synth_pipe_update_mono(float *dst,struct synth_pipe *pipe,int framec) {
  struct synth_pipe_stage **p=pipe->stagev;
  int i=pipe->stagec;
  for (;i-->0;p++) {
    struct synth_pipe_stage *stage=*p;
    stage->update_mono(dst,stage,framec);
  }
}

void synth_pipe_update_stereo(float *dstl,float *dstr,struct synth_pipe *pipe,int framec) {
  struct synth_pipe_stage **p=pipe->stagev;
  int i=pipe->stagec;
  for (;i-->0;p++) {
    struct synth_pipe_stage *stage=*p;
    stage->update_stereo(dstl,dstr,stage,framec);
  }
}
