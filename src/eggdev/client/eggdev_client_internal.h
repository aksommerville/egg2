#ifndef EGGDEV_CLIENT_INTERNAL_H
#define EGGDEV_CLIENT_INTERNAL_H

#include "eggdev/eggdev_internal.h"

int eggdev_client_resolve_value(int *v,int nstype,const char *ns,int nsc,const char *k,int kc);
int eggdev_client_resolve_name(void *kpp,int nstype,const char *ns,int nsc,int v);
struct eggdev_ns *eggdev_client_ns_intern(int nstype,const char *name,int namec);
struct eggdev_sym *eggdev_client_sym_intern(struct eggdev_ns *ns,const char *k,int kc,int v);

#endif
