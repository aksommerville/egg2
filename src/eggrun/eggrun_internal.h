#ifndef EGGRUN_INTERNAL_H
#define EGGRUN_INTERNAL_H

#include "eggrt/eggrt_internal.h"

extern const char *eggrun_rom_path;

/* Locate the ROM file's path, load it into a newly-allocated buffer, and blank out the argument.
 * Sets (eggrun_rom_path) on success. And on some failures but not all.
 * Does not fully validate the ROM.
 */
int eggrun_load_file(void *dstpp,int argc,char **argv);

/* Locate code:1, bring WAMR online, load it all.
 * Does not call egg_client_init(), but after a success, we are ready to.
 */
int eggrun_boot(const void *rom,int romc,const char *path);

#endif
