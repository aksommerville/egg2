/* zip.h
 * Helpers for reading and writing PKZIP files.
 * Requires zlib and serial.
 * We only operate on entire archives in memory.
 */
 
#ifndef ZIP_H
#define ZIP_H

#include <stdint.h>
#include "opt/serial/serial.h"

/* Read Zip file iteratively.
 ************************************************************************************/

struct zip_reader {
  const uint8_t *src;
  int srcc,srcp;
  void *tmp;
  int tmpa;
};

struct zip_file {
  uint16_t zip_version;
  uint16_t flags;
  uint16_t compression;
  uint16_t mtime;
  uint16_t mdate;
  uint32_t crc;
  uint32_t csize;
  uint32_t usize;
  const char *name;
  int namec;
  const uint8_t *extra;
  int extrac;
  const void *cdata; // Compressed, directly off the file binary.
  const void *udata; // Uncompressed. Goes out of scope after the next file gets queued up.
};

/* Readers require cleanup if next was ever called.
 * If init succeeds, call next until it returns <=0. >0 means (file) is populated.
 * Input binary must be held unchanged during iteration.
 * We assume that you want every file uncompressed, and we go ahead and do that during next, or fail.
 */
void zip_reader_cleanup(struct zip_reader *reader);
int zip_reader_init(struct zip_reader *reader,const void *src,int srcc);
int zip_reader_next(struct zip_file *file,struct zip_reader *reader);

/* Compose Zip file.
 *************************************************************************************/
 
struct zip_writer {
  struct sr_encoder lft;
  struct sr_encoder cd;
  int filec;
};

void zip_writer_cleanup(struct zip_writer *writer);

/* If (cdata) null, we will select an appropriate compression method and do it. (either Uncompressed or Deflate).
 * If (cdata) set, we ignore (udata) and trust your setting of (compression,usize,crc).
 */
int zip_writer_add(struct zip_writer *writer,const struct zip_file *file);

int zip_writer_finish(struct sr_encoder *dst,struct zip_writer *writer);

#endif
