#include "synth_internal.h"

/* Rephrase all leg delays as frames, from milliseconds.
 */
 
static void synth_env_frames_from_ms(struct synth_env *env) {
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    if ((point->tlo=synth_frames_from_ms(point->tlo))<1) point->tlo=1;
    if ((point->thi=synth_frames_from_ms(point->thi))<1) point->thi=1;
  }
}

/* Fallback configs.
 */
 
static void synth_env_fallback(struct synth_env *env,int fallback) {
  __builtin_memset(env,0,sizeof(struct synth_env));
  switch (fallback) {
    case SYNTH_ENV_FALLBACK_ZERO: env->initlo=env->inithi=0.0f; break;
    case SYNTH_ENV_FALLBACK_ONE: env->flags=SYNTH_ENV_INITIALS; env->initlo=env->inithi=1.0f; break;
    case SYNTH_ENV_FALLBACK_HALF: env->flags=SYNTH_ENV_INITIALS; env->initlo=env->inithi=0.5f; break;
    case SYNTH_ENV_FALLBACK_LEVEL: {
        env->flags=SYNTH_ENV_VELOCITY|SYNTH_ENV_SUSTAIN;
        env->susp=2;
        env->pointc=4;
        // Points in ms initially for reference. (tlo,thi,vlo,vhi)
        env->pointv[0]=(struct synth_env_point){  30, 20,0.500f,1.000f };
        env->pointv[1]=(struct synth_env_point){  20, 20,0.250f,0.400f };
        env->pointv[2]=(struct synth_env_point){   0,  0,0.250f,0.400f }; // sustain
        env->pointv[3]=(struct synth_env_point){ 150,300,0.000f,0.000f };
        synth_env_frames_from_ms(env);
      } break;
  }
}

/* Decode config.
 */
 
int synth_env_decode(struct synth_env *env,const void *src,int srcc,int fallback) {
  const uint8_t *SRC=src;
  if (!src||(srcc<1)||!SRC[0]) {
    synth_env_fallback(env,fallback);
    return (srcc>=1)?1:0;
  }
  env->flags=SRC[0];
  if (env->flags&0xf0) return -1; // Reserved flags must be zero.
  env->flags|=SYNTH_ENV_PRESENT; // Non-default envelopes get PRESENT whether they specified or not.
  int srcp=1;
  
  // Initials?
  if (env->flags&SYNTH_ENV_INITIALS) {
    if (srcp>srcc-2) return -1;
    env->initlo=((SRC[srcp]<<8)|SRC[srcp+1])/65536.0f;
    srcp+=2;
    if (env->flags&SYNTH_ENV_VELOCITY) {
      if (srcp>srcc-2) return -1;
      env->inithi=((SRC[srcp]<<8)|SRC[srcp+1])/65536.0f;
      srcp+=2;
    } else {
      env->inithi=env->initlo;
    }
  } else {
    env->initlo=env->inithi=0.0f;
  }
  
  // Sustain index and point count.
  if (srcp>=srcc) return -1;
  if (env->flags&SYNTH_ENV_SUSTAIN) {
    env->susp=SRC[srcp]>>4;
  } else {
    env->susp=-1;
  }
  env->pointc=SRC[srcp]&15;
  srcp++;
  int pointlen=(env->flags&SYNTH_ENV_VELOCITY)?8:4;
  if (srcp>srcc-env->pointc*pointlen) return -1;
  if (env->flags&SYNTH_ENV_SUSTAIN) {
    if (env->susp>=env->pointc) return -1;
    env->pointc++;
    env->susp++;
  }
  
  // Points.
  struct synth_env_point *point=env->pointv;
  int pointi=0;
  for (;pointi<env->pointc;pointi++,point++) {
    if (pointi==env->susp) {
      point[0]=point[-1];
      point->tlo=1;
      point->thi=1;
    } else {
      point->tlo=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
      point->vlo=((SRC[srcp]<<8)|SRC[srcp+1])/65536.0f; srcp+=2;
      if (env->flags&SYNTH_ENV_VELOCITY) {
        point->thi=(SRC[srcp]<<8)|SRC[srcp+1]; srcp+=2;
        point->vhi=((SRC[srcp]<<8)|SRC[srcp+1])/65536.0f; srcp+=2;
      }
    }
  }
  synth_env_frames_from_ms(env);
  
  return srcp;
}

