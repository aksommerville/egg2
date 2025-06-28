#include "synth_internal.h"

/* Delete stage.
 */
 
static void synth_stage_del(struct synth_stage *stage) {
  if (!stage) return;
  if (stage->del) stage->del(stage);
  free(stage);
}

/* Delay.
- `0x01 DELAY`: u8.8 period qnotes=1, u0.8 mix=0.5, u0.8 feedback=0.5, u0.8 sparkle=0.
- - Sparkle adjusts the rate slightly for the two stereo channels, noop for mono.
 */
 
struct synth_stage_delay {
  struct synth_stage hdr;
  int lp,rp;
  int lc,rc;
  float *lv,*rv;
  float dry,wet,sto,fbk;
};

#define STAGE ((struct synth_stage_delay*)stage)

static void _delay_del(struct synth_stage *stage) {
  if (STAGE->lv) free(STAGE->lv);
  if (STAGE->rv) free(STAGE->rv);
}

static void _delay_mono(float *v,int framec,struct synth_stage *stage) {
  for (;framec-->0;v++) {
    float dry=*v;
    float prv=STAGE->lv[STAGE->lp];
    *v=dry*STAGE->dry+prv*STAGE->wet;
    STAGE->lv[STAGE->lp]=dry*STAGE->sto+prv*STAGE->fbk;
    if (++(STAGE->lp)>=STAGE->lc) STAGE->lp=0;
  }
}

static void _delay_stereo(float *v,int framec,struct synth_stage *stage) {
  for (;framec-->0;v+=2) {
  
    float dry=v[0];
    float prv=STAGE->lv[STAGE->lp];
    v[0]=dry*STAGE->dry+prv*STAGE->wet;
    STAGE->lv[STAGE->lp]=dry*STAGE->sto+prv*STAGE->fbk;
    if (++(STAGE->lp)>=STAGE->lc) STAGE->lp=0;
    
    dry=v[1];
    prv=STAGE->rv[STAGE->rp];
    v[1]=dry*STAGE->dry+prv*STAGE->wet;
    STAGE->rv[STAGE->rp]=dry*STAGE->sto+prv*STAGE->fbk;
    if (++(STAGE->rp)>=STAGE->rc) STAGE->rp=0;
  }
}

static int _delay_decode(struct synth_stage *stage,struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  int srcp=0;
  float qnotes=1.0f,sparkle=0.0f;
  STAGE->dry=STAGE->wet=STAGE->sto=STAGE->fbk=0.5f;
  if (srcp<srcc-2) {
    qnotes=((src[srcp]<<8)|src[srcp+1])/256.0f;
    srcp+=2;
    if (srcp<srcc) STAGE->dry=src[srcp++]/255.0f;
    if (srcp<srcc) STAGE->wet=src[srcp++]/255.0f;
    if (srcp<srcc) STAGE->sto=src[srcp++]/255.0f;
    if (srcp<srcc) STAGE->fbk=src[srcp++]/255.0f;
    if (srcp<srcc) sparkle=(src[srcp++]-0x80)/127.0f;
  }
  
  int framec=(int)((float)pipe->tempo*qnotes);
  if (framec<1) framec=1;
  
  // If we're mono, initialize only (l) and ignore (sparkle).
  if (pipe->chanc==1) {
    STAGE->lc=framec;
    if (!(STAGE->lv=calloc(sizeof(float),STAGE->lc))) return -1;
  
  // Stereo: Use both ring buffers, and apply (sparkle) if nonzero.
  } else {
    STAGE->lc=STAGE->rc=framec;
    int d=(int)(sparkle*(float)framec*0.125f);
    if ((STAGE->lc-=d)<1) STAGE->lc=1;
    if ((STAGE->rc+=d)<1) STAGE->rc=1;
    if (!(STAGE->lv=calloc(sizeof(float),STAGE->lc))) return -1;
    if (!(STAGE->rv=calloc(sizeof(float),STAGE->rc))) return -1;
  }
  return 0;
}

