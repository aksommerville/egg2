#include "zip.h"
#include "opt/serial/serial.h"

/* Cleanup.
 */
 
void zip_writer_cleanup(struct zip_writer *writer) {
  sr_encoder_cleanup(&writer->lft);
  sr_encoder_cleanup(&writer->cd);
}

/* Add file.
 */
 
int zip_writer_add(struct zip_writer *writer,const struct zip_file *file) {
  if (!writer||!file) return -1;
  //if (writer->lft.c&&!writer->cd.c) return -1; // Already finished.

  int lftp0=writer->lft.c;
  if (sr_encode_raw(&writer->lft,"PK\3\4",4)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->zip_version,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->flags,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->compression,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->mtime,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->mdate,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->crc,4)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->csize,4)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->usize,4)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->namec,2)<0) return -1;
  if (sr_encode_intle(&writer->lft,file->extrac,2)<0) return -1;
  if (sr_encode_raw(&writer->lft,file->name,file->namec)<0) return -1;
  if (sr_encode_raw(&writer->lft,file->extra,file->extrac)<0) return -1;
  
  /* If compressed content was provided, great, we've emitted the correct values,
   * now just dump the provided content.
   * Ditto if the only supplied uncompressed but it's empty.
   */
  if (file->cdata||!file->usize) {
    fprintf(stderr,"ALREADY COMPRESSED OR EMPTY %d => %d\n",file->usize,file->csize);
    if (sr_encode_raw(&writer->lft,file->cdata,file->csize)<0) return -1;
    
  /* User is asking us to compress it.
   * Run zlib directly onto the lft buffer.
   */
  } else {
    int cdatap=writer->lft.c;
    int bound=compressBound(file->usize);
    if (bound<1) return -1;
    if (sr_encoder_require(&file->lft,bound)<0) return -1;
    uLongf dstlen=file->lft.a-file->lft.c;
    int err=compress2((Bytef*)file->lft.v+file->lft.c,&dstlen,(Bytef*)file->udata,file->usize,Z_BEST_COMPRESSION);
    if (err<0) return -1;
    if (dstlen<file->usize) {
      fprintf(stderr,"COMPRESSED %d => %d\n",file->usize,dstlen);
      file->lft.c+=dstlen;
      uint8_t *hdr=(uint8_t*)file->lft.v+lftp0;
      hdr[8]=8; hdr[9]=0; // Deflate.
      hdr[14]=crc; hdr[15]=crc>>8; hdr[16]=crc>>16; hdr[17]=crc>>24;
      hdr[18]=dstlen; hdr[19]=dstlen>>8; hdr[20]=dstlen>>16; hdr[21]=dstlen>>24;
    } else {
      fprintf(stderr,"UNCOMPRESSED %d\n",file->usize);
      if (sr_encode_raw(&file->lft,file->udata,file->usize)<0) return -1;
      uint8_t *hdr=(uint8_t*)file->lft.v+lftp0;
      hdr[8]=0; hdr[9]=0; // Uncompressed.
      hdr[18]=file->usize; hdr[19]=file->usize>>8; hdr[20]=file->usize>>16; hdr[21]=file->usize>>24;
    }
  }
  
  //TODO Add to Central Directory. Do we really need to?

  return 0;
}

/* Finish.
 */
 
int zip_writer_finish(void *dstpp,struct zip_writer *writer) {
  if (sr_encode_raw(&writer->lft,writer->cd.v,writer->cd.c)<0) return -1;
  writer->cd.c=0;
  *(void**)dstpp=writer->lft.v;
  return writer->lft.c;
}
