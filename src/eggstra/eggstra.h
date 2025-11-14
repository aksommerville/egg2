/* eggstra.h
 * This is a companion executable to eggdev.
 * It's not required for normal builds, and it does include I/O drivers.
 */
 
#ifndef EGGSTRA_H
#define EGGSTRA_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

extern struct eggstra {

  // Command line. We'll hold dashed arguments in (optv) without examining them; they can be read later.
  // "--help" is special; it gets processed at eggstra_configure() and is not included here.
  const char *exename;
  const char *command;
  const char **srcpathv;
  int srcpathc,srcpatha;
  const char **optv; // leading dashes stripped.
  int optc,opta;
  
  volatile int sigc;
  
} eggstra;

int eggstra_configure(int argc,char **argv);
void eggstra_print_help(const char *topic,int topicc);
const char *eggstra_opt(const char *k,int kc);
int eggstra_opti(const char *k,int kc,int fallback);

int eggstra_main_play();

#endif
