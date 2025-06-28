#include "synth_internal.h"

/* Decode config.
 */
 
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int rate) {
  const uint8_t *SRC=src;
  int srcp=0;
  
  /* Starts with flags.
   * Any unknown flag is an error; future flags are likely influence framing.
   */
  if (srcp>=srcc) return -1;
  env->flags=SRC[srcp++];
  if (env->flags&~(SYNTH_ENV_FLAG_INITIALS|SYNTH_ENV_FLAG_VELOCITY|SYNTH_ENV_FLAG_SUSTAIN)) return -1;
  
  /* Next, the initial values or zero.
   */
  if (env->flags&SYNTH_ENV_FLAG_INITIALS) {
    if (srcp>srcc-2) return -1;
    env->initlo=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if (env->flags&SYNTH_ENV_FLAG_VELOCITY) {
      if (srcp>srcc-2) return -1;
      env->inithi=(SRC[srcp]<<8)|SRC[srcp+1];
      srcp+=2;
    } else {
      env->inithi=env->initlo;
    }
  } else {
    env->initlo=env->inithi=0.0f;
  }
  
  /* Then one byte containing both (susp) and (pointc) -- why we're intrinsically limited to 15 points.
   */
  if (srcp>=srcc) return -1;
  uint8_t susp_ptc=SRC[srcp++];
  env->susp=susp_ptc>>4;
  env->pointc=susp_ptc&15;
  int ptlen=(env->flags&SYNTH_ENV_FLAG_VELOCITY)?8:4;
  if (srcp>srcc-ptlen*env->pointc) return -1;
  
  /* Decode points initially without the sustain insertion, and with times in ms.
   */
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->tlo=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
    point->vlo=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
    if (env->flags&SYNTH_ENV_FLAG_VELOCITY) {
      point->thi=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
      point->vhi=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
    } else {
      point->thi=point->tlo;
      point->vhi=point->vlo;
    }
  }
  
  /* Convert all times to hz, and clamp to 1.
   */
  float tscale=(float)rate/1000.0f;
  for (point=env->pointv,i=env->pointc;i-->0;point++) {
    if ((point->tlo=(int)(point->tlo*tscale))<1) point->tlo=1;
    if ((point->thi=(int)(point->thi*tscale))<1) point->thi=1;
  }
  
  /* If sustain is in play, insert a new point after (susp) with the same values as the prior, and bump (susp) by one.
   * If the sustain flag is set, but (susp) out of range, that's an error.
   */
  if (env->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    if (env->susp>=env->pointc) return -1;
    point=env->pointv+env->susp;
    memmove(point+1,point,sizeof(struct synth_env_point)*(env->pointc-env->susp));
    env->pointc++;
    env->susp++; // (susp) refers to the second of the two identical points; the one we have free rein over its duration.
    point++;
    point->tlo=point->thi=1;
  }
  
  return srcp;
}

/* Default configs.
 */
 
void synth_env_default_level(struct synth_env *env,int rate) {
  const uint8_t serial[]={
    SYNTH_ENV_FLAG_VELOCITY|SYNTH_ENV_FLAG_SUSTAIN,
    0x13,
    0x00,0x20, 0x40,0x00, 0x00,0x10, 0xff,0xff,
    0x00,0x30, 0x30,0x00, 0x00,0x30, 0x50,0x00,
    0x00,0x80, 0x00,0x00, 0x01,0x00, 0x00,0x00,
  };
  synth_env_decode(env,serial,sizeof(serial),rate);
  synth_env_scale(env,1.0f/65535.0f);
  env->flags|=SYNTH_ENV_FLAG_DEFAULT; // Add after; decode will correctly fail on it in serial data.
}

void synth_env_default_range(struct synth_env *env,int rate) {
  const uint8_t serial[]={ // Constant one.
    SYNTH_ENV_FLAG_INITIALS,
    0xff,0xff,
    0x00,
  };
  synth_env_decode(env,serial,sizeof(serial),rate);
  synth_env_scale(env,1.0f/65535.0f);
  env->flags|=SYNTH_ENV_FLAG_DEFAULT; // Add after; decode will correctly fail on it in serial data.
}

