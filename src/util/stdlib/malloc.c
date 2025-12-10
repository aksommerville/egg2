#if USE_real_stdlib
  int egg_stdlib_malloc_dummy=0;
#else

#include "egg-stdlib.h"

/* Copied from synth.
 * We're exporting the real stdlib symbols ("malloc" et al) so we can't fake it for native builds, the way synth does.
 */
 
struct mem {
  int32_t *v;
  int c;
};

/* Convert between the length-word (p) we prefer and payload addresses you should expose to the public.
 * I'm trying to trap as much of the "+1" "-1" in one place as possible.
 */
static int mem_index_from_pointer(const struct mem *mem,const void *ptr) {
  if (!ptr) return -1;
  int p=((int32_t*)ptr-mem->v)-1;
  if ((p<0)||(p>=mem->c)) return -1;
  return p;
}
static void *mem_pointer_from_index(const struct mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return 0;
  return mem->v+p+1;
}

static void mem_free(struct mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return;
  if (mem->v[p]<0) return; // Double free!
  mem->v[p]=-mem->v[p];
}

// Size of an allocated block in words.
static int mem_get_block_size(const struct mem *mem,int p) {
  if ((p<0)||(p>=mem->c)) return -1;
  return mem->v[p];
}

// Size of contiguous unallocated blocks. <0 if allocated.
static int mem_get_free_size(const struct mem *mem,int p) {
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
static int mem_grow_block(struct mem *mem,int p,int na) {
  if ((p<0)||(p>=mem->c)) return -1;
  int len=mem->v[p];
  if (len>=na) return len; // Already big enough.
  int nextp=p+1+len;
  int available=mem_get_free_size(mem,nextp);
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
static int mem_shrink_block(struct mem *mem,int p,int na) {
  if ((p<0)||(p>=mem->c)) return -1;
  int len=mem->v[p];
  if (len<0) return -1; // Not an allocated block.
  if (len<na) return -1; // That's not what "shrink" means.
  mem->v[p]=na;
  mem->v[p+1+na]=-(len-na-1);
  return na;
}

// Allocate a block of at least (c) words and return its "p".
static int mem_allocate(struct mem *mem,int c) {
  if (c<0) return -1;
  int p=0;
  while (p<mem->c) {
    if (mem->v[p]<0) {
      int available=mem_get_free_size(mem,p);
      if (available>=c) {
        //if ((mem->v[p]<0)&&(p+1-mem->v[p]>=mem->c)) logint(p+1+c); // XXX TEMP Log the high-water mark.
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
static int mem_master_grown(struct mem *mem,int nc) {
  int addc=nc-mem->c;
  if (addc<0) return -1;
  if (!addc) return 0;
  int freelen=addc-1;
  mem->v[mem->c]=-freelen;
  mem->c=nc;
  return 0;
}

extern uint32_t __heap_base;
static int pagec0=0;
static int pagea=0;
  
static struct mem mem={0};

static void malloc_quit() {
  __builtin_memset(&mem,0,sizeof(struct mem));
}
  
static int malloc_init() {
  if (mem.c) return -1;
  if (!pagec0) {
    pagec0=__builtin_wasm_memory_size(0);
    int na=pagec0+1000;
    __builtin_wasm_memory_grow(0,na);
    int pagecz=__builtin_wasm_memory_size(0);
    pagea=na;
  }
  mem.v=(int32_t*)(((uint8_t*)&__heap_base)+(pagec0*0x10000));
  mem.c=(pagea-pagec0)*(0x10000>>2); // words from pages
  mem.v[0]=-(mem.c-1);
  return 0;
}
  
void free(void *addr) {
  int p=mem_index_from_pointer(&mem,addr);
  if (p<0) return; // Would terminate a real program, and maybe we should do that.
  mem_free(&mem,p);
}
  
void *malloc(long unsigned int len) {
  if (!mem.c&&(malloc_init()<0)) return 0;
  if (len<1) return 0;
  if (len>INT_MAX-3) return 0;
  int wordc=(len+3)>>2;
  int p=mem_allocate(&mem,wordc);
  if (p<0) return 0;
  return mem_pointer_from_index(&mem,p);
}
  
void *calloc(long unsigned int a,long unsigned int b) {
  if (!mem.c&&(malloc_init()<0)) return 0;
  if ((a<1)||(b<1)) return 0;
  if (a>INT_MAX/b) return 0;
  int bytec=a*b;
  int wordc=(bytec+3)>>2;
  int p=mem_allocate(&mem,wordc);
  if (p<0) return 0;
  void *v=mem_pointer_from_index(&mem,p);
  __builtin_memset(v,0,bytec);
  return v;
}
  
void *realloc(void *addr,long unsigned int len) {
  if (!mem.c&&(malloc_init()<0)) return 0;
  if (!addr) return malloc(len);
  if (len<1) return 0;
  if (len>INT_MAX-3) return 0;
  int wordc=(len+3)>>2;
  int p=mem_index_from_pointer(&mem,addr);
  if (p<0) return 0;
  if (mem_grow_block(&mem,p,wordc)>=0) return addr; // OK, grew in place.
  int np=mem_allocate(&mem,wordc);
  if (np<0) return 0; // Failed to allocate a new block.
  void *nv=mem_pointer_from_index(&mem,np);
  __builtin_memcpy(nv,addr,mem_get_block_size(&mem,p)<<2);
  mem_free(&mem,p);
  return nv;
}

#endif
