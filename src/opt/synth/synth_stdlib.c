#include "synth_internal.h"

/* Substitute rand(), for both native and web.
 * We want a source of random numbers which is actually deterministic.
 * Every time you call this it produces the same sequence.
 * Global context should call it just once, during init.
 */

uint32_t synth_rand(float *v,int c,uint32_t state) {
  for (;c-->0;v++) {
    state^=state<<13;
    state^=state>>17;
    state^=state<<5;
    *v=((float)state/2147483648.0f)-1.0f;
  }
  return state;
}

/* Memory allocator, purely generic.
 * All addresses and sizes are in 32-bit ints, not bytes.
 * (v) is the linear memory. First slot holds the block length, or negative block length if free.
 * Callers will always reference blocks by the index of their first block length: NB! Not the actual start of payload.
 */
 
struct synth_mem {
  int32_t *v;
  int c;
};

/* Convert between the length-word (p) we prefer and payload addresses you should expose to the public.
 * I'm trying to trap as much of the "+1" "-1" in one place as possible.
 */
static int synth_mem_index_from_pointer(const struct synth_mem *mem,const void *ptr) {
  if (!ptr) return -1;
  int p=((int32_t*)ptr-mem->v)-1;
  if ((p<0)||(p>=mem->c)) return -1;
  return p;
}
static void *synth_mem_pointer_from_index(const struct synth_mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return 0;
  return mem->v+p+1;
}

static void synth_mem_free(struct synth_mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return;
  if (mem->v[p]<0) return; // Double free!
  mem->v[p]=-mem->v[p];
}

// Size of an allocated block in words.
static int synth_mem_get_block_size(const struct synth_mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return -1;
  return mem->v[p];
}

// Size of contiguous unallocated blocks. <0 if allocated.
static int synth_mem_get_free_size(const struct synth_mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return -1;
  if (mem->v[p]>=0) return -1;
  int len=-mem->v[p];
  int nextp=p+1+len;
  while (nextp<mem->c) {
    int nextlen=mem->v[nextp];
    if (nextlen>=0) break; // Next block is allocated.
    nextlen=-nextlen;
    len+=1+nextlen; // Include the length word.
    nextp+=1+nextlen;
  }
  return len;
}

// Grow a block in-place to at least (na) or fail.
// Returns total size on success. Can be greater than (na).
static int synth_mem_grow_block(struct synth_mem *mem,int p,int na) {
  if ((p<0)||(p>=mem->c)) return -1;
  int len=mem->v[p];
  if (len>=na) return len; // Already big enough.
  int nextp=p+1+len;
  int available=synth_mem_get_free_size(mem,nextp);
  if (available<0) return -1; // Next block is allocated, or we're at the end.
  if (len+1+available<na) return -1; // Not enough room here.
  // Consume blocks until we're big enough, not necessarily all of (available).
  int p0=p;
  while (len<na) {
    p=nextp;
    int nextlen=mem->v[p];
    if (nextlen>=0) return -1; // oops
    nextlen=-nextlen;
    len+=1+nextlen;
    nextp=p+1+nextlen;
  }
  // If we consumed more than needed, mark the excess free. "8" here is entirely arbitrary, "0" would be technically valid.
  if (len>na+8) {
    int freeafter=len-na-1;
    mem->v[p0+1+na]=-freeafter;
    len=na;
  }
  mem->v[p0]=len;
  return len;
}

// Shrink a block in place to exactly the given word count. Not sure we'll want this, just providing for symmetry's sake.
static int synth_mem_shrink_block(struct synth_mem *mem,int p,int na) {
  if ((p<0)||(p>=mem->c)) return -1;
  int len=mem->v[p];
  if (len<0) return -1; // Not an allocated block.
  if (len<na) return -1; // That's not what "shrink" means.
  mem->v[p]=na;
  mem->v[p+1+na]=-(len-na-1);
  return na;
}

// Allocate a block of at least (c) words and return its "p".
static int synth_mem_allocate(struct synth_mem *mem,int c) {
  if (c<0) return -1;
  int p=0;
  while (p<mem->c) {
    if (mem->v[p]<0) {
      int available=synth_mem_get_free_size(mem,p);
      if (available>=c) {
        mem->v[p]=c;
        if (c<available) { // Mark the remainder free.
          mem->v[p+1+c]=-(available-c)+1;
        }
        return p;
      }
      p+=1+available;
    } else {
      p+=1+mem->v[p];
    }
  }
  return -1;
}

