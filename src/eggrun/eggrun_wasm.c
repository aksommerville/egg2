#include "eggrun_internal.h"
#include "wasm_c_api.h"
#include "wasm_export.h"

static struct eggrun_wasm {
  wasm_module_t mod;
  wasm_module_inst_t inst;
  wasm_exec_env_t ee;
  int exec_callstate;
  
  // Wamr evidently writes into the binary we give it. Why would it do that? Well, we have to copy it then.
  void *code;
  int codec;
  
  wasm_function_inst_t egg_client_quit;
  wasm_function_inst_t egg_client_init;
  wasm_function_inst_t egg_client_notify;
  wasm_function_inst_t egg_client_update;
  wasm_function_inst_t egg_client_render;
} eggrun_wasm={0};

/* We're not cleaning up, but here's how it would be done: (from egg1, so some names have changed)
    if (eggrun_wasm.ee) wasm_runtime_destroy_exec_env(eggrun_wasm.ee);
    if (eggrun_wasm.inst) wasm_runtime_deinstantiate(eggrun_wasm.inst);
    if (eggrun_wasm.mod) wasm_module_delete(&eggrun_wasm.mod);
    if (eggrun_wasm.code) free(eggrun_wasm.code);
*/

/* WAMR hooks for the Egg Platform API.
 */

static void *eggrun_wasm_get_client_memory(uint32_t addr,int len) {
  if (!wasm_runtime_validate_app_addr(eggrun_wasm.inst,addr,len)) return 0;
  return wasm_runtime_addr_app_to_native(eggrun_wasm.inst,addr);
}

static void egg_wasm_terminate(wasm_exec_env_t ee,int status) {
  return egg_terminate(status);
}

static void egg_wasm_log(wasm_exec_env_t ee,const char *msg) {
  egg_log(msg);
}

static double egg_wasm_time_real(wasm_exec_env_t ee) {
  return egg_time_real();
}

static void egg_wasm_time_local(wasm_exec_env_t ee,int dstp,int dsta) {
  if (dsta<1) return;
  int *dst=eggrun_wasm_get_client_memory(dstp,dsta*sizeof(int));
  if (dst) egg_time_local(dst,dsta);
}

static int egg_wasm_prefs_get(wasm_exec_env_t ee,int k) {
  return egg_prefs_get(k);
}

static int egg_wasm_prefs_set(wasm_exec_env_t ee,int k,int v) {
  return egg_prefs_set(k,v);
}

static int egg_wasm_rom_get(wasm_exec_env_t ee,void *dst,int dsta) {
  return egg_rom_get(dst,dsta);
}

static int egg_wasm_rom_get_res(wasm_exec_env_t ee,void *dst,int dsta,int tid,int rid) {
  return egg_rom_get_res(dst,dsta,tid,rid);
}

static int egg_wasm_store_get(wasm_exec_env_t ee,char *v,int va,const char *k,int kc) {
  return egg_store_get(v,va,k,kc);
}

static int egg_wasm_store_set(wasm_exec_env_t ee,const char *k,int kc,const char *v,int vc) {
  return egg_store_set(k,kc,v,vc);
}

static int egg_wasm_store_key_by_index(wasm_exec_env_t ee,char *k,int ka,int p) {
  return egg_store_key_by_index(k,ka,p);
}

static void egg_wasm_input_configure(wasm_exec_env_t ee) {
  egg_input_configure();
}

static void egg_wasm_input_get_all(wasm_exec_env_t ee,uint32_t dstp,int dsta) {
  int *dst=eggrun_wasm_get_client_memory(dstp,sizeof(int)*dsta);
  egg_input_get_all(dst,dsta);
}

static int egg_wasm_input_get_one(wasm_exec_env_t ee,int playerid) {
  return egg_input_get_one(playerid);
}

static void egg_wasm_input_set_mode(wasm_exec_env_t ee,int mode) {
  egg_input_set_mode(mode);
}

static int egg_wasm_input_get_mouse(wasm_exec_env_t ee,int xp,int yp) {
  int *x=0,*y=0;
  if (xp) x=eggrun_wasm_get_client_memory(xp,sizeof(int));
  if (yp) y=eggrun_wasm_get_client_memory(yp,sizeof(int));
  return egg_input_get_mouse(x,y);
}

