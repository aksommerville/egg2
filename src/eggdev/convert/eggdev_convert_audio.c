/* eggdev_convert_audio.c
 * Most of the real audio formatting happens in the "eau" unit.
 * We glue it all together and package for eggdev.
 */

#include "eggdev/eggdev_internal.h"
#include "opt/eau/eau.h"
#include "opt/synth/synth.h"

/* Get channel header.
 */
 
int eggdev_get_chdr(void *dstpp,int fqpid) {
  if ((fqpid<0)||(fqpid>0xff)) return 0;
  const void *eau=0;
  int eauc=eggdev_config_get_instruments(&eau);
  if (eauc<1) return 0;
  struct eau_file eau_file;
  if (eau_file_decode(&eau_file,eau,eauc)<0) return 0;
  struct eau_chdr_reader reader={.v=eau_file.chdr,.c=eau_file.chdrc};
  struct eau_chdr_entry entry;
  while (eau_chdr_reader_next(&entry,&reader)>0) {
    if (entry.chid==fqpid) {
      *(const void**)dstpp=entry.v;
      return entry.c;
    }
  }
  return 0;
}

/* Eggdev-style conversion with intermediary.
 */
 
static int eggdev_cvt_intermediary(
  struct sr_convert_context *ctx,
  int (*first)(struct sr_convert_context *ctx),
  int (*second)(struct sr_convert_context *ctx)
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

/* Other conversions, use EAU as an intermediary.
 */

int eggdev_wav_from_mid(struct sr_convert_context *ctx) {
  return eggdev_cvt_intermediary(ctx,eau_cvt_eau_midi,eggdev_wav_from_eau);
}

/* Wrap this EAU song in a fake ROM and install in the synthesizer.
 * It will have id song:1.
 */
 
static int eggdev_synth_install_song(const void *src,int srcc) {
  const int pfxlen=3;
  if ((srcc<1)||(srcc>0x400000)) return -1;
  uint8_t *rom=synth_get_rom(pfxlen+srcc);
  if (!rom) return -1;
  rom[0]=0x80|((srcc-1)>>16);
  rom[1]=(srcc-1)>>8;
  rom[2]=srcc-1;
  memcpy(rom+pfxlen,src,srcc);
  return 0;
}

/* Quantize and interleave from synth's split float buffers into WAV's interleaved s16 buffer.
 * Appends directly to (dst).
 */
 
static void eggdev_synth_emit_mono(struct sr_encoder *dst,const float *l,int framec) {
  if (sr_encoder_require(dst,framec*2)<0) return;
  for (;framec-->0;l++) {
    int sample=(*l<=-1.0f)?-32768:(*l>=1.0f)?32767:(int)((*l)*32767.0f);
    ((uint8_t*)dst->v)[dst->c++]=sample;
    ((uint8_t*)dst->v)[dst->c++]=sample>>8;
  }
}
 
static void eggdev_synth_emit_stereo(struct sr_encoder *dst,const float *l,const float *r,int framec) {
  if (sr_encoder_require(dst,framec*4)<0) return;
  for (;framec-->0;l++,r++) {
    int sample=(*l<=-1.0f)?-32768:(*l>=1.0f)?32767:(int)((*l)*32767.0f);
    ((uint8_t*)dst->v)[dst->c++]=sample;
    ((uint8_t*)dst->v)[dst->c++]=sample>>8;
    sample=(*r<=-1.0f)?-32768:(*r>=1.0f)?32767:(int)((*r)*32767.0f);
    ((uint8_t*)dst->v)[dst->c++]=sample;
    ((uint8_t*)dst->v)[dst->c++]=sample>>8;
  }
}

/* WAV from EAU, stand a synthesizer and yoink its output.
 */

int eggdev_wav_from_eau(struct sr_convert_context *ctx) {

  int rate,chanc;
  if (sr_convert_arg_int(&rate,ctx,"rate",4)<0) rate=44100;
  if (sr_convert_arg_int(&chanc,ctx,"chanc",5)<0) chanc=2;
  if ((rate<1)||(chanc<1)) {
    rate=44100;
    chanc=2;
  }
  if (rate>200000) rate=200000;
  if (chanc>2) chanc=2; // Please be reasonable! Synth won't produce more than 2.
  const int buffer_frames=2048;
  int framec_panic=rate*60*60; // Abort after generating one hour of PCM; if a song really is that long we don't want anything to do with it.
  
  // Stand the synthesizer and load our song.
  int err=0;
  if (synth_init(rate,chanc,buffer_frames)<0) {
    return sr_convert_error(ctx,"Failed to create synthesizer, rate=%d, chanc=%d.",rate,chanc);
  }
  float *bufl=synth_get_buffer(0);
  float *bufr=synth_get_buffer(1);
  if (!bufl||((chanc>=2)&&!bufr)) {
    synth_quit();
    return sr_convert_error(ctx,"Failed to acquire synth buffers.");
  }
  if (eggdev_synth_install_song(ctx->src,ctx->srcc)<0) {
    synth_quit();
    return sr_convert_error(ctx,"Failed to install song in temporary synthesizer.");
  }
  synth_play_song(1,1,0,1.0f,0.0f);
  if (synth_get(1,0xff,SYNTH_PROP_EXISTENCE)<1.0f) {
    synth_quit();
    return sr_convert_error(ctx,"Failed to start song. Likely misencoded.");
  }
  
  // Emit WAV header.
  int headersize=44; // From the RIFF header thru the "data" introducer, ie everything except the PCM.
  int byterate=rate*chanc*2;
  sr_encode_raw(ctx->dst,"RIFF",4);
  int flenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0\0\0",4); // flen placeholder
  sr_encode_raw(ctx->dst,"WAVEfmt \x10\0\0\0",12); // Begin "fmt ", length 16.
  sr_encode_intle(ctx->dst,1,2); // AudioFormat=LPCM
  sr_encode_intle(ctx->dst,chanc,2);
  sr_encode_intle(ctx->dst,rate,4);
  sr_encode_intle(ctx->dst,byterate,4);
  sr_encode_intle(ctx->dst,2*chanc,2); // bytes per frame
  sr_encode_intle(ctx->dst,16,2); // sample size, bits
  sr_encode_raw(ctx->dst,"data",4);
  int dlenp=ctx->dst->c;
  sr_encode_raw(ctx->dst,"\0\0\0\0",4); // dlen placeholder
  
  // Run until it reports completion, collecting s16 PCM into the output.
  int framec_total=0;
  for (;;) {
    synth_update(buffer_frames);
    if (chanc==1) {
      eggdev_synth_emit_mono(ctx->dst,bufl,buffer_frames);
    } else {
      eggdev_synth_emit_stereo(ctx->dst,bufl,bufr,buffer_frames);
    }
    if (synth_get(1,0xff,SYNTH_PROP_EXISTENCE)<1.0f) break; // Stopped on its own, great.
    framec_total+=buffer_frames;
    if (framec_total>framec_panic) {
      synth_quit();
      return sr_convert_error(ctx,"Panic! Song has not completed after %d frames.",framec_total);
    }
  }
  synth_quit();
  
  // Eliminate silent frames from the tail. We update in blocks, so we've surely produced a substantial amount of silence.
  int framelen=2*chanc; // Size of a frame in bytes.
  int floorp=dlenp+4+framelen; // Just after the first frame. We keep one frame even if it's silent.
  while (ctx->dst->c>floorp) {
    int silent=1;
    int i=framelen;
    const uint8_t *q=((uint8_t*)ctx->dst->v)+ctx->dst->c-1;
    for (;i-->0;q--) if (*q) { silent=0; break; }
    if (!silent) break;
    ctx->dst->c-=framelen;
  }
  
  // Fill in the two lengths.
  int flen=ctx->dst->c-flenp-4;
  int dlen=ctx->dst->c-dlenp-4;
  if ((flen<1)||(dlen<1)) return sr_convert_error(ctx,"Invalid file lengths after truncation.");
  ((uint8_t*)ctx->dst->v)[flenp+0]=flen;
  ((uint8_t*)ctx->dst->v)[flenp+1]=flen>>8;
  ((uint8_t*)ctx->dst->v)[flenp+2]=flen>>16;
  ((uint8_t*)ctx->dst->v)[flenp+3]=flen>>24;
  ((uint8_t*)ctx->dst->v)[dlenp+0]=dlen;
  ((uint8_t*)ctx->dst->v)[dlenp+1]=dlen>>8;
  ((uint8_t*)ctx->dst->v)[dlenp+2]=dlen>>16;
  ((uint8_t*)ctx->dst->v)[dlenp+3]=dlen>>24;
  
  return sr_encoder_assert(ctx->dst);
}
