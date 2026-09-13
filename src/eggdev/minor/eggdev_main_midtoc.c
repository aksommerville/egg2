#include "eggdev/eggdev_internal.h"
#include "opt/midi/midi.h"

/* Dump contents of MIDI file.
 * Caller creates a midi_file first.
 */
 
static int midtoc_dump(struct midi_file *file,const char *srcpath) {

  /* Stream the file and gather some stats.
   */
  int tempous=0;
  int loopms=0;
  int notec_by_channel[16]={0};
  int wheel_by_channel[16]={0};
  int duration=0;
  const uint8_t *chdr=0,*text=0;
  int chdrc=0,textc=0;
  for (;;) {
    struct midi_event event;
    int delay=midi_file_next(&event,file);
    if (delay<0) {
      if (!midi_file_is_finished(file)) {
        fprintf(stderr,"%s: Error streaming events off MIDI file.\n",srcpath);
        return -2;
      }
      break;
    }
    if (delay) {
      duration+=delay;
      if (midi_file_advance(file,delay)<0) return -1;
      continue;
    }
    switch (event.opcode) {
      case 0x90: notec_by_channel[event.chid]++; break;
      case 0xe0: wheel_by_channel[event.chid]++; break;
      case MIDI_OPCODE_META: switch (event.a) {
          case 0x07: if ((event.c==4)&&!memcmp(event.v,"LOOP",4)) {
              if (loopms) fprintf(stderr,"%s:WARNING: Multiple loop points.\n",srcpath);
              loopms=duration;
            } break;
          case 0x51: if (event.c==3) {
              if (tempous) fprintf(stderr,"%s:WARNING: Multiple Set Tempo commands.\n",srcpath);
              if (duration) fprintf(stderr,"%s:WARNING: Set Tempo not at time zero (%d ms).\n",srcpath,duration);
              const uint8_t *b=event.v;
              tempous=(b[0]<<16)|(b[1]<<8)|b[2];
            } break;
          case 0x77: {
              if (chdrc) fprintf(stderr,"%s:WARNING: Multiple CHDR.\n",srcpath);
              if (duration) fprintf(stderr,"%s:WARNING: CHDR not at time zero.\n",srcpath);
              chdr=event.v;
              chdrc=event.c;
            } break;
          case 0x78: {
              if (text) fprintf(stderr,"%s:WARNING: Multiple TEXT.\n",srcpath);
              if (duration) fprintf(stderr,"%s:WARNING: TEXT not at time zero.\n",srcpath);
              text=event.v;
              textc=event.c;
            } break;
        } break;
    }
  }
  
  /* Report global features.
   */
  fprintf(stderr,"%s:\n",srcpath);
  fprintf(stderr,"  Duration: %d ms\n",duration);
  fprintf(stderr,"  Tempo: %d us/qnote\n",tempous);
  fprintf(stderr,"  Loop: %d ms\n",loopms);
  fprintf(stderr,"  CHDR: %d\n",chdrc);
  fprintf(stderr,"  TEXT: %d\n",textc);
  fprintf(stderr,"  Channels with note:");
  int i=0;
  for (;i<16;i++) if (notec_by_channel[i]) fprintf(stderr," %d",i);
  fprintf(stderr,"\n");
  fprintf(stderr,"  Channels with wheel:");
  for (;i<16;i++) if (wheel_by_channel[i]) fprintf(stderr," %d",i);
  fprintf(stderr,"\n");
  return 0;
}

/* Copy an MTrk payload, stripping any Egg Meta events.
 */
 