static struct synth_stage *synth_stage_delay_new(struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  struct synth_stage *stage=calloc(1,sizeof(struct synth_stage_delay));
  if (!stage) return 0;
  stage->del=_delay_del;
  stage->update_mono=_delay_mono;
  if (pipe->chanc==2) stage->update_stereo=_delay_stereo;
  else stage->update_stereo=_delay_mono; // Can't be null but it won't get called. Mono will break the signal but do no technical harm.
  if (_delay_decode(stage,pipe,src,srcc)<0) {
    synth_stage_del(stage);
    return 0;
  }
  return stage;
}

#undef STAGE

/* Waveshaper.
- `0x02 WAVESHAPER`: u0.16... levels. Positive half only and no zero. ie a single 0xffff is noop.
 */
 
struct synth_stage_waveshaper {
  struct synth_stage hdr;
  int ptc;
  float *ptv;
  float scaleup;
};

#define STAGE ((struct synth_stage_waveshaper*)stage)

static void _waveshaper_del(struct synth_stage *stage) {
  if (STAGE->ptv) free(STAGE->ptv);
}

static void _waveshaper_mono(float *v,int framec,struct synth_stage *stage) {
  float scale=STAGE->ptc*0.5f;
  int lastp=STAGE->ptc-1;
  for (;framec-->0;v++) {
    float vup=((*v)+1.0f)*scale;
    int plo=(int)vup;
    if (plo<0) {
      *v=STAGE->ptv[0];
    } else if (plo>=lastp) {
      *v=STAGE->ptv[lastp];
    } else {
      float lo=STAGE->ptv[plo];
      float hi=STAGE->ptv[plo+1];
      float n=vup-(float)plo;
      if (n<0.0f) n=0.0f; else if (n>1.0f) n=1.0f;
      *v=lo*(1.0f-n)+hi*n;
    }
  }
}

static void _waveshaper_stereo(float *v,int framec,struct synth_stage *stage) {
  _waveshaper_mono(v,framec<<1,stage); // We are purely time-invariant, so we can just pretend it's a mono signal of double the length.
}

