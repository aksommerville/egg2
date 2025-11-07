/* eau.h
 * Convert between EAU and MIDI.
 */
 
#ifndef EAU_H
#define EAU_H

struct sr_encoder;

typedef int (*eau_get_chdr_fn)(void *dstpp,int fqpid);

int eau_cvt_eau_midi(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg);
int eau_cvt_midi_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg);
int eau_cvt_eau_eaut(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg);
int eau_cvt_eaut_eau(struct sr_encoder *dst,const void *src,int srcc,const char *path,eau_get_chdr_fn get_chdr,int strip_names,struct sr_encoder *errmsg);

#endif
