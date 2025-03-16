/* eggdev_rom.h
 */
 
#ifndef EGGDEV_ROM_H
#define EGGDEV_ROM_H

struct eggdev_rom_writer {
  struct eggdev_rw_res {
    int tid,rid;
    void *v;
    int c;
  } *resv;
  int resc,resa;
};

void eggdev_rom_writer_cleanup(struct eggdev_rom_writer *writer);

int eggdev_rom_writer_search(const struct eggdev_rom_writer *writer,int tid,int rid);
struct eggdev_rw_res *eggdev_rom_writer_insert(struct eggdev_rom_writer *writer,int p,int tid,int rid);
int eggdev_rw_res_set_serial(struct eggdev_rw_res *res,const void *src,int srcc);
int eggdev_rw_res_handoff_serial(struct eggdev_rw_res *res,void *src,int srcc);

int eggdev_rom_writer_encode(struct sr_encoder *dst,const struct eggdev_rom_writer *writer);

//int eggdev_rom_writer_from_path(struct eggdev_rom_writer *writer,const char *path);

struct eggdev_rom_reader {
  const uint8_t *v;
  int c,p;
  int tid,rid;
};
struct eggdev_res {
  int tid,rid;
  int c;
  const void *v;
};

int eggdev_rom_reader_init(struct eggdev_rom_reader *reader,const void *src,int srcc);
int eggdev_rom_reader_next(struct eggdev_res *res,struct eggdev_rom_reader *reader);

#endif