static int midtoc_copy_mtrk_without_meta(struct sr_encoder *dst,const uint8_t *src,int srcc) {
  int srcp=0;
  uint8_t status=0;
  while (srcp<srcc) {
  
    // First is delay. If nonzero, we can stop and copy the rest verbatim -- Egg events are only permitted at time zero.
    if (src[srcp]) {
      if (sr_encode_raw(dst,src+srcp,srcc-srcp)<0) return -1;
      return 0;
    }
    srcp++;
    
    // Next status byte. Yes, Running Status is in play.
    if (srcp>=srcc) return -1;
    if (src[srcp]&0x80) status=src[srcp++];
    else if (!status) return -1;
    
    // Sysex copy verbatim, but a little painful to measure. Oh well.
    if ((status==0xf0)||(status==0xf7)) {
      int paylen,lenlen;
      if ((lenlen=sr_vlq_decode(&paylen,src+srcp,srcc-srcp))<0) return -1;
      srcp+=lenlen;
      if (srcp>srcc-paylen) return -1;
      if (sr_encode_u8(dst,0)<0) return -1; // Delay
      if (sr_encode_u8(dst,status)<0) return -1; // Sysex
      if (sr_encode_vlq(dst,paylen)<0) return -1;
      if (sr_encode_raw(dst,src+srcp,paylen)<0) return -1;
      srcp+=paylen;
      status=0;
      continue;
    }
    
    // Meta are similar to Sysex, with an extra "type" byte, and we might want to ignore the whole event.
    if (status==0xff) {
      if (srcp>=srcc) return -1;
      int type=src[srcp++];
      int paylen,lenlen;
      if ((lenlen=sr_vlq_decode(&paylen,src+srcp,srcc-srcp))<0) return -1;
      srcp+=lenlen;
      if (srcp>srcc-paylen) return -1;
      if ((type!=0x77)&&(type!=0x78)) { // Emit only if non-Egg.
        if (sr_encode_u8(dst,0)<0) return -1; // Delay
        if (sr_encode_u8(dst,0xff)<0) return -1; // Meta
        if (sr_encode_u8(dst,type)<0) return -1;
        if (sr_encode_vlq(dst,paylen)<0) return -1;
        if (sr_encode_raw(dst,src+srcp,paylen)<0) return -1;
      }
      srcp+=paylen;
      status=0;
      continue;
    }
    
    // Anything else must be a channel voice event: 1 or 2 bytes after status.
    int paylen;
    switch (status&0xf0) {
      // Note Off, Note On, Note Adjust, Control Change, Wheel: 2 bytes payload.
      case 0x80: case 0x90: case 0xa0: case 0xb0: case 0xe0: paylen=2; break;
      // Program Change, Channel Pressure: 1 byte payload.
      case 0xc0: case 0xd0: paylen=1; break;
      default: return -1;
    }
    if (srcp>srcc-paylen) return -1;
    if (sr_encode_u8(dst,0)<0) return -1; // Delay
    if (sr_encode_u8(dst,status)<0) return -1; // Status. If it used Running Status originally, oops, wasting some bytes.
    if (sr_encode_raw(dst,src+srcp,paylen)<0) return -1;
    srcp+=paylen;
    // (status) remains set
  }
  return 0;
}

/* Merge MIDI files.
 * Write a new MIDI file in (dst) containing notes from (notefile) and everything else from (metafile).
 * This provides both --replace-voices and --replace-notes.
 */
 
