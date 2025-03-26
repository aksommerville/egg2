/* synth_stage.h
 * Single stage of the post pipe.
 */
 
#ifndef SYNTH_STAGE_H
#define SYNTH_STAGE_H

/* Generic stage.
 *****************************************************************************/

struct synth_stage {
  struct synth *synth; // WEAK
  int chanc;
  void (*del)(struct synth_stage *stage);
  void (*update)(float *v,int framec,struct synth_stage *stage);
};

void synth_stage_del(struct synth_stage *stage);

struct synth_stage *synth_stage_new(struct synth *synth,int chanc,int stageid,const void *src,int srcc);

void synth_stage_update(float *v,int framec,struct synth_stage *stage);

/* Specific implementations.
 ***************************************************************************/
 
struct synth_stage_gain {
  struct synth_stage hdr;
  float gain,clip;
};

int synth_stage_gain_init(struct synth_stage *stage,const uint8_t *src,int srcc);

struct synth_stage_delay {
  struct synth_stage hdr;
  struct synth_ring ring;
  float dry,wet,sto,fbk;
};

int synth_stage_delay_init(struct synth_stage *stage,const uint8_t *src,int srcc);

struct synth_stage_iir {
  struct synth_stage hdr;
  struct synth_iir3 iir;
  struct synth_iir3 r;
};

int synth_stage_lopass_init(struct synth_stage *stage,const uint8_t *src,int srcc);
int synth_stage_hipass_init(struct synth_stage *stage,const uint8_t *src,int srcc);
int synth_stage_bpass_init(struct synth_stage *stage,const uint8_t *src,int srcc);
int synth_stage_notch_init(struct synth_stage *stage,const uint8_t *src,int srcc);

struct synth_stage_waveshaper {
  struct synth_stage hdr;
  float *map; // Entire range -1..1
  int mapc;
};

int synth_stage_waveshaper_init(struct synth_stage *stage,const uint8_t *src,int srcc);

#endif
