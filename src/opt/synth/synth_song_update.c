#include "synth_internal.h"

/* Update song's signal graph, main entry point.
 * Add to (v).
 * Return >0 to stay alive.
 */
 
static void voice_update(float *v,int framec,struct synth_voice *voice,struct synth_song *song) {//XXX
  int updc=voice->ttl;
  if (updc>framec) updc=framec;
  voice->ttl-=updc;
  if (song->synth->chanc>1) {
    for (;updc-->0;v+=song->synth->chanc) {
      voice->p+=voice->dp;
      if (voice->p&0x80000000) {
        v[0]+=voice->level;
        v[1]+=voice->level;
      } else {
        v[0]-=voice->level;
        v[1]-=voice->level;
      }
    }
  } else {
    for (;updc-->0;v++) {
      voice->p+=voice->dp;
      if (voice->p&0x80000000) (*v)+=voice->level;
      else (*v)-=voice->level;
    }
  }
}
  
int synth_song_update(float *v,int framec,struct synth_song *song) {
  if ((song->delay-=framec)<0) song->delay=0;
  song->phframes+=framec;
  
  //XXX very temp
  struct synth_voice *voice=song->voicev;
  int i=song->voicec;
  for (;i-->0;voice++) {
    voice_update(v,framec,voice,song);
  }
  while (song->voicec&&(song->voicev[song->voicec-1].ttl<=0)) song->voicec--;
  
  return 1;
}
