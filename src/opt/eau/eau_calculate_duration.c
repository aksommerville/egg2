#include "eau.h"
#include <stdlib.h>

/* DELAY method, dead simple.
 */
 
static int eau_calculate_duration_DELAY(struct eau_file *file) {
  int dur=0;
  struct eau_event_reader reader={.v=file->eventv,.c=file->eventc};
  struct eau_event event;
  while (eau_event_reader_next(&event,&reader)>0) {
    if (event.type=='d') dur+=event.delay;
  }
  return dur;
}

/* RELEASE method. Pretty simple. We only need to look at events.
 */
 
static int eau_calculate_duration_RELEASE(struct eau_file *file) {
  int dur=0,delaysum=0;
  struct eau_event_reader reader={.v=file->eventv,.c=file->eventc};
  struct eau_event event;
  while (eau_event_reader_next(&event,&reader)>0) {
    switch (event.type) {
      case 'd': delaysum+=event.delay; break;
      case 'n': {
          int t=delaysum+event.note.durms;
          if (t>dur) dur=t;
        } break;
    }
  }
  return (dur>delaysum)?dur:delaysum;
}

/* ROUND_UP method. Basically RELEASE, but also force to fit the tempo.
 */
 
static int eau_calculate_duration_ROUND_UP(struct eau_file *file) {
  int dur=eau_calculate_duration_RELEASE(file);
  int mod=dur%file->tempo;
  if (mod<1) return dur;
  return dur+file->tempo-mod;
}

/* Channel mode specific preprocessing for VOICE_TAIL mode.
 */
 
struct envtiming {
  int prelo,prehi; // Sum of leg durations before sustain.
  int postlo,posthi; // '' after sustain, or zero if not sustainable.
  int *drumtimes; // If not null, 128 ints. DRUM channels only. (a drum note has a fixed duration).
};

static int eau_calculate_envtiming_DRUM(struct envtiming *envtiming,const uint8_t *src,int srcc) {
  int srcp=0;
  for (;;) {
    if (srcp>=srcc) break;
    if (srcp>srcc-6) return -1;
    uint8_t noteid=src[srcp++];
    srcp+=3; // trim and pan don't matter
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return -1;
    if (noteid>=0x80) { srcp+=len; continue; }
    int dur=eau_calculate_duration(src+srcp,len,EAU_DURATION_METHOD_VOICE_TAIL);
    srcp+=len;
    if (dur<1) continue;
    if (!envtiming->drumtimes) {
      if (!(envtiming->drumtimes=calloc(sizeof(int),128))) return -1;
    }
    envtiming->drumtimes[noteid]=dur;
  }
  return 0;
}

static int eau_calculate_envtiming_env(struct envtiming *envtiming,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  int srcp=1;
  uint8_t flags=src[0];
  if (flags&0x01) { // We don't care about the initial levels, just skip them.
    srcp+=2;
    if (flags&0x02) srcp+=2;
  }
  if (srcp>=srcc) return -1;
  uint8_t susp_ptc=src[srcp++];
  int susp=susp_ptc>>4;
  if (!(flags&0x04)) susp=256;
  int ptc=susp_ptc&15;
  int ptlen=(flags&0x02)?8:4;
  if (srcp>srcc-ptc*ptlen) return -1;
  int i=0;
  for (;i<ptc;i++) {
    int tlo=(src[srcp]<<8)|src[srcp+1];
    if (tlo<1) tlo=1;
    srcp+=4; // skip level
    int thi=tlo;
    if (flags&0x02) {
      thi=(src[srcp]<<8)|src[srcp+1];
      if (thi<1) thi=1;
      srcp+=4; // skip level
    }
    if (i>susp) { // strictly greater than, not equal
      envtiming->postlo+=tlo;
      envtiming->posthi+=thi;
    } else {
      envtiming->prelo+=tlo;
      envtiming->prehi+=thi;
    }
  }
  return 0;
}

static int eau_calculate_envtiming_FM(struct envtiming *envtiming,const uint8_t *src,int srcc) {
  if (srcc<4) return 0;
  return eau_calculate_envtiming_env(envtiming,src+4,srcc-4);
}