static int _waveshaper_decode(struct synth_stage *stage,const uint8_t *src,int srcc) {
  
  int explicitc=srcc>>1;
  if (explicitc) STAGE->ptc=1+(explicitc<<1);
  else STAGE->ptc=3;
  if (!(STAGE->ptv=malloc(sizeof(float)*STAGE->ptc))) return -1;
  
  // An empty waveshaper is legal and noop.
  if (!explicitc) {
    STAGE->ptv[0]=-1.0f;
    STAGE->ptv[1]=0.0f;
    STAGE->ptv[2]=1.0f;
    return 0;
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
  
  return 0;
}

static struct synth_stage *synth_stage_waveshaper_new(struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  struct synth_stage *stage=calloc(1,sizeof(struct synth_stage_waveshaper));
  if (!stage) return 0;
  stage->del=_waveshaper_del;
  stage->update_mono=_waveshaper_mono;
  stage->update_stereo=_waveshaper_stereo;
  if (_waveshaper_decode(stage,src,srcc)<0) {
    synth_stage_del(stage);
    return 0;
  }
  return stage;
}

#undef STAGE

/* Tremolo.
- `0x03 TREMOLO`: u8.8 period qnotes, u0.8 depth=1, u0.8 phase=0.
 */
 
struct synth_stage_tremolo {
  struct synth_stage hdr;
  float depth; // actually half of the nominal depth
  float bias;
  uint32_t p;
  uint32_t dp;
  const float *sine;
};

#define STAGE ((struct synth_stage_tremolo*)stage)

static void _tremolo_del(struct synth_stage *stage) {
}

static void _tremolo_mono(float *v,int framec,struct synth_stage *stage) {
  for (;framec-->0;v++) {
    STAGE->p+=STAGE->dp;
    float trim=STAGE->sine[STAGE->p>>SYNTH_WAVE_SHIFT];
    trim*=STAGE->depth;
    trim+=STAGE->bias;
    v[0]*=trim;
  }
}

static void _tremolo_stereo(float *v,int framec,struct synth_stage *stage) {
  for (;framec-->0;v+=2) {
    STAGE->p+=STAGE->dp;
    float trim=STAGE->sine[STAGE->p>>SYNTH_WAVE_SHIFT];
    trim*=STAGE->depth;
    trim+=STAGE->bias;
    v[0]*=trim;
    v[1]*=trim;
  }
}

static int _tremolo_decode(struct synth_stage *stage,struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  int srcp=0;
  float qnotes=1.0f,phase=0.0f;
  STAGE->depth=1.0f;
  if (srcp<srcc-2) {
    qnotes=((src[srcp]<<8)|src[srcp+1])/256.0f;
    srcp+=2;
    if (srcp<srcc) STAGE->depth=src[srcp++]/255.0f;
    if (srcp<srcc) phase=src[srcp++]/255.0f;
  }
  STAGE->depth*=0.5f; // We'll be starting with a sine wave, range of 2.
  STAGE->bias=1.0f-STAGE->depth;
  int framec=(int)((float)pipe->tempo*qnotes);
  if (framec<1) framec=1;
  STAGE->p=(uint32_t)(phase*4294967296.0f);
  STAGE->dp=0xffffffff/framec;
  return 0;
}

static struct synth_stage *synth_stage_tremolo_new(struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  struct synth_stage *stage=calloc(1,sizeof(struct synth_stage_tremolo));
  if (!stage) return 0;
  stage->del=_tremolo_del;
  stage->update_mono=_tremolo_mono;
  stage->update_stereo=_tremolo_stereo;
  STAGE->sine=pipe->synth->sine;
  if (_tremolo_decode(stage,pipe,src,srcc)<0) {
    synth_stage_del(stage);
    return 0;
  }
  return stage;
}

#undef STAGE

/* Delete pipe.
 */
 
void synth_pipe_del(struct synth_pipe *pipe) {
  if (!pipe) return;
  if (pipe->stagev) {
    while (pipe->stagec-->0) synth_stage_del(pipe->stagev[pipe->stagec]);
    free(pipe->stagev);
  }
  free(pipe);
}

/* Decode into new pipe.
 */
 
static int synth_pipe_decode(struct synth_pipe *pipe,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
  
    if (srcp>srcc-2) return -1;
    uint8_t stageid=src[srcp++];
    uint8_t len=src[srcp++];
    if (srcp>srcc-len) return -1;
    const uint8_t *body=src+srcp;
    srcp+=len;
    
    if (pipe->stagec>=pipe->stagea) {
      int na=pipe->stagea+8;
      if (na>INT_MAX/sizeof(void*)) return -1;
      void *nv=realloc(pipe->stagev,sizeof(void*)*na);
      if (!nv) return -1;
      pipe->stagev=nv;
      pipe->stagea=na;
    }
    
    struct synth_stage *stage=0;
    switch (stageid) {
      case 1: stage=synth_stage_delay_new(pipe,body,len); break;
      case 2: stage=synth_stage_waveshaper_new(pipe,body,len); break;
      case 3: stage=synth_stage_tremolo_new(pipe,body,len); break;
      default: continue;
    }
    if (!stage) return -1;
    pipe->stagev[pipe->stagec++]=stage;
  }
  return 0;
}

/* New pipe.
 */

struct synth_pipe *synth_pipe_new(struct synth *synth,int chanc,int tempo,const uint8_t *src,int srcc) {
  struct synth_pipe *pipe=calloc(1,sizeof(struct synth_pipe));
  if (!pipe) return 0;
  pipe->synth=synth;
  pipe->rate=synth->rate;
  if (chanc>=2) pipe->chanc=2;
  else pipe->chanc=1;
  pipe->tempo=tempo;
  if (synth_pipe_decode(pipe,src,srcc)<0) {
    synth_pipe_del(pipe);
    return 0;
  }
  return pipe;
}

/* Run pipe in place.
 */

void synth_pipe_update(float *v,int framec,struct synth_pipe *pipe) {
  struct synth_stage **p=pipe->stagev;
  int i=pipe->stagec;
  if (pipe->chanc==2) {
    for (;i-->0;p++) {
      struct synth_stage *stage=*p;
      stage->update_stereo(v,framec,stage);
    }
  } else {
    for (;i-->0;p++) {
      struct synth_stage *stage=*p;
      stage->update_mono(v,framec,stage);
    }
  }
}
