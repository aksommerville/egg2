#include "zip.h"
#include "opt/serial/serial.h"
#include <zlib.h>
#include <stdio.h>

/* Cleanup.
 */
 
void zip_writer_cleanup(struct zip_writer *writer) {
  sr_encoder_cleanup(&writer->lft);
  sr_encoder_cleanup(&writer->cd);
}

/* Compress.
 */
 
static int zip_compress(struct sr_encoder *dst,const void *src,int srcc) {
  if (sr_encoder_require(dst,srcc)<0) return -1;
  z_stream z={0};
  if (deflateInit2(&z,Z_BEST_COMPRESSION,Z_DEFLATED,-15,9,Z_DEFAULT_STRATEGY)<0) return -1;
  z.next_in=(Bytef*)src;
  z.avail_in=srcc;
  for (;;) {
    if (sr_encoder_require(dst,1024)<0) {
      deflateEnd(&z);
      return -1;
    }
    z.next_out=(Bytef*)dst->v+dst->c;
    z.avail_out=dst->a-dst->c;
    int ao0=z.avail_out;
    int err=deflate(&z,Z_FINISH);
    if (err<0) {
      deflateEnd(&z);
      return -1;
    }
    int addc=ao0-z.avail_out;
    dst->c+=addc;
    if (err==Z_STREAM_END) {
      int result=z.total_out;
      deflateEnd(&z);
      return result;
    }
  }
}

/* Add file.
 */
 
int zip_writer_add(struct zip_writer *writer,const struct zip_file *file) {
  if (!writer||!file) return -1;
  if (writer->lft.c&&!writer->cd.c) return -1; // Already finished.
  if ((file->namec<0)||(file->namec>0xffff)) return -1;
  if ((file->extrac<0)||(file->extrac>0xffff)) return -1;

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
  int compression=file->compression;
  int crc=file->crc;
  int csize=file->csize;
  if (file->cdata||!file->usize) {
    if (sr_encode_raw(&writer->lft,file->cdata,file->csize)<0) return -1;
    
  /* User is asking us to compress it.
   * Run zlib directly onto the lft buffer.
   */
  } else {
    int cdatap=writer->lft.c;
    int dstlen=zip_compress(&writer->lft,file->udata,file->usize);
    if (dstlen<0) return -1;
    compression=8;
    crc=crc32(0,file->udata,file->usize);
    csize=dstlen;
    uint8_t *hdr=(uint8_t*)writer->lft.v+lftp0;
    hdr[8]=compression; hdr[9]=compression>>8;
    hdr[14]=crc; hdr[15]=crc>>8; hdr[16]=crc>>16; hdr[17]=crc>>24;
    hdr[18]=csize; hdr[19]=csize>>8; hdr[20]=csize>>16; hdr[21]=csize>>24;
  }
  
  /* Stupidest thing you can imagine, we have to repeat that entire header, file name and all, for the Central Directory.
   */
  if (sr_encode_raw(&writer->cd,"PK\1\2",4)<0) return -1;
  if (sr_encode_intle(&writer->cd,file->zip_version,2)<0) return -1; // Version Made By = 2.0
  if (sr_encode_intle(&writer->cd,file->zip_version,2)<0) return -1; // Version Needed = 2.0
  if (sr_encode_intle(&writer->cd,file->flags,2)<0) return -1; // Flags
  if (sr_encode_intle(&writer->cd,compression,2)<0) return -1; // Compression
  if (sr_encode_intle(&writer->cd,file->mtime,2)<0) return -1;
  if (sr_encode_intle(&writer->cd,file->mdate,2)<0) return -1;
  if (sr_encode_intle(&writer->cd,crc,4)<0) return -1;
  if (sr_encode_intle(&writer->cd,csize,4)<0) return -1;
  if (sr_encode_intle(&writer->cd,file->usize,4)<0) return -1;
  if (sr_encode_intle(&writer->cd,file->namec,2)<0) return -1;
  if (sr_encode_intle(&writer->cd,file->extrac,2)<0) return -1;
  if (sr_encode_intle(&writer->cd,0,2)<0) return -1; // Comment Length
  if (sr_encode_intle(&writer->cd,0,2)<0) return -1; // Disk Number Start
  if (sr_encode_intle(&writer->cd,0,2)<0) return -1; // Internal Attributes
  if (sr_encode_raw(&writer->cd,"\x00\x00\xb6\x81",4)<0) return -1; // External Attributes. Controls permissions somehow.
  if (sr_encode_intle(&writer->cd,lftp0,4)<0) return -1; // Relative Offset Of Local Header
  if (sr_encode_raw(&writer->cd,file->name,file->namec)<0) return -1;
  if (sr_encode_raw(&writer->cd,file->extra,file->extrac)<0) return -1;
  // +comment if we supported that

  writer->filec++;
  return 0;
}

/* Finish.
 */
 
int zip_writer_finish(struct sr_encoder *dst,struct zip_writer *writer) {

  int cdp=writer->lft.c;
  if (sr_encode_raw(dst,writer->lft.v,writer->lft.c)<0) return -1;
  if (sr_encode_raw(dst,writer->cd.v,writer->cd.c)<0) return -1;

  if (sr_encode_raw(dst,"PK\5\6",4)<0) return -1;
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1; // disk number
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1; // cd start disk
  if (sr_encode_intle(dst,writer->filec,2)<0) return -1; // cd entry count on disk
  if (sr_encode_intle(dst,writer->filec,2)<0) return -1; // cd entry count
  if (sr_encode_intle(dst,writer->cd.c,4)<0) return -1; // cd size
  if (sr_encode_intle(dst,cdp,4)<0) return -1; // cd start
  if (sr_encode_raw(dst,"\0\0",2)<0) return -1; // comment length

  return 0;
}