static void egg_wasm_play_sound(wasm_exec_env_t ee,int soundid,float trim,float pan) {
  egg_play_sound(soundid,trim,pan);
}

static void egg_wasm_play_song(wasm_exec_env_t ee,int songid,int rid,int repeat,float trim,float pan) {
  egg_play_song(songid,rid,repeat,trim,pan);
}

static void egg_wasm_song_set(wasm_exec_env_t ee,int songid,int chid,int prop,float v) {
  egg_song_set(songid,chid,prop,v);
}

static void egg_wasm_song_event_note_on(wasm_exec_env_t ee,int songid,int chid,int noteid,int velocity) {
  egg_song_event_note_on(songid,chid,noteid,velocity);
}

static void egg_wasm_song_event_note_off(wasm_exec_env_t ee,int songid,int chid,int noteid) {
  egg_song_event_note_off(songid,chid,noteid);
}

static void egg_wasm_song_event_note_once(wasm_exec_env_t ee,int songid,int chid,int noteid,int velocity,int durms) {
  egg_song_event_note_once(songid,chid,noteid,velocity,durms);
}

static void egg_wasm_song_event_wheel(wasm_exec_env_t ee,int songid,int chid,int v) {
  egg_song_event_wheel(songid,chid,v);
}

static float egg_wasm_song_get_playhead(wasm_exec_env_t ee,int songid) {
  return egg_song_get_playhead(songid);
}

static void egg_wasm_texture_del(wasm_exec_env_t ee,int texid) {
  egg_texture_del(texid);
}

static int egg_wasm_texture_new(wasm_exec_env_t ee) {
  return egg_texture_new();
}

static void egg_wasm_texture_get_size(wasm_exec_env_t ee,int wp,int hp,int texid) {
  int *w=eggrun_wasm_get_client_memory(wp,4);
  int *h=eggrun_wasm_get_client_memory(hp,4);
  egg_texture_get_size(w,h,texid);
}

static int egg_wasm_texture_load_image(wasm_exec_env_t ee,int texid,int rid) {
  return egg_texture_load_image(texid,rid);
}

static int egg_wasm_texture_load_raw(wasm_exec_env_t ee,int texid,int w,int h,int stride,const void *src,int srcc) {
  return egg_texture_load_raw(texid,w,h,stride,src,srcc);
}

static int egg_wasm_texture_get_pixels(wasm_exec_env_t ee,void *dst,int dsta,int texid) {
  return egg_texture_get_pixels(dst,dsta,texid);
}

static void egg_wasm_texture_clear(wasm_exec_env_t ee,int dsttexid) {
  egg_texture_clear(dsttexid);
}

static void egg_wasm_render(wasm_exec_env_t ee,int uniformp,const void *vtxv,int vtxc) {
  if (!vtxv||(vtxc<1)) return;
  const struct egg_render_uniform *uniform=eggrun_wasm_get_client_memory(uniformp,sizeof(struct egg_render_uniform));
  if (!uniform) return;
  egg_render(uniform,vtxv,vtxc);
}

