#include "synth_internal.h"

/* Advance all printers and drop finished ones.
 */
 
static void synth_update_printers(struct synth *synth,int framec) {
  int i=synth->printerc;
  while (i-->0) {
    struct synth_printer *printer=synth->printerv[i];
    if (synth_printer_update(printer,framec)<=0) {
      synth->printerc--;
      memmove(synth->printerv+i,synth->printerv+i+1,sizeof(void*)*(synth->printerc-i));
      synth_printer_del(printer);
    }
  }
}

/* Update voices (channel, pcmplay).
 */
 
static void synth_update_voices(float *v,int framec,struct synth *synth) {
  int i;
  for (i=synth->channelc;i-->0;) {
    struct synth_channel *channel=synth->channelv[i];
    if (synth_channel_update(v,framec,channel)<=0) {
      synth->channelc--;
      memmove(synth->channelv+i,synth->channelv+i+1,sizeof(void*)*(synth->channelc-i));
      if (synth->channel_by_chid[channel->chid]==channel) {
        synth->channel_by_chid[channel->chid]=0;
      }
      synth_channel_del(channel);
    }
  }
  struct synth_pcmplay *pcmplay=synth->pcmplayv+synth->pcmplayc-1;
  for (i=synth->pcmplayc;i-->0;pcmplay--) {
    if (synth_pcmplay_update(v,framec,pcmplay)<=0) {
      synth_pcmplay_cleanup(pcmplay);
      synth->pcmplayc--;
      memmove(pcmplay,pcmplay+1,sizeof(struct synth_pcmplay)*(synth->pcmplayc-i));
    }
  }
}

/* Trigger any song events waiting at time zero, and enforce delays.
 * (limit) in frames and must be >0.
 * Returns the frame count to the next event, but never more than (limit).
 * Whatever frame count we return, we will already have advanced past in the song.
 * Call this whether a song exists or not, let that be my problem.
 */
 
static int synth_update_song(struct synth *synth,int limit) {
  for (;;) {
  
    // No song? Easy.
    if (!synth->song) return limit;
    
    // Delay already pending? Advance clock as far as we can and get out.
    if (synth->songdelay) {
      int updframec=synth->songdelay;
      if (updframec>limit) updframec=limit;
      synth->songdelay-=updframec;
      return updframec;
    }
    
    // End of song? Either repeat or unload it.
    uint8_t lead;
    if ((synth->songp>=synth->songc)||!(lead=synth->song[synth->songp++])) {
      if (synth->songrepeat) {
        /* Report a one-frame delay at each repeat.
         * Won't have any noticeable impact on output.
         * But if a song misbehaves by repeating immediately, this will only be Bad instead of Catastrophic.
         */
        synth->songp=synth->songloopp;
        return 1;
      }
      synth_end_song(synth);
      return limit;
    }
    
    /* Process event.
     * If it's a delay, the logic above will handle returning, on the next pass of this loop.
     * If it's invalid, terminate the song.
     * Delays can go up to 2048 ms and our rate limit is 200 kHz ~= 409 M frames. Fits in 31 bits.
     */
    if (!(lead&0x80)) {
      synth->songdelay=(lead*synth->rate)/1000;
      continue;
    }
    switch (lead&0xf0) {

      case 0x80: { // LONG DELAY
          synth->songdelay=((((lead&0x0f)+1)<<7)*synth->rate)/1000;
        } break;
        
      case 0x90: { // SHORT NOTE
          if (synth->songp>synth->songc-2) goto _invalid_;
          uint8_t a=synth->song[synth->songp++];
          uint8_t b=synth->song[synth->songp++];
          uint8_t chid=lead&0x0f;
          if (!synth->channel_by_chid[chid]) break;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&1)<<3)|(b>>5);
          int dur=b&0x1f;
          dur=(dur*synth->rate)/1000;
          synth_channel_note(synth->channel_by_chid[chid],noteid,velocity/15.0f,dur);
        } break;
        
      case 0xa0: { // MEDIUM NOTE
          if (synth->songp>synth->songc-2) goto _invalid_;
          uint8_t a=synth->song[synth->songp++];
          uint8_t b=synth->song[synth->songp++];
          uint8_t chid=lead&0x0f;
          if (!synth->channel_by_chid[chid]) break;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&1)<<3)|(b>>5);
          int dur=b&0x1f;
          dur=(dur+1)<<5; // range 1024 => overflow impossible
          dur=(dur*synth->rate)/1000;
          synth_channel_note(synth->channel_by_chid[chid],noteid,velocity/15.0f,dur);
        } break;
        
      case 0xb0: { // LONG NOTE
          if (synth->songp>synth->songc-2) goto _invalid_;
          uint8_t a=synth->song[synth->songp++];
          uint8_t b=synth->song[synth->songp++];
          uint8_t chid=lead&0x0f;
          if (!synth->channel_by_chid[chid]) break;
          uint8_t noteid=a>>1;
          uint8_t velocity=((a&1)<<3)|(b>>5);
          int dur=b&0x1f;
          // If we calculated duration correctly ((((dur+1)*1024)*synth->rate)/1000), overflow would be possible.
          // But we can cheat it by pretending there are 1024 ms in a second -- 1024 is the encoded scale here. So just pretend they're encoded as seconds.
          // The effect of this is that long notes (>1 second) play 1/40 longer than intended. I doubt anyone will notice.
          dur=(dur+1)*synth->rate;
          synth_channel_note(synth->channel_by_chid[chid],noteid,velocity/15.0f,dur);
        } break;
        
      case 0xc0: { // WHEEL
          if (synth->songp>=synth->songc) goto _invalid_;
          uint8_t chid=lead&0x0f;
          uint8_t v=synth->song[synth->songp++];
          if (!synth->channel_by_chid[chid]) break;
          synth_channel_wheel(synth->channel_by_chid[chid],v);
        } break;
        
      default: _invalid_: {
          synth_end_song(synth);
          return limit;
        }
    }
  }
  return limit;
}

/* Update to length-limited floating-point buffer.
 */

void synth_update_internal(float *v,int framec,struct synth *synth) {
  synth_update_printers(synth,framec);
  synth->framec_in_progress=framec;
  while (framec>0) {
    int updframec=synth_update_song(synth,framec);
    synth_update_voices(v,updframec,synth);
    v+=updframec*synth->chanc;
    framec-=updframec;
  }
  synth->framec_in_progress=0;
}
