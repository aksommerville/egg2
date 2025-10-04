/* eggdev_convert_audio.c
 * Most of the real audio formatting happens in the "eau" unit.
 * We glue it all together and package for eggdev.
 */

#include "eggdev/eggdev_internal.h"
#include "opt/eau/eau.h"
#include "opt/synth/synth.h"

/* Get channel header.
 */
 
static int eggdev_get_chdr(void *dstpp,int fqpid) {
  //TODO
  return 0;
}

/* Eau-to-eggdev adapter.
 */
 
static int eggdev_cvt_eau(
  struct eggdev_convert_context *ctx,
  int (*eaufn)(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg)
) {
  const char *refname=ctx->refname;
  if (ctx->errmsg&&!refname) refname="<EAU>"; // EAU won't log at all without a refname.
  int err=eaufn(ctx->dst,ctx->src,ctx->srcc,refname,eggdev_get_chdr,ctx->flags&EGGDEV_CVTFLAG_STRIP,ctx->errmsg);
  return err;
}

/* Eggdev-style conversion with intermediary.
 */
 
static int eggdev_cvt_intermediary(
  struct eggdev_convert_context *ctx,
  int (*first)(struct eggdev_convert_context *ctx),
  int (*second)(struct eggdev_convert_context *ctx)
) {
  struct sr_encoder tmp={0};
  struct sr_encoder *pvdst=ctx->dst;
  const void *pvsrc=ctx->src;
  int pvsrcc=ctx->srcc;
  ctx->dst=&tmp;
  int err=first(ctx);
  if (err<0) {
    ctx->dst=pvdst;
    sr_encoder_cleanup(&tmp);
    return err;
  }
  ctx->dst=pvdst;
  ctx->src=tmp.v;
  ctx->srcc=tmp.c;
  err=second(ctx);
  sr_encoder_cleanup(&tmp);
  ctx->src=pvsrc;
  ctx->srcc=pvsrcc;
  return err;
}

/* Conversions provided directly by eau unit.
 */

int eggdev_eau_from_eaut(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_eau(ctx,eau_cvt_eau_eaut);
}

int eggdev_eau_from_mid(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_eau(ctx,eau_cvt_eau_midi);
}

int eggdev_eaut_from_eau(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_eau(ctx,eau_cvt_eaut_eau);
}

int eggdev_mid_from_eau(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_eau(ctx,eau_cvt_midi_eau);
}

/* Other conversions, use EAU as an intermediary.
 */

int eggdev_mid_from_eaut(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_intermediary(ctx,eggdev_eau_from_eaut,eggdev_mid_from_eau);
}

int eggdev_eaut_from_mid(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_intermediary(ctx,eggdev_eau_from_mid,eggdev_eaut_from_eau);
}

int eggdev_wav_from_mid(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_intermediary(ctx,eggdev_eau_from_mid,eggdev_wav_from_eau);
}

int eggdev_wav_from_eaut(struct eggdev_convert_context *ctx) {
  return eggdev_cvt_intermediary(ctx,eggdev_eau_from_eaut,eggdev_wav_from_eau);
}

/* WAV from EAU, stand a synthesizer and yoink its output.
 */

int eggdev_wav_from_eau(struct eggdev_convert_context *ctx) {

  int rate=ctx->rate;
  int chanc=ctx->chanc;
  if ((rate<1)||(chanc<1)) {
    rate=44100;
    chanc=2;
  }
  int method=EAU_DURATION_METHOD_DEFAULT; // Let eau decide (it will pick ROUND_UP).
  
  // Take some measurements and allocate the PCM buffer.
  int durms=eau_calculate_duration(ctx->src,ctx->srcc,method);
  int framec=(int)(((double)durms*(double)rate)/1000.0);
  if (framec<1) framec=1; // Empty is fine for the song, but do produce some nonzero amount of silence.
  int samplec=framec*chanc;
  int16_t *samplev=malloc(sizeof(int16_t)*samplec);
  if (!samplev) return -1;
  
  // Stand the synthesizer and run it.
  int err=0;
  struct synth *synth=synth_new(rate,chanc);
  if (!synth) {
    free(samplev);
    return eggdev_convert_error(ctx,"Failed to create synthesizer, rate=%d, chanc=%d.",rate,chanc);
  }
  if (synth_install_song(synth,1,ctx->src,ctx->srcc)<0) {
    synth_del(synth);
    free(samplev);
    return eggdev_convert_error(ctx,"Failed to install song in temporary synthesizer.");
  }
  synth_play_song(synth,1,0,0);
  if (!synth_get_songid(synth)) {
    synth_del(synth);
    free(samplev);
    return eggdev_convert_error(ctx,"Failed to start song. Likely misencoded.\n");
  }
  synth_updatei(samplev,samplec,synth); // <-- All the interesting work happens here.
  synth_del(synth);
  
  // Emit WAV header.
  int headersize=44; // From the RIFF header thru the "data" introducer, ie everything except the PCM.
  int filesize=headersize-8+samplec*2;
  int datasize=samplec*2;
  int byterate=rate*chanc*2;
  sr_encode_raw(ctx->dst,"RIFF",4);
  sr_encode_intle(ctx->dst,filesize,4);
  sr_encode_raw(ctx->dst,"WAVEfmt \x10\0\0\0",12); // Begin "fmt ", length 16.
  sr_encode_intle(ctx->dst,1,2); // AudioFormat=LPCM
  sr_encode_intle(ctx->dst,chanc,2);
  sr_encode_intle(ctx->dst,rate,4);
  sr_encode_intle(ctx->dst,byterate,4);
  sr_encode_intle(ctx->dst,2*chanc,2); // bytes per frame
  sr_encode_intle(ctx->dst,16,2); // sample size, bits
  sr_encode_raw(ctx->dst,"data",4);
  sr_encode_intle(ctx->dst,datasize,4);
  
  // Emit samples. s16 little-endian twos-complement interleaved, ie exactly the way we have them if the host is little-endian.
  // Since little-endian hosts are likely (100% likely), copy it straight, and then make an exception for the big-endian hosts that don't exist.
  sr_encode_raw(ctx->dst,samplev,datasize);
  uint32_t bodetect=0x44332211;
  if (*(uint8_t*)&bodetect==0x44) {
    uint8_t *p=((uint8_t*)ctx->dst->v)+ctx->dst->c;
    int i=samplec;
    while (i-->0) {
      p-=2;
      uint8_t tmp=p[0];
      p[0]=p[1];
      p[1]=tmp;
    }
  }
  
  free(samplev);
  return sr_encoder_assert(ctx->dst);
}