static NativeSymbol eggrun_wasm_exports[]={
  {"egg_terminate",egg_wasm_terminate,"(i)"},
  {"egg_log",egg_wasm_log,"($)"},
  {"egg_time_real",egg_wasm_time_real,"()F"},
  {"egg_time_local",egg_wasm_time_local,"(ii)"},
  {"egg_prefs_get",egg_wasm_prefs_get,"(i)i"},
  {"egg_prefs_set",egg_wasm_prefs_set,"(ii)i"},
  {"egg_rom_get",egg_wasm_rom_get,"(*~)i"},
  {"egg_rom_get_res",egg_wasm_rom_get_res,"(*~ii)i"},
  {"egg_store_get",egg_wasm_store_get,"(*~*~)i"},
  {"egg_store_set",egg_wasm_store_set,"(*~*~)i"},
  {"egg_store_key_by_index",egg_wasm_store_key_by_index,"(*~i)i"},
  {"egg_input_configure",egg_wasm_input_configure,"()"},
  {"egg_input_get_all",egg_wasm_input_get_all,"(ii)"},
  {"egg_input_get_one",egg_wasm_input_get_one,"(i)i"},
  {"egg_input_set_mode",egg_wasm_input_set_mode,"(i)"},
  {"egg_input_get_mouse",egg_wasm_input_get_mouse,"(**)i"},
  {"egg_play_sound",egg_wasm_play_sound,"(iff)"},
  {"egg_play_song",egg_wasm_play_song,"(iiiff)"},
  {"egg_song_set",egg_wasm_song_set,"(iiif)"},
  {"egg_song_event_note_on",egg_wasm_song_event_note_on,"(iiii)"},
  {"egg_song_event_note_off",egg_wasm_song_event_note_off,"(iii)"},
  {"egg_song_event_note_once",egg_wasm_song_event_note_once,"(iiiii)"},
  {"egg_song_event_wheel",egg_wasm_song_event_wheel,"(iii)"},
  {"egg_song_get_playhead",egg_wasm_song_get_playhead,"(i)f"},
  {"egg_texture_del",egg_wasm_texture_del,"(i)"},
  {"egg_texture_new",egg_wasm_texture_new,"()i"},
  {"egg_texture_get_size",egg_wasm_texture_get_size,"(iii)"},
  {"egg_texture_load_image",egg_wasm_texture_load_image,"(ii)i"},
  {"egg_texture_load_raw",egg_wasm_texture_load_raw,"(iiii*~)i"},
  {"egg_texture_get_pixels",egg_wasm_texture_get_pixels,"(*~i)i"},
  {"egg_texture_clear",egg_wasm_texture_clear,"(i)"},
  {"egg_render",egg_wasm_render,"(i*i)"},
};

/* Get code:1 from the ROM.
 * We could generalize resource extraction, but no point, this is the only resource we're going to need.
 * We can't use eggrt's resource loading facilities because eggrt proper hasn't been initialized yet.
 * Luckily the ROM format is dead simple.
 */
 
static int eggrun_get_code1(void *dstpp,const uint8_t *src,int srcc) {
  if ((srcc<4)||memcmp(src,"\0ERM",4)) return -1;
  int srcp=4,tid=1,rid=1;
  for (;;) {
    if (srcp>=srcc) return 0; // No terminator. Invalid.
    uint8_t lead=src[srcp++];
    if (!lead) return 0; // End of file and we haven't found it yet.
    switch (lead&0xc0) {
      case 0x00: {
          tid+=lead;
          if (tid>EGG_TID_code) return -1;
          rid=1;
        } break;
      case 0x40: { // RID
          if (srcp>srcc-1) return -1;
          int d=(lead&0x3f)<<8;
          d|=src[srcp++];
          d++;
          rid+=d;
          if (rid>0xffff) return -1;
        } break;
      case 0x80: { // RES
          if (srcp>srcc-2) return -1;
          int len=(lead&0x3f)<<16;
          len|=src[srcp++]<<8;
          len|=src[srcp++];
          len++;
          if (srcp>srcc-len) return -1;
          if ((tid==EGG_TID_code)&&(rid==1)) {
            *(const void**)dstpp=src+srcp;
            return len;
          }
          srcp+=len;
        } break;
      default: return -1; // Reserved, invalid.
    }
  }
}

/* Start up the Wasm runtime.
 */
 
