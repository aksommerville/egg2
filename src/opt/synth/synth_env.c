#include "synth_internal.h"

/* Default envelope.
 * When a wave is unspecified we use sine. Unfortunately, there isn't such a canonical default for envelopes.
 * Whatever we do here must be done exactly the same in web, so keep it simple.
 */
 
static int synth_env_default(struct synth_env *env,int rate) {
  env->flags=SYNTH_ENV_FLAG_SUSTAIN;
  env->susp=1;
  env->initlo=env->inithi=0.0f;
  env->pointc=3;
  env->pointv[0].tlo=env->pointv[0].thi=rate/50; // 20 ms attack
  env->pointv[0].vlo=env->pointv[0].vhi=0.500;   // 1/2 attack level
  env->pointv[1].tlo=env->pointv[1].thi=rate/50; // 20 ms decay
  env->pointv[1].vlo=env->pointv[1].vhi=0.125;   // 1/8 sustain level
  env->pointv[2].tlo=env->pointv[2].thi=rate/8;  // 125 ms release
  env->pointv[2].vlo=env->pointv[2].vhi=0.000;   // silence at the end (this one at least is not debatable)
  // If you change these, update eau.c:eau_estimate_channel_duration()
  return 0;
}

/* Decode.
 */
 
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int rate) {
  if (!src||(srcc<1)) return synth_env_default(env,rate);
  if (!env) return -1;
  const uint8_t *SRC=src;
  int srcp=0;
  double tscale=(double)rate/1000.0;
  
  env->flags=SRC[srcp++];
  
  if (env->flags&SYNTH_ENV_FLAG_INITIAL) {
    if (srcp>srcc-2) return -1;
    env->initlo=((SRC[srcp]<<8)|SRC[srcp+1])/65535.0f;
    srcp+=2;
    if (env->flags&SYNTH_ENV_FLAG_VELOCITY) {
      if (srcp>srcc-2) return -1;
      env->inithi=((SRC[srcp]<<8)|SRC[srcp+1])/65535.0f;
      srcp+=2;
    }
  }
  
  if (env->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    if (srcp>=srcc) return -1;
    env->susp=SRC[srcp++];
  }
  
  if (srcp>=srcc) return -1;
  env->pointc=SRC[srcp++];
  if (env->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    if (env->pointc>SYNTH_ENV_POINT_LIMIT-1) return -1;
  } else {
    if (env->pointc>SYNTH_ENV_POINT_LIMIT) return -1;
  }
  int ptlen=(env->flags&SYNTH_ENV_FLAG_VELOCITY)?8:4;
  if (srcp>srcc-ptlen*env->pointc) return -1;
  
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->tlo=(SRC[srcp]<<8)|SRC[srcp+1];
    srcp+=2;
    if ((point->tlo=(int)((double)point->tlo*tscale))<1) point->tlo=1;
    point->vlo=((SRC[srcp]<<8)|SRC[srcp+1])/65535.0f;
    srcp+=2;
    if (env->flags&SYNTH_ENV_FLAG_VELOCITY) {
      point->thi=(SRC[srcp]<<8)|SRC[srcp+1];
      srcp+=2;
      if ((point->thi=(int)((double)point->thi*tscale))<1) point->thi=1;
      point->vhi=((SRC[srcp]<<8)|SRC[srcp+1])/65535.0f;
      srcp+=2;
    } else {
      point->thi=point->tlo;
      point->vhi=point->vlo;
    }
  }
  
  if (env->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    if (env->susp>=env->pointc) {
      env->flags&=~SYNTH_ENV_FLAG_SUSTAIN;
    } else {
      point=env->pointv+env->susp;
      memmove(point+1,point,sizeof(struct synth_env_point)*(env->pointc-env->susp));
      env->pointc++;
      env->susp++; // (susp) when valid is the extra point, with time initially minimum
      point++;
      point->tlo=point->thi=1;
    }
  }
  
  return srcp;
}

/* Adjust.
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

/* Start runner.
 */

void synth_env_reset(struct synth_env *runner,const struct synth_env *config,float velocity,int durframec) {

  // Copy points, interpolating values if intermediate velocity.
  runner->flags=config->flags;
  runner->pointc=config->pointc;
  if (!(config->flags&SYNTH_ENV_FLAG_VELOCITY)||(velocity<=0.0f)) {
    runner->initlo=config->initlo;
    memcpy(runner->pointv,config->pointv,sizeof(struct synth_env_point)*config->pointc);
  } else if (velocity>=1.0f) {
    runner->initlo=config->inithi;
    struct synth_env_point *dpt=runner->pointv;
    const struct synth_env_point *spt=config->pointv;
    int i=config->pointc;
    for (;i-->0;dpt++,spt++) {
      dpt->tlo=spt->thi;
      dpt->vlo=spt->vhi;
    }
  } else {
    float l=1.0f-velocity;
    runner->initlo=config->initlo*l+config->inithi*velocity;
    struct synth_env_point *dpt=runner->pointv;
    const struct synth_env_point *spt=config->pointv;
    int i=config->pointc;
    for (;i-->0;dpt++,spt++) {
      if ((dpt->tlo=(int)(spt->tlo*l+spt->thi*velocity))<1) dpt->tlo=1;
      dpt->vlo=spt->vlo*l+spt->vhi*velocity;
    }
  }
  
  // If there's a sustain point, the config already has an insertion of it with duration 1.
  // Update duration of that point per durframec, after subtracting the lead duration.
  if (config->flags&SYNTH_ENV_FLAG_SUSTAIN) {
    runner->susp=config->susp;
    int i=config->susp;
    while (i-->0) {
      durframec-=runner->pointv[i].tlo;
    }
    if (durframec>1) runner->pointv[runner->susp].tlo=durframec;
  }
  
  // Set up for the initial leg.
  runner->level=runner->initlo;
  runner->pointp=0;
  if (runner->pointc>0) {
    runner->c=runner->pointv[0].tlo;
    runner->dlevel=(runner->pointv[0].vlo-runner->level)/runner->c;
  } else {
    runner->c=INT_MAX;
    runner->dlevel=0.0f;
  }
  
  //fprintf(stderr,"%s pointc=%d velocity=%f\n",__func__,runner->pointc,velocity);
  //int i=0; for (;i<runner->pointc;i++) fprintf(stderr,"  %d %f\n",runner->pointv[i].tlo,runner->pointv[i].vlo);
}

/* Release.
 */

void synth_env_release(struct synth_env *env) {
  if (!(env->flags&SYNTH_ENV_FLAG_SUSTAIN)) return;
  if (env->pointp<env->susp) {
    env->pointv[env->susp].tlo=1;
  } else if (env->pointp==env->susp) {
    env->c=1;
  }
}

/* Advance.
 */

void synth_env_advance(struct synth_env *env) {
  env->pointp++;
  if (env->pointp>=env->pointc) {
    env->pointp=env->pointc;
    env->c=INT_MAX;
    env->dlevel=0.0f;
    if (env->pointc) env->level=env->pointv[env->pointc-1].vlo;
    else env->level=env->initlo;
  } else {
    env->level=env->pointv[env->pointp-1].vlo;
    env->c=env->pointv[env->pointp].tlo;
    env->dlevel=(env->pointv[env->pointp].vlo-env->level)/(float)env->c;
  }
}

/* How much time remaining?
 */
 
int synth_env_remaining(const struct synth_env *env) {
  int framec=env->c;
  int i=env->pointp+1;
  for (;i<env->pointc;i++) framec+=env->pointv[i].tlo;
  return framec;
}