// Notify that you've reallocated (mem->v) to now allow (nc) words. (mem->c) must be the old count, and we'll update it.
static int synth_mem_master_grown(struct synth_mem *mem,int nc) {
  int addc=nc-mem->c;
  if (addc<0) return -1;
  if (!addc) return 0;
  int freelen=addc-1;
  mem->v[mem->c]=-freelen;
  mem->c=nc;
  return 0;
}

/* synth_malloc, public* interface.
 * [*] Only reachable from within synth of course.
 */

// Native builds should normally use their own libc. We can map to that trivially.
#if USE_native && !USE_FAKE_MALLOC
  #include <stdlib.h>
  void synth_malloc_quit() {}
  int synth_malloc_init() { return 0; }
  void synth_free(void *p) { free(p); }
  void *synth_malloc(int len) { return malloc(len); }
  void *synth_calloc(int a,int b) { return calloc(a,b); }
  void *synth_realloc(void *p,int len) { return realloc(p,len); }
  
// Otherwise, synth_mem is in play.
// We do allow USE_native here, and will use libc in that case. But we only use real malloc to create the master block.
#else

  #if USE_native
    #include <stdlib.h>
  #else
    extern uint32_t __heap_base;
    static int synth_pagec0=0;
    static int synth_pagea=0;
  #endif
  
  static struct synth_mem mem={0};

  void synth_malloc_quit() {
    #if USE_native
      if (mem.v) free(mem.v);
    #endif
    __builtin_memset(&mem,0,sizeof(struct synth_mem));
  }
  
  int synth_malloc_init() {
    if (mem.c) return -1;
    // When running native, we will never resize (mem.v). So initialize it with a huge block.
    #if USE_native
      int wordc=0xf00000; // <64 MB.
      if (!(mem.v=malloc(wordc<<2))) return -1;
      mem.c=wordc;
    // When running in WebAssembly, use intrinsics to query the initial memory size, then grow it to something huge.
    #else
      if (!synth_pagec0) {
        synth_pagec0=__builtin_wasm_memory_size(0);
        int na=synth_pagec0+1280;
        __builtin_wasm_memory_grow(0,na);
        synth_pagea=na;
      }
      mem.v=(int32_t*)(((uint8_t*)&__heap_base)+(synth_pagec0*0x10000));
      mem.c=(synth_pagea-synth_pagec0)*(0x1000>>2); // words from pages
    #endif
    mem.v[0]=-(mem.c-1);
    return 0;
  }
  
  void synth_free(void *addr) {
    int p=synth_mem_index_from_pointer(&mem,addr);
    if (p<0) return; // Would terminate a real program, and maybe we should do that.
    synth_mem_free(&mem,p);
  }
  
  void *synth_malloc(int len) {
    if (len<1) return 0;
    if (len>INT_MAX-3) return 0;
    int wordc=(len+3)>>2;
    int p=synth_mem_allocate(&mem,wordc);
    if (p<0) return 0;
    return synth_mem_pointer_from_index(&mem,p);
  }
  
  void *synth_calloc(int a,int b) {
    if ((a<1)||(b<1)) return 0;
    if (a>INT_MAX/b) return 0;
    int bytec=a*b;
    int wordc=(bytec+3)>>2;
    int p=synth_mem_allocate(&mem,wordc);
    if (p<0) return 0;
    void *v=synth_mem_pointer_from_index(&mem,p);
    __builtin_memset(v,0,bytec);
    return v;
  }
  
  void *synth_realloc(void *addr,int len) {
    if (!addr) return synth_malloc(len);
    if (len<1) return 0;
    if (len>INT_MAX-3) return 0;
    int wordc=(len+3)>>2;
    int p=synth_mem_index_from_pointer(&mem,addr);
    if (p<0) return 0;
    if (synth_mem_grow_block(&mem,p,wordc)>=0) return addr; // OK, grew in place.
    int np=synth_mem_allocate(&mem,wordc);
    if (np<0) return 0; // Failed to allocate a new block.
    void *nv=synth_mem_pointer_from_index(&mem,np);
    __builtin_memcpy(nv,addr,synth_mem_get_block_size(&mem,p)<<2);
    synth_mem_free(&mem,p);
    return nv;
  }
  
#endif
