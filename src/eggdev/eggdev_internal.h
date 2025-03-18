#ifndef EGGDEV_INTERNAL_H
#define EGGDEV_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "egg/egg.h"
#include "convert/eggdev_convert.h"
#include "opt/serial/serial.h"
#include "opt/fs/fs.h"

struct http_context;

extern struct g {

// Populated at eggdev_configure():
  const char *exename;
  const char *sdkpath;
  int command;
  char *dstpath;
  char **srcpathv;
  int srcpathc,srcpatha;
  char *dstfmt,*srcfmt; // convert
  char **htdocsv;
  int htdocsc,htdocsa;
  int port;
  int unsafe_external;
  char *writeable;
  char *project;
  char *lang;
  char *format;
  int verbatim;
  int terminate;
  
// Populated the first time you ask for eggdev_config_key_by_index() or eggdev_config_get():
  struct eggdev_bcfg {
    char *k,*v;
    int kc,vc;
  } *bcfgv;
  int bcfgc,bcfga;
  
// May be populated when you resolve tid or symbols.
  struct eggdev_client {
    char *root;
    int rootc;
    int ready;
    struct eggdev_ns {
      int nstype;
      char *name;
      int namec;
      struct eggdev_sym {
        int v;
        char *k;
        int kc;
      } *symv;
      int symc,syma;
    } *nsv;
    int nsc,nsa;
  } client;
  
  struct http_context *http;
  volatile int sigc;
  
} g;

#define EGGDEV_COMMAND_build 1
#define EGGDEV_COMMAND_serve 2
#define EGGDEV_COMMAND_minify 3
#define EGGDEV_COMMAND_convert 4
#define EGGDEV_COMMAND_config 5
#define EGGDEV_COMMAND_project 6
#define EGGDEV_COMMAND_metadata 7
#define EGGDEV_COMMAND_pack 8
#define EGGDEV_COMMAND_unpack 9
#define EGGDEV_COMMAND_list 10
#define EGGDEV_COMMAND_run 11
#define EGGDEV_COMMAND_dump 12
#define EGGDEV_COMMAND_FOR_EACH \
  _(build) \
  _(serve) \
  _(minify) \
  _(convert) \
  _(config) \
  _(project) \
  _(metadata) \
  _(pack) \
  _(unpack) \
  _(list) \
  _(run) \
  _(dump)
  
#define _(tag) int eggdev_main_##tag();
EGGDEV_COMMAND_FOR_EACH
#undef _

void eggdev_print_help(const char *topic,int topicc);

int eggdev_command_eval(const char *src,int srcc);
const char *eggdev_command_repr(int command);

int eggdev_configure(int argc,char **argv);

int eggdev_config_key_by_index(void *dstpp,int p);
int eggdev_config_get(void *dstpp,const char *k,int kc);
int eggdev_config_get_sub(void *dstpp,const char *target,int targetc,const char *k,int kc);

// client/eggdev_client_public.c
void eggdev_client_dirty();
int eggdev_client_require();
int eggdev_client_set_root(const char *path,int pathc);
int eggdev_tid_eval(const char *src,int srcc);
int eggdev_tid_eval_standard(const char *src,int srcc); // Standard types only. No integers, no custom.
int eggdev_tid_repr(char *dst,int dsta,int tid);
int eggdev_symbol_eval(int *dst,const char *src,int srcc,int nstype,const char *ns,int nsc);
int eggdev_symbol_repr(char *dst,int dsta,int src,int nstype,const char *ns,int nsc);
#define EGGDEV_NSTYPE_CMD 1
#define EGGDEV_NSTYPE_NS 2
#define EGGDEV_NSTYPE_RES 3 /* (ns) is type name */
#define EGGDEV_NSTYPE_RESTYPE 4 /* (ns) unused */

// Load an HTML template and return it WEAK.
int eggdev_get_separate_html_template(void *dstpp);
int eggdev_get_standalone_html_template(void *dstpp);

/* Helpers for regular file input and output.
 * Empty paths, "-", "<stdin>", and "<stdout>" automatically use stdin/stdout instead.
 */
int eggdev_read_input(void *dstpp,const char *path);
int eggdev_write_output(const char *path,const void *src,int srcc);
int eggdev_read_stdin(void *dstpp);
int eggdev_write_stdout(const void *src,int srcc);

int eggdev_lineno(const char *src,int srcc);
int eggdev_relative_path(char *dst,int dsta,const char *ref,int refc,const char *sub,int subc);
int eggdev_res_ids_from_path(int *tid,int *rid,const char *path);

#endif