void synth_env_default_pitch(struct synth_env *env,int rate) {
  const uint8_t serial[]={0,0}; // Constant-zero, except this time it's signed cents.
  synth_env_decode(env,serial,sizeof(serial),rate);
  env->flags|=SYNTH_ENV_FLAG_DEFAULT; // Add after; decode will correctly fail on it in serial data.
}

/* Arithmetic on all config values.
 */
 
void synth_env_scale(struct synth_env *env,float mlt) {
  env->initlo*=mlt;
  env->inithi*=mlt;
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->vlo*=mlt;
    point->vhi*=mlt;
  }
}

void synth_env_bias(struct synth_env *env,float add) {
  env->initlo+=add;
  env->inithi+=add;
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->vlo+=add;
    point->vhi+=add;
  }
}

/* Initialize runner.
 */
 
void synth_env_apply(struct synth_env *runner,const struct synth_env *config,float velocity,int durframes) {

  // A few things copy verbatim.
  runner->flags=config->flags;
  runner->susp=config->susp;
  runner->pointc=config->pointc;
  
  // Copy initial and points with respect to (velocity) if applicable. Only "lo" on the runner's side.
  struct synth_env_point *dst=runner->pointv;
  const struct synth_env_point *src=config->pointv;
  int i=config->pointc;
  if (!(config->flags&SYNTH_ENV_FLAG_VELOCITY)||(velocity<=0.0f)) {
    runner->initlo=config->initlo;
    for (;i-->0;dst++,src++) {
      dst->tlo=src->tlo;
      dst->vlo=src->vlo;
    }
  } else if (velocity>=1.0f) {
    runner->initlo=config->inithi;
    for (;i-->0;dst++,src++) {
      dst->tlo=src->thi;
      dst->vlo=src->vhi;
    }
  } else {
    float hiw=velocity;
    float low=1.0f-hiw;
    runner->initlo=config->initlo*low+config->inithi*hiw;
    for (;i-->0;dst++,src++) {
      if ((dst->tlo=(int)((float)src->tlo*low+(float)src->thi*hiw))<1) dst->tlo=1;
      dst->vlo=src->vlo*low+src->vhi*hiw;
    }
  }
  
  // If sustained, the sustain point's duration depends on (durframes).
  if (config->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    for (i=0,dst=runner->pointv;i<runner->susp;i++,dst++) durframes-=dst->tlo;
    if (durframes>0) dst->tlo=durframes;
    else dst->tlo=1;
  }
  
  // If there are no points, set up runner with the initial value and call it finished.
  if (!runner->pointc) {
    runner->pointp=0;
    runner->v=runner->initlo;
    runner->c=INT_MAX;
    runner->dv=0.0f;
    runner->finished=1;
    
  // At least one point is present, so begin walking from initial to that.
  } else {
    runner->pointp=0;
    runner->v=runner->initlo;
    runner->c=runner->pointv[0].tlo;
    runner->dv=(runner->pointv[0].vlo-runner->v)/(float)runner->c;
    runner->finished=0;
  }
}

/* Release.
 */
 
void synth_env_release(struct synth_env *env) {
  if (!(env->flags&SYNTH_ENV_FLAG_SUSTAIN)) return;
  if (env->pointp>env->susp) return;
  if (env->pointp<env->susp) {
    env->pointv[env->susp].tlo=1;
    return;
  }
  env->c=1;
}

/* Advance to next point.
 */
 
void synth_env_advance(struct synth_env *env) {
  env->pointp++;
  if (env->pointp>=env->pointc) {
    env->pointp=env->pointc;
    env->finished=1;
    env->dv=0.0f;
    env->c=INT_MAX;
    if (env->pointc) {
      env->v=env->pointv[env->pointc-1].vlo;
    } else {
      env->v=env->initlo;
    }
  } else {
    const struct synth_env_point *point=env->pointv+env->pointp;
    env->v=point[-1].vlo;
    env->c=point->tlo;
    env->dv=(point->vlo-env->v)/(float)env->c;
  }
}
