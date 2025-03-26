/* eggdev_eaut_from_eau.c
 * I don't expect ever to need this, it's here mostly for the sake of symmetry.
 * But who knows, maybe there will come a time when you want a MIDI file in legible form?
 */

#include "eggdev/eggdev_internal.h"
#include "opt/synth/eau.h"

/* Globals block.
 */
 
static int eggdev_eaut_generate_globals(struct eggdev_convert_context *ctx,struct eau_file *file) {
  return sr_encode_fmt(ctx->dst,"globals {\ntempo %d}\n",file->tempo);
}

/* Envelope field.
 */
 
static int eggdev_eaut_env(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  int srcp=0;
  if (srcp>=srcc) return 0;
  uint8_t flags=src[srcp++];
  
  uint16_t initlo=0,inithi=0;
  if (flags&0x02) {
    if (srcp>srcc-2) return -1;
    initlo=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (flags&0x01) {
      if (srcp>srcc-2) return -1;
      inithi=(src[srcp]<<8)|src[srcp+1];
      srcp+=2;
    } else {
      inithi=initlo;
    }
  }
  
  int susp=256;
  if (flags&0x04) {
    if (srcp>=srcc) return -1;
    susp=src[srcp++];
  }
  
  if (srcp>=srcc) return -1;
  int ptc=src[srcp++];
  int ptlen=(flags&0x01)?8:4;
  if (srcp>srcc-ptc*ptlen) return -1;
  
  // It's valid. Begin output.
  if (sr_encode_fmt(ctx->dst,"%s 0x%04x",fldname,initlo)<0) return -1;
  if (initlo!=inithi) {
    if (sr_encode_fmt(ctx->dst,"..0x%04x",inithi)<0) return -1;
  }
  int ptp=0;
  for (;ptp<ptc;ptp++) {
    int tlo=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    int vlo=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    int thi=tlo,vhi=vlo;
    if (flags&0x01) {
      thi=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
      vhi=(src[srcp]<<8)|src[srcp+1]; srcp+=2;
    }
    if (sr_encode_fmt(ctx->dst," %d",tlo)<0) return -1;
    if (tlo!=thi) {
      if (sr_encode_fmt(ctx->dst,"..%d",thi)<0) return -1;
    }
    if (sr_encode_fmt(ctx->dst," 0x%04x",vlo)<0) return -1;
    if (vlo!=vhi) {
      if (sr_encode_fmt(ctx->dst,"0x%04x",vhi)<0) return -1;
    }
    if (ptp==susp) {
      if (sr_encode_u8(ctx->dst,'*')<0) return -1;
    }
  }
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Wave field.
 */
 
static int eggdev_eaut_wave(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (srcc<3) return eggdev_convert_error(ctx,"Malformed EAU wave.");
  int harmc=src[2];
  if (srcc<3+harmc*2) return eggdev_convert_error(ctx,"Malformed EAU wave.");
  int err;
  switch (src[0]) {
    case EAU_SHAPE_SINE: err=sr_encode_fmt(ctx->dst,"%s sine",fldname); break;
    case EAU_SHAPE_SQUARE: err=sr_encode_fmt(ctx->dst,"%s square",fldname); break;
    case EAU_SHAPE_SAW: err=sr_encode_fmt(ctx->dst,"%s saw",fldname); break;
    case EAU_SHAPE_TRIANGLE: err=sr_encode_fmt(ctx->dst,"%s triangle",fldname); break;
    case EAU_SHAPE_FIXEDFM: err=sr_encode_fmt(ctx->dst,"%s fixedfm",fldname); break;
    default: err=sr_encode_fmt(ctx->dst,"%s %d",fldname,src[0]); break;
  }
  if (err<0) return -1;
  if (src[1]) {
    if (sr_encode_fmt(ctx->dst," +%d",src[1])<0) return -1;
  }
  int srcp=3;
  for (;harmc-->0;srcp+=2) {
    if (sr_encode_fmt(ctx->dst," 0x%04x",(src[srcp]<<8)|src[srcp+1])<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Typed helpers for scalar fields.
 */
 
static int eggdev_eaut_u8(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (sr_encode_fmt(ctx->dst,"%s %d\n",fldname,src[0])<0) return -1;
  return 1;
}
 
static int eggdev_eaut_u16(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (srcc<2) return -1;
  if (sr_encode_fmt(ctx->dst,"%s %d\n",fldname,(src[0]<<8)|src[1])<0) return -1;
  return 2;
}
 
static int eggdev_eaut_u16_pair(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (srcc<4) return -1;
  if (sr_encode_fmt(ctx->dst,"%s %d %d\n",fldname,(src[0]<<8)|src[1],(src[2]<<8)|src[3])<0) return -1;
  return 2;
}
 
static int eggdev_eaut_u0_8(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (sr_encode_fmt(ctx->dst,"%s %f\n",fldname,src[0]/255.0f)<0) return -1;
  return 1;
}
 
static int eggdev_eaut_u8_8(struct eggdev_convert_context *ctx,const char *fldname,const uint8_t *src,int srcc) {
  if (srcc<1) return 0;
  if (srcc<2) return -1;
  if (sr_encode_fmt(ctx->dst,"%s %f\n",fldname,((src[0]<<8)|src[1])/256.0f)<0) return -1;
  return 2;
}

/* Channel-specific fields: DRUM
 */
 
static int eggdev_eaut_generate_chhdr_drum(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0;
  while (srcp<srcc) {
    if (srcp>srcc-6) return eggdev_convert_error(ctx,"Malformed EAU drum config.");
    uint8_t noteid=src[srcp++];
    uint8_t trimlo=src[srcp++];
    uint8_t trimhi=src[srcp++];
    uint8_t pan=src[srcp++];
    int len=(src[srcp]<<8)|src[srcp+1];
    srcp+=2;
    if (srcp>srcc-len) return eggdev_convert_error(ctx,"Malformed EAU drum config.");
    if (sr_encode_fmt(ctx->dst,"note %d %d %d %d {\n",noteid,trimlo,trimhi,pan)<0) return -1;
    struct eggdev_convert_context subctx=*ctx;
    subctx.src=src+srcp;
    subctx.srcc=len;
    int err=eggdev_eaut_from_eau(&subctx);
    if (err<0) return err;
    srcp+=len;
    if (sr_encode_raw(ctx->dst,"}\n",2)<0) return -1;
  }
  return 0;
}

/* Channel-specific fields: FM
 */
 
static int eggdev_eaut_generate_chhdr_fm(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0,err;
  #define NEXT(fn,fldname) { \
    if (srcp>=srcc) return 0; \
    if ((err=fn(ctx,fldname,src+srcp,srcc-srcp))<0) return err; \
    srcp+=err; \
  }
  NEXT(eggdev_eaut_env,"level")
  NEXT(eggdev_eaut_wave,"wave")
  NEXT(eggdev_eaut_env,"pitchenv")
  NEXT(eggdev_eaut_u16,"wheel")
  NEXT(eggdev_eaut_u8_8,"rate")
  NEXT(eggdev_eaut_u8_8,"range")
  NEXT(eggdev_eaut_env,"rangeenv")
  NEXT(eggdev_eaut_u8_8,"rangelforate")
  NEXT(eggdev_eaut_u0_8,"rangelfodepth")
  #undef NEXT
  return 0;
}

/* Channel-specific fields: SUB
 */
 
static int eggdev_eaut_generate_chhdr_sub(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  int srcp=0,err;
  #define NEXT(fn,fldname) { \
    if (srcp>=srcc) return 0; \
    if ((err=fn(ctx,fldname,src+srcp,srcc-srcp))<0) return err; \
    srcp+=err; \
  }
  NEXT(eggdev_eaut_env,"level")
  NEXT(eggdev_eaut_u16_pair,"width")
  NEXT(eggdev_eaut_u8,"stagec")
  NEXT(eggdev_eaut_u8_8,"gain")
  #undef NEXT
  return 0;
}

/* Channel-specific fields: Unknown mode.
 */
 
static int eggdev_eaut_generate_chhdr_generic(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (sr_encode_raw(ctx->dst,"modecfg ",8)<0) return -1;
  for (;srcc-->0;src++) {
    if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*src)>>4])<0) return -1;
    if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*src)&15])<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Post stage: Generic
 */
 
