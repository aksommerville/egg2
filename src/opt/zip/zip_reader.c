#include "zip.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

/* Cleanup reader.
 */
 
void zip_reader_cleanup(struct zip_reader *reader) {
  if (reader->tmp) free(reader->tmp);
}

/* Init reader.
 */
 
int zip_reader_init(struct zip_reader *reader,const void *src,int srcc) {
  if (!src||(srcc<0)) return -1;
  reader->src=src;
  reader->srcc=srcc;
  reader->srcp=0;
  return 0;
}

/* Decompress a file into (reader->tmp).
 */
 
static int zip_reader_uncompress(struct zip_reader *reader,const void *src,int srcc,int usize,int method) {
  if (!src||(srcc<0)||(usize<0)) return -1;
  if (usize>reader->tmpa) {
    if (usize>0x10000000) return -1; // Arbitrary 256 MB limit per file.
    int na=(usize+32768)&~32767;
    void *nv=realloc(reader->tmp,na);
    if (!nv) return -1;
    reader->tmp=nv;
    reader->tmpa=na;
  }
  switch (method) {
  
    case 0: { // Uncompressed.
        memcpy(reader->tmp,src,srcc);
      } break;
      
    case 8: { // Deflate.
        z_stream z={0};
        z.next_in=(Bytef*)src;
        z.avail_in=srcc;
        z.next_out=(Bytef*)reader->tmp;
        z.avail_out=usize;
        if (inflateInit2(&z,-15)<0) return -1;
        int err=inflate(&z,Z_FINISH);
        inflateEnd(&z);
        if (err<0) return -1;
      } break;
      
    // Zip spec also mentions Shrink, Reduce, Implode, Tokenize, Deflate64, and bzip2.
    // Should we implement any of those?
    default: return -1;
  }
  return 0;
}

/* Next file from reader.
 */

int zip_reader_next(struct zip_file *file,struct zip_reader *reader) {

  /* Expect a Local File Header.
   * Anything else, report EOF.
   * TODO This means we are ignoring the Central Directory. Is that ok?
   * What is the Central Directory for?
   * ...experimentally yes, this is sufficient to read a Zip file.
   * Not sure if that will hold against files that have been updated, created on non-Linux systems, etc....
   */
  if (reader->srcp>reader->srcc-30) return 0;
  const uint8_t *SRC=reader->src+reader->srcp;
  if (memcmp(reader->src+reader->srcp,"PK\3\4",4)) return 0;
  file->zip_version=SRC[4]|(SRC[5]<<8);
  file->flags=SRC[6]|(SRC[7]<<8);
  file->compression=SRC[8]|(SRC[9]<<8);
  file->mtime=SRC[10]|(SRC[11]<<8);
  file->mdate=SRC[12]|(SRC[13]<<8);
  file->crc=SRC[14]|(SRC[15]<<8)|(SRC[16]<<16)|(SRC[17]<<24);
  file->csize=SRC[18]|(SRC[19]<<8)|(SRC[20]<<16)|(SRC[21]<<24);
  file->usize=SRC[22]|(SRC[23]<<8)|(SRC[24]<<16)|(SRC[25]<<24);
  file->namec=SRC[26]|(SRC[27]<<8);
  file->extrac=SRC[28]|(SRC[29]<<8);
  reader->srcp+=30;
  SRC+=30;
  
  if (reader->srcp>reader->srcc-file->csize-file->extrac-file->namec) return -1;
  file->name=(char*)SRC;
  file->extra=SRC+file->namec;
  file->cdata=SRC+file->namec+file->extrac;
  if ((file->compression==0)||(file->usize==0)) { // Uncompressed.
    file->udata=file->cdata;
  } else {
    if (zip_reader_uncompress(reader,file->cdata,file->csize,file->usize,file->compression)<0) return -1;
    file->udata=reader->tmp;
  }
  reader->srcp+=file->namec+file->extrac+file->csize;
  
  return 1;
}
