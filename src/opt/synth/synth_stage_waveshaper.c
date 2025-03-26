#include "synth_internal.h"

#define STAGE ((struct synth_stage_waveshaper*)stage)

/* Cleanup.
 */
 
static void _waveshaper_del(struct synth_stage *stage) {
  if (STAGE->map) free(STAGE->map);
}

/* Update.
 */
 
static void _waveshaper_update_mono(float *v,int c,struct synth_stage *stage) {
  const float range=((float)STAGE->mapc-1.0f)/2.0f;
  for (;c-->0;v++) {
    if (*v<=-1.0f) {
      *v=STAGE->map[0];
    } else if (*v>=1.0f) {
      *v=STAGE->map[STAGE->mapc-1];
    } else {
      float upscale=((*v)+1.0f)*range;
      int mapp=(int)upscale;
      float dummy;
      float hi=modff(upscale,&dummy);
      float lo=1.0f-hi;
      *v=STAGE->map[mapp]*lo+STAGE->map[mapp+1]*hi;
    }
  }
}

static void _waveshaper_update_stereo(float *v,int c,struct synth_stage *stage) {
  _waveshaper_update_mono(v,c<<1,stage);
}

/* Init.
 */
 
int synth_stage_waveshaper_init(struct synth_stage *stage,const uint8_t *src,int srcc) {
  stage->del=_waveshaper_del;
  int coefc=srcc>>1; // Caller provides only the positive coefficients. There's an implicit zero, and the negative range is presumed symmetric.
  if (coefc<1) { // Empty input means we will emit only silence. But we'll do it correctly and create the minimum three points.
    if (!(STAGE->map=calloc(sizeof(float),3))) return -1;
    STAGE->mapc=3;
  } else {
    STAGE->mapc=1+(coefc<<1);
    if (!(STAGE->map=calloc(sizeof(float),STAGE->mapc))) return -1;
    float *dst=STAGE->map+coefc+1;
    int i=coefc;
    for (;i-->0;dst++,src+=2) {
      *dst=((src[0]<<8)|src[1])/65535.0f;
    }
    dst=STAGE->map+coefc;
    const float *src=dst;
    for (i=coefc;i-->0;) {
      dst--;
      src++;
      *dst=-*src;
    }
  }
  if (stage->chanc==1) stage->update=_waveshaper_update_mono;
  else if (stage->chanc==2) stage->update=_waveshaper_update_stereo;
  return 0;
}