static int eggdev_eaut_generate_post_generic(struct eggdev_convert_context *ctx,uint8_t stageid,const uint8_t *src,int srcc) {
  if (sr_encode_fmt(ctx->dst,"%d ",stageid)<0) return -1;
  for (;srcc-->0;src++) {
    if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*src)>>4])<0) return -1;
    if (sr_encode_u8(ctx->dst,"0123456789abcdef"[(*src)&15])<0) return -1;
  }
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Post stage: GAIN
 */
 
static int eggdev_eaut_generate_post_gain(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc==3) return sr_encode_fmt(ctx->dst,"gain %f %f\n",((src[0]<<8)|src[1])/256.0f,src[2]/255.0f);
  if (srcc==2) return sr_encode_fmt(ctx->dst,"gain %f\n",((src[0]<<8)|src[1])/256.0f);
  return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_GAIN,src,srcc);
}

/* Post stage: DELAY
 */
 
static int eggdev_eaut_generate_post_delay(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=6) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_DELAY,src,srcc);
  return sr_encode_fmt(ctx->dst,
    "delay %f %f %f %f %f\n",
    ((src[0]<<8)|src[1])/256.0f,src[2]/255.0f,src[3]/255.0f,src[4]/255.0f,src[5]/255.0f
  );
}

/* Post stage: LOPASS
 */
 