static int midtoc_merge(
  struct sr_encoder *dst,
  struct midi_file *metafile,const uint8_t *meta,int metac,
  struct midi_file *notefile,const uint8_t *note,int notec,
  const char *metapath,const char *notepath
) {
  
  /* Read (metafile) only to extract the CHDR and TEXT chunks.
   */
  const void *chdr=0,*text=0;
  int chdrc=0,textc=0;
  for (;;) {
    struct midi_event event;
    int err=midi_file_next(&event,metafile);
    if (err<0) {
      fprintf(stderr,"%s: Error streaming MIDI events.\n",metapath);
      return -2;
    }
    if (err>0) {
      // Stop reading at the first delay. CHDR and TEXT may only appear at time zero.
      break;
    } else if (event.opcode==MIDI_OPCODE_META) {
      if (event.a==0x77) {
        chdr=event.v;
        chdrc=event.c;
      } else if (event.a==0x78) {
        text=event.v;
        textc=event.c;
      }
    }
  }
  
  /* Copy (note) chunkwise.
   * Prepend (chdr) and (text) to the first MTrk chunk.
   * MTrk must go thru a filter to remove any existing CHDR or TEXT.
   */
  int notep=0,copied=0;
  while (notep<=notec-8) {
    const uint8_t *chunkid=note+notep;
    int chunklen=(note[notep+4]<<24)|(note[notep+5]<<16)|(note[notep+6]<<8)|note[notep+7];
    notep+=8;
    if ((chunklen<0)||(notep>notec-chunklen)) { // odd, midi_file didn't complain.
      fprintf(stderr,"%s: Malformed MIDI file\n",notepath);
      return -1;
    }
    
    // Start with chunkid always, verbatim.
    if (sr_encode_raw(dst,chunkid,4)<0) return -1;
    
    // For non-MTrk chunks, copy the payload verbatim too.
    if (memcmp(chunkid,"MTrk",4)) {
      if (sr_encode_intbe(dst,chunklen,4)<0) return -1;
      if (sr_encode_raw(dst,note+notep,chunklen)<0) return -1;
      notep+=chunklen;
      continue;
    }
    
    // MTrk, so we're going to modify it possibly in two ways. Mark the length position.
    int lenp=dst->c;
    if (sr_encode_raw(dst,"\0\0\0\0",4)<0) return -1;
    
    // If we haven't emitted the metadata yet, do that first.
    if (!copied) {
      copied=1;
      if (chdrc) {
        if (sr_encode_u8(dst,0)<0) return -1; // Delay
        if (sr_encode_u8(dst,0xff)<0) return -1; // Meta
        if (sr_encode_u8(dst,0x77)<0) return -1; // EAU Channel Headers (private to Egg)
        if (sr_encode_vlq(dst,chdrc)<0) return -1;
        if (sr_encode_raw(dst,chdr,chdrc)<0) return -1;
      }
      if (textc) {
        if (sr_encode_u8(dst,0)<0) return -1; // Delay
        if (sr_encode_u8(dst,0xff)<0) return -1; // Meta
        if (sr_encode_u8(dst,0x78)<0) return -1; // EAU Text (private to Egg)
        if (sr_encode_vlq(dst,textc)<0) return -1;
        if (sr_encode_raw(dst,text,textc)<0) return -1;
      }
    }
    
    // Get the chunk copied with Egg Meta events filtered out.
    if (midtoc_copy_mtrk_without_meta(dst,note+notep,chunklen)<0) return -1;
    notep+=chunklen;
    
    // Fill in the length.
    int len=dst->c-(lenp+4);
    uint8_t *lendst=(uint8_t*)dst->v+lenp;
    lendst[0]=len>>24;
    lendst[1]=len>>16;
    lendst[2]=len>>8;
    lendst[3]=len;
  }
  
  /* If the metadata didn't copy, there must not have been an MTrk chunk.
   * That situation is unusual enough that I don't want to ignore it.
   */
  if (!copied&&(chdrc||textc)) {
    fprintf(stderr,"%s:ERROR: MIDI file didn't contain any MTrk.\n",notepath);
    return -1;
  }
  
  return 0;
}

/* midtoc main.
 */
 
int eggdev_main_midtoc() {

  if (g.srcpathc!=1) {
    fprintf(stderr,"%s: One input file required.\n",g.exename);
    return -2;
  }
  
  void *serial=0;
  int serialc=file_read(&serial,g.srcpathv[0]);
  if (serialc<0) {
    fprintf(stderr,"%s: Failed to read file.\n",g.srcpathv[0]);
    return -2;
  }
  
  struct midi_file *file=midi_file_new(serial,serialc,1000);
  if (!file) {
    free(serial);
    fprintf(stderr,"%s: Failed to decode file as MIDI.\n",g.srcpathv[0]);
    return -2;
  }
  
  void *dstserial=0;
  int dstserialc=0;
  struct midi_file *dstfile=0;
  if (g.dstpath) {
    if ((dstserialc=file_read(&dstserial,g.dstpath))<0) {
      fprintf(stderr,"%s: Failed to read in-place output file (it's supposed to exist).\n",g.dstpath);
      free(serial);
      midi_file_del(file);
      return -2;
    }
    if (!(dstfile=midi_file_new(dstserial,dstserialc,1000))) {
      fprintf(stderr,"%s: Failed to decode file as MIDI.\n",g.dstpath);
      free(serial);
      midi_file_del(file);
      free(dstserial);
      return -2;
    }
  }
  
  int err=-1;
  const char *dummy;
  struct sr_encoder dst={0};
  int write_dst=0;
  if (g.replace_notes) {
    write_dst=1;
    err=midtoc_merge(&dst,dstfile,dstserial,dstserialc,file,serial,serialc,g.dstpath,g.srcpathv[0]);
  } else if (g.replace_voices) {
    write_dst=1;
    err=midtoc_merge(&dst,file,serial,serialc,dstfile,dstserial,dstserialc,g.srcpathv[0],g.dstpath);
  } else {
    err=midtoc_dump(file,g.srcpathv[0]);
  }
  
  if (write_dst&&(err>=0)) {
    if (file_write(g.dstpath,dst.v,dst.c)<0) {
      fprintf(stderr,"%s: Failed to write %d bytes output.\n",g.dstpath,dst.c);
      err=-2;
    }
  }

  sr_encoder_cleanup(&dst);
  if (dstfile) midi_file_del(dstfile);
  if (dstserial) free(dstserial);
  midi_file_del(file);
  free(serial);
  return err;
}