static int eau_calculate_envtiming_HARSH(struct envtiming *envtiming,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  return eau_calculate_envtiming_env(envtiming,src+1,srcc-1);
}

static int eau_calculate_envtiming_HARM(struct envtiming *envtiming,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  int harmc=src[0];
  int srcp=1;
  if (srcp>srcc-harmc*2) return -1;
  srcp+=harmc*2;
  return eau_calculate_envtiming_env(envtiming,src+srcp,srcc-srcp);
}

/* VOICE_TAIL method. Expert mode.
 */
 
static int eau_calculate_duration_VOICE_TAIL(struct eau_file *file) {

  /* First summarize the timing of each channel's level envelope.
   */
  struct envtiming envtimingv[16]={0};
  struct envtiming *envtiming=envtimingv;
  struct eau_file_channel *channel=file->channelv;
  int chid=0;
  for (;chid<file->channelc;chid++,envtiming++,channel++) {
    switch (channel->mode) {
      case 1: eau_calculate_envtiming_DRUM(envtiming,channel->modecfg,channel->modecfgc); break;
      case 2: eau_calculate_envtiming_FM(envtiming,channel->modecfg,channel->modecfgc); break;
      case 3: eau_calculate_envtiming_HARSH(envtiming,channel->modecfg,channel->modecfgc); break;
      case 4: eau_calculate_envtiming_HARM(envtiming,channel->modecfg,channel->modecfgc); break;
    }
  }
  
  /* Now do it like RELEASE, but our high watermark accounts for these envelope timings.
   */
  int delaysum=0,dur=0;
  struct eau_event_reader reader={.v=file->eventv,.c=file->eventc};
  struct eau_event event;
  while (eau_event_reader_next(&event,&reader)>0) {
    switch (event.type) {
      case 'd': delaysum+=event.delay; break;
      case 'n': {
          if (event.note.chid>=0x10) break;
          if (event.note.noteid>=0x80) break;
          envtiming=envtimingv+event.note.chid;
          int invvel=0x7f-event.note.velocity;
          int endtime=delaysum;
          if (envtiming->drumtimes) { // drum?
            endtime+=envtiming->drumtimes[event.note.noteid];
          } else if (envtiming->postlo) { // sustain? Use the longer of (pre,sustain).
            int pretime=(envtiming->prelo*invvel+envtiming->prehi*event.note.velocity+127)>>7; // +127=ceiling
            if (pretime<event.note.durms) pretime=event.note.durms;
            endtime+=pretime;
            endtime+=(envtiming->postlo*invvel+envtiming->posthi*event.note.velocity+127)>>7;
          } else { // pre only
            endtime+=(envtiming->prelo*invvel+envtiming->prehi*event.note.velocity+127)>>7;
          }
          if (endtime>dur) dur=endtime;
        } break;
    }
  }
  
  // Delete drumtimes, then report  the result.
  int i=file->channelc;
  for (envtiming=envtimingv;i-->0;envtiming++) {
    if (envtiming->drumtimes) free(envtiming->drumtimes);
  }
  return (delaysum>dur)?delaysum:dur;
}

/* Calculate duration, main entry point.
 */
 
int eau_calculate_duration(const void *src,int srcc,int method) {
  if (method==EAU_DURATION_METHOD_DEFAULT) method=EAU_DURATION_METHOD_ROUND_UP;
  struct eau_file file={0};
  if (eau_file_decode(&file,src,srcc)<0) return 0;
  switch (method) {
    case EAU_DURATION_METHOD_DELAY: return eau_calculate_duration_DELAY(&file);
    case EAU_DURATION_METHOD_RELEASE: return eau_calculate_duration_RELEASE(&file);
    case EAU_DURATION_METHOD_VOICE_TAIL: return eau_calculate_duration_VOICE_TAIL(&file);
    case EAU_DURATION_METHOD_ROUND_UP: return eau_calculate_duration_ROUND_UP(&file);
  }
  return 0;
}