static int eggdev_eaut_generate_post_lopass(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=2) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_LOPASS,src,srcc);
  return sr_encode_fmt(ctx->dst,"lopass %d\n",(src[0]<<8)|src[1]);
}

/* Post stage: HIPASS
 */
 
static int eggdev_eaut_generate_post_hipass(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=2) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_HIPASS,src,srcc);
  return sr_encode_fmt(ctx->dst,"hipass %d\n",(src[0]<<8)|src[1]);
}

/* Post stage: BPASS
 */
 
static int eggdev_eaut_generate_post_bpass(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=4) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_BPASS,src,srcc);
  return sr_encode_fmt(ctx->dst,"bpass %d %d\n",(src[0]<<8)|src[1],(src[2]<<8)|src[3]);
}

/* Post stage: NOTCH
 */
 
static int eggdev_eaut_generate_post_notch(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=4) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_NOTCH,src,srcc);
  return sr_encode_fmt(ctx->dst,"notch %d %d\n",(src[0]<<8)|src[1],(src[2]<<8)|src[3]);
}

/* Post stage: WAVESHAPER
 */
 
static int eggdev_eaut_generate_post_waveshaper(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc&1) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_WAVESHAPER,src,srcc);
  int c=srcc>>1;
  if (sr_encode_raw(ctx->dst,"waveshaper",10)<0) return -1;
  for (;c-->0;src+=2) if (sr_encode_fmt(ctx->dst," 0x%04x",(src[0]<<8)|src[1])<0) return -1;
  if (sr_encode_u8(ctx->dst,0x0a)<0) return -1;
  return 0;
}

/* Post stage: TREMOLO
 */
 
static int eggdev_eaut_generate_post_tremolo(struct eggdev_convert_context *ctx,const uint8_t *src,int srcc) {
  if (srcc!=4) return eggdev_eaut_generate_post_generic(ctx,EAU_STAGEID_TREMOLO,src,srcc);
  return sr_encode_fmt(ctx->dst,"tremolo %f %f %f\n",((src[0]<<8)|src[1])/256.0f,src[2]/255.0f,src[3]/255.0f);
}

/* Channel Header blocks.
 */
 
