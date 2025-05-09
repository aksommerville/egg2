/* res.h
 * Helpers for unpacking an Egg ROM and standard resource types.
 * This is designed to be included in clients, but it's also embedded in the runtime.
 * No dependencies, not even stdlib.
 *
 * Most types work roughly the same way:
 *   struct TYPE_reader reader;
 *   if (TYPE_reader_init(&reader,SERIAL,SERIALC)>=0) {
 *     struct TYPE_entry entry;
 *     while (TYPE_reader_next(&entry,&reader)>0) {
 *       ...do something with (entry)...
 *     }
 *   }
 * Iterators and decoded models point into the serial you provide.
 */
 
#ifndef RES_H
#define RES_H

struct rom_reader {
  const unsigned char *v;
  int c,p;
  int tid,rid;
};
struct rom_entry {
  int tid,rid;
  const void *v;
  int c;
};
int rom_reader_init(struct rom_reader *reader,const void *src,int srcc);
int rom_reader_next(struct rom_entry *entry,struct rom_reader *reader);

struct metadata_reader {
  const unsigned char *v;
  int c,p;
};
struct metadata_entry {
  const char *k,*v;
  int kc,vc;
};
int metadata_reader_init(struct metadata_reader *reader,const void *src,int srcc);
int metadata_reader_next(struct metadata_entry *entry,struct metadata_reader *reader);

struct strings_reader {
  const unsigned char *v;
  int c,p,index;
};
struct strings_entry {
  int index,c;
  const char *v;
};
int strings_reader_init(struct strings_reader *reader,const void *src,int srcc);
int strings_reader_next(struct strings_entry *entry,struct strings_reader *reader); // Empty strings are skipped.

struct cmdlist_reader {
  const unsigned char *v;
  int c,p;
};
struct cmdlist_entry {
  unsigned char opcode;
  const unsigned char *arg;
  int argc; // Usually knowable from (opcode); we'll never return something that disagrees.
};
int cmdlist_reader_init(struct cmdlist_reader *reader,const void *src,int srcc);
int cmdlist_reader_next(struct cmdlist_entry *entry,struct cmdlist_reader *reader);
int sprite_reader_init(struct cmdlist_reader *reader,const void *src,int srcc); // sprite is just a cmdlist with a signature.

struct map_res {
  int w,h;
  const unsigned char *v; // (w*h) LRTB
  const void *cmd; // cmdlist
  int cmdc;
};
int map_res_decode(struct map_res *map,const void *src,int srcc);

//TODO tilesheet
//TODO decalsheet

#endif