/* Adjust config.
 */
 
void synth_env_mlt(struct synth_env *env,float mlt) {
  env->initlo*=mlt;
  env->inithi*=mlt;
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->vlo*=mlt;
    point->vhi*=mlt;
  }
}

void synth_env_add(struct synth_env *env,float add) {
  env->initlo+=add;
  env->inithi+=add;
  struct synth_env_point *point=env->pointv;
  int i=env->pointc;
  for (;i-->0;point++) {
    point->vlo+=add;
    point->vhi+=add;
  }
}

/* Initialize runner from config.
 */

void synth_env_apply(struct synth_env *runner,const struct synth_env *config,float velocity,int durframes) {
  runner->flags=config->flags;
  runner->pointc=config->pointc;
  runner->susp=config->susp;
  
  // Calculate the velocity-dependent things.
  struct synth_env_point *rpt=runner->pointv;
  const struct synth_env_point *cpt=config->pointv;
  int i=config->pointc;
  if (!(config->flags&SYNTH_ENV_VELOCITY)||(velocity<=0.0f)) {
    runner->initlo=config->initlo;
    for (;i-->0;rpt++,cpt++) {
      rpt->tlo=cpt->tlo;
      rpt->vlo=cpt->vlo;
    }
  } else if (velocity>=1.0f) {
    runner->initlo=config->inithi;
    for (;i-->0;rpt++,cpt++) {
      rpt->tlo=cpt->thi;
      rpt->vlo=cpt->vhi;
    }
  } else {
    float loweight=1.0f-velocity;
    runner->initlo=config->initlo*loweight+config->inithi*velocity;
    for (;i-->0;rpt++,cpt++) {
      rpt->tlo=(int)((float)cpt->tlo*loweight+(float)cpt->thi*velocity);
      rpt->vlo=cpt->vlo*loweight+cpt->vhi*velocity;
    }
  }
  
  // Fill in sustain time.
  if (runner->flags&SYNTH_ENV_SUSTAIN) {
    for (rpt=runner->pointv,i=runner->susp;i-->0;rpt++) durframes-=rpt->tlo;
    rpt=runner->pointv+runner->susp;
    if (durframes>rpt->tlo) rpt->tlo=durframes;
  }
  
  // Start iteration.
  runner->v=runner->initlo;
  if (runner->pointc>0) {
    runner->finished=0;
    runner->pointp=0;
    rpt=runner->pointv;
    runner->c=rpt->tlo;
    runner->dv=(rpt->vlo-runner->v)/runner->c;
  } else {
    runner->finished=1;
    runner->c=INT_MAX;
    runner->dv=0.0f;
  }
}

/* Release runner.
 */

void synth_env_release(struct synth_env *env) {
  if (!(env->flags&SYNTH_ENV_SUSTAIN)) return;
  if (env->pointp>env->susp) return; // Already done sustaining.
  if (env->pointp<env->susp) { // Haven't reached sustain leg yet. Great, drop it to one frame.
    env->pointv[env->susp].tlo=1;
    return;
  }
  // We're in the middle of the sustain leg. Drop the current ttl to one.
  env->c=1;
}

/* Advance runner to next point.
 */
 
void synth_env_advance(struct synth_env *env) {
  env->pointp++;
  if (env->finished||(env->pointp>=env->pointc)) {
    env->finished=1;
    env->pointp=env->pointc;
    env->dv=0.0f;
    env->c=INT_MAX;
  } else {
    struct synth_env_point *dst=env->pointv+env->pointp;
    env->c=dst->tlo;
    env->dv=(dst->vlo-env->v)/env->c;
  }
}