int eggrun_boot(const void *rom,int romc,const char *path) {
  
  // Find and copy code:1.
  const void *code=0;
  int codec=eggrun_get_code1(&code,rom,romc);
  if (codec<1) {
    fprintf(stderr,"%s: ROM file does not contain WebAssembly code.\n",path);
    return -2;
  }
  if (!(eggrun_wasm.code=malloc(codec))) return -1;
  memcpy(eggrun_wasm.code,code,codec);
  eggrun_wasm.codec=codec;
  
  // Bring WAMR online and register our entry points.
  if (!wasm_runtime_init()) return -1;
  if (!wasm_runtime_register_natives("env",eggrun_wasm_exports,sizeof(eggrun_wasm_exports)/sizeof(NativeSymbol))) return -1;
  
  int stack_size=0x01000000;//TODO
  int heap_size=0x01000000;//TODO
  char msg[1024]={0};
  if (!(eggrun_wasm.mod=wasm_runtime_load(eggrun_wasm.code,eggrun_wasm.codec,msg,sizeof(msg)))) {
    fprintf(stderr,"%s: wasm_runtime_load failed: %s\n",eggrt.exename,msg);
    return -2;
  }
  if (!(eggrun_wasm.inst=wasm_runtime_instantiate(eggrun_wasm.mod,stack_size,heap_size,msg,sizeof(msg)))) {
    fprintf(stderr,"%s: wasm_runtime_instantiate failed: %s\n",eggrt.exename,msg);
    return -2;
  }
  if (!(eggrun_wasm.ee=wasm_runtime_create_exec_env(eggrun_wasm.inst,stack_size))) {
    fprintf(stderr,"%s: wasm_runtime_create_exec_env failed\n",eggrt.exename);
    return -2;
  }
  
  #define LOADFN(name) { \
    if (!(eggrun_wasm.name=wasm_runtime_lookup_function(eggrun_wasm.inst,#name))) { \
      fprintf(stderr,"%s: ROM does not export required function '%s'\n",eggrt.exename,#name); \
      return -2; \
    } \
  }
  LOADFN(egg_client_quit)
  LOADFN(egg_client_init)
  LOADFN(egg_client_notify)
  LOADFN(egg_client_update)
  LOADFN(egg_client_render)
  #undef LOADFN
  
  return 0;
}

/* Call client hooks.
 */

void egg_client_quit(int status) {
  if (!eggrun_wasm.ee) return;
  if (eggrun_wasm.exec_callstate!=1) return;
  eggrun_wasm.exec_callstate=2;
  uint32_t argv[1]={status};
  if (!wasm_runtime_call_wasm(eggrun_wasm.ee,eggrun_wasm.egg_client_quit,1,argv)) {
    const char *msg=wasm_runtime_get_exception(eggrun_wasm.inst);
    fprintf(stderr,"%s: egg_client_quit failed hard: %s\n",eggrt.exename,msg);
    eggrt.terminate=1;
    eggrt.status=1;
  }
}

int egg_client_init() {
  if (!eggrun_wasm.ee) return -1;
  if (eggrun_wasm.exec_callstate!=0) return -1;
  eggrun_wasm.exec_callstate=1;
  uint32_t argv[1]={0};
  if (wasm_runtime_call_wasm(eggrun_wasm.ee,eggrun_wasm.egg_client_init,0,argv)) {
    int result=argv[0];
    if (result<0) {
      fprintf(stderr,"%s: Error %d from egg_client_init\n",eggrt.exename,result);
      return -2;
    }
  } else {
    const char *msg=wasm_runtime_get_exception(eggrun_wasm.inst);
    fprintf(stderr,"%s: egg_client_init failed hard: %s\n",eggrt.exename,msg);
    return -2;
  }
  return 0;
}

void egg_client_notify(int k,int v) {
  if (!eggrun_wasm.ee) return;
  if (eggrun_wasm.exec_callstate!=1) return;
  uint32_t argv[2]={k,v};
  if (wasm_runtime_call_wasm(eggrun_wasm.ee,eggrun_wasm.egg_client_notify,2,argv)) return;
  const char *msg=wasm_runtime_get_exception(eggrun_wasm.inst);
  fprintf(stderr,"%s: egg_client_notify failed hard: %s\n",eggrt.exename,msg);
  eggrt.terminate=1;
  eggrt.status=1;
}

void egg_client_update(double elapsed) {
  if (!eggrun_wasm.ee) return;
  if (eggrun_wasm.exec_callstate!=1) return;
  uint32_t argv[2]={0,0};
  memcpy(argv,&elapsed,sizeof(double));
  if (wasm_runtime_call_wasm(eggrun_wasm.ee,eggrun_wasm.egg_client_update,2,argv)) return;
  const char *msg=wasm_runtime_get_exception(eggrun_wasm.inst);
  fprintf(stderr,"%s: egg_client_update failed hard: %s\n",eggrt.exename,msg);
  eggrt.terminate=1;
  eggrt.status=1;
}

void egg_client_render() {
  if (!eggrun_wasm.ee) return;
  if (eggrun_wasm.exec_callstate!=1) return;
  uint32_t argv[1]={0};
  if (!wasm_runtime_call_wasm(eggrun_wasm.ee,eggrun_wasm.egg_client_render,0,argv)) {
    const char *msg=wasm_runtime_get_exception(eggrun_wasm.inst);
    fprintf(stderr,"%s: egg_client_render failed hard: %s\n",eggrt.exename,msg);
    eggrt.terminate=1;
    eggrt.status=1;
  }
}