static int eggdev_eaut_generate_chhdr(struct eggdev_convert_context *ctx,struct eau_file *file) {
  int err;
  struct eau_channel_reader reader={.v=file->chhdrv,.c=file->chhdrc};
  struct eau_channel_entry channel;
  while ((err=eau_channel_reader_next(&channel,&reader))>0) {
  
    const char *modename=0;
    switch (channel.mode) {
      case EAU_CHANNEL_MODE_NOOP: modename="noop"; break;
      case EAU_CHANNEL_MODE_DRUM: modename="drum"; break;
      case EAU_CHANNEL_MODE_FM: modename="fm"; break;
      case EAU_CHANNEL_MODE_SUB: modename="sub"; break;
    }
    if (modename) {
      if (sr_encode_fmt(ctx->dst,"%d %d %d %s {\n",channel.chid,channel.trim,channel.pan,modename)<0) return -1;
    } else {
      if (sr_encode_fmt(ctx->dst,"%d %d %d %d {\n",channel.chid,channel.trim,channel.pan,channel.mode)<0) return -1;
    }
    
    switch (channel.mode) {
      case EAU_CHANNEL_MODE_DRUM: err=eggdev_eaut_generate_chhdr_drum(ctx,channel.payload,channel.payloadc); break;
      case EAU_CHANNEL_MODE_FM: err=eggdev_eaut_generate_chhdr_fm(ctx,channel.payload,channel.payloadc); break;
      case EAU_CHANNEL_MODE_SUB: err=eggdev_eaut_generate_chhdr_sub(ctx,channel.payload,channel.payloadc); break;
      default: err=eggdev_eaut_generate_chhdr_generic(ctx,channel.payload,channel.payloadc);
    }
    if (err<0) return err;
    
    if (channel.postc) {
      if (sr_encode_raw(ctx->dst,"post {\n",-1)<0) return -1;
      int postp=0;
      while (postp<channel.postc) {
        if (postp>channel.postc-2) return eggdev_convert_error(ctx,"Malformed EAU post.");
        uint8_t stageid=channel.post[postp++];
        uint8_t len=channel.post[postp++];
        if (postp>channel.postc-len) return eggdev_convert_error(ctx,"Malformed EAU post.");
        switch (stageid) {
          case EAU_STAGEID_GAIN: err=eggdev_eaut_generate_post_gain(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_DELAY: err=eggdev_eaut_generate_post_delay(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_LOPASS: err=eggdev_eaut_generate_post_lopass(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_HIPASS: err=eggdev_eaut_generate_post_hipass(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_BPASS: err=eggdev_eaut_generate_post_bpass(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_NOTCH: err=eggdev_eaut_generate_post_notch(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_WAVESHAPER: err=eggdev_eaut_generate_post_waveshaper(ctx,channel.post+postp,len); break;
          case EAU_STAGEID_TREMOLO: err=eggdev_eaut_generate_post_tremolo(ctx,channel.post+postp,len); break;
          default: err=eggdev_eaut_generate_post_generic(ctx,stageid,channel.post+postp,len); break;
        }
        if (err<0) return err;
        postp+=len;
      }
      if (sr_encode_raw(ctx->dst,"}\n",2)<0) return -1;
    }
    
    if (sr_encode_raw(ctx->dst,"}\n",2)<0) return -1;
  }
  if (err<0) return eggdev_convert_error(ctx,"Malformed EAU Channel Headers.");
  return 0;
}

/* Events block.
 */
 
static int eggdev_eaut_generate_events(struct eggdev_convert_context *ctx,struct eau_file *file) {
  if (sr_encode_raw(ctx->dst,"events {\n",-1)<0) return -1;
  int evtp=0,err;
  while (evtp<file->evtc) {
  
    // Loop position is indicated by breaking events block in two.
    if (evtp&&(evtp==file->loopp)) {
      if (sr_encode_raw(ctx->dst,"}\nevents {\n",-1)<0) return -1;
    }
    
    struct eau_event event;
    if ((err=eau_event_decode(&event,file->evtv+evtp,file->evtc-evtp))<0) {
      return eggdev_convert_error(ctx,"Malformed EAU events.");
    }
    if (!err) break;
    switch (event.type) {
      case 'd': if (sr_encode_fmt(ctx->dst,"delay %d\n",event.delay)<0) return -1; break;
      case 'n': if (sr_encode_fmt(ctx->dst,"note %d %d %d %d\n",event.chid,event.noteid,event.velocity,event.delay)<0) return -1; break;
      case 'w': if (sr_encode_fmt(ctx->dst,"wheel %d %d\n",event.chid,event.velocity)<0) return -1; break;
    }
  }
  if (sr_encode_raw(ctx->dst,"}\n",2)<0) return -1;
  return 0;
}

/* EAU text from bin, main entry point.
 */
 
int eggdev_eaut_from_eau(struct eggdev_convert_context *ctx) {
  int err;
  struct eau_file file;
  if (eau_file_decode(&file,ctx->src,ctx->srcc)<0) return eggdev_convert_error(ctx,"Failed to decode EAU file.");
  if ((err=eggdev_eaut_generate_globals(ctx,&file))<0) return err;
  if ((err=eggdev_eaut_generate_chhdr(ctx,&file))<0) return err;
  if ((err=eggdev_eaut_generate_events(ctx,&file))<0) return err;
  return 0;
}
