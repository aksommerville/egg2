#include "demo.h"

struct g g={0};

/* Cleanup.
 */

void egg_client_quit(int status) {
}

/* Init.
 */

int egg_client_init() {

  // We do use rand(), esp in video tests.
  srand_auto();

  // Validate framebuffer size. It's wise to do this during init, because it's easy to mess up.
  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\n",fbw,fbh,FBW,FBH);
    return -1;
  }

  // Acquire the ROM.
  g.romc=egg_rom_get(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  egg_rom_get(g.rom,g.romc);
  
  // Break ROM into resources.
  {
    struct rom_reader reader;
    if (rom_reader_init(&reader,g.rom,g.romc)>=0) {
      struct rom_entry res;
      while (rom_reader_next(&res,&reader)>0) {
        if (g.resc>=g.resa) {
          int na=g.resa+32;
          if (na>INT_MAX/sizeof(struct rom_entry)) return -1;
          void *nv=realloc(g.resv,sizeof(struct rom_entry)*na);
          if (!nv) return -1;
          g.resv=nv;
          g.resa=na;
        }
        memcpy(g.resv+g.resc++,&res,sizeof(struct rom_entry));
      }
    }
  }
  
  // Create our standard font.
  if (!(g.font=font_new())) return -1;
  const char *msg;
  if (msg=font_add_image(g.font,RID_image_font9_0020,0x0020)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  if (msg=font_add_image(g.font,RID_image_font9_00a1,0x00a1)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  if (msg=font_add_image(g.font,RID_image_font9_0400,0x0400)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  
  // Load some global images.
  if (egg_texture_load_image(g.texid_fonttiles=egg_texture_new(),RID_image_fonttiles)<0) return -1;
  
  // Create the home menu.
  if (!modal_new_home()) return -1;
  
  /* Test printf.
   * Alas we can't write unit tests for any of the fake stdlib, because those would be built natively and use real stdlib instead.
   */
  if (0) {
    fprintf(stderr,"Decimal plain(%d) pad(%5d) zeropad(%05d) leftalign(%-5d)\n",123,123,123,123);
    fprintf(stderr,"Negative plain(%d) pad(%5d) zeropad(%05d) leftalign(%-5d)\n",-123,-123,-123,-123);
    fprintf(stderr,"Unsigned plain(%u) pad(%5u) zeropad(%05u) leftalign(%-5u)\n",123,123,123,123);
    fprintf(stderr,"Octal plain(%o) pad(%5o) zeropad(%05o) leftalign(%-5o)\n",511,511,511,511);
    fprintf(stderr,"Hex plain(%x) pad(%6x) zeropad(%06x) leftalign(%-6x)\n",0xbee,0xbee,0xbee,0xbee);
    fprintf(stderr,"Hex-prefix plain(%#x) pad(%#6x) zeropad(%#06x) leftalign(%#-6x)\n",0xbee,0xbee,0xbee,0xbee);
    fprintf(stderr,"Float plain(%.3f) pad(%7.3f) zeropad(%07.3f) leftalign(%-7.3f)\n",3.141,3.141,3.141,3.141);
    fprintf(stderr,"Float plain(%.3f) pad(%7.3f) zeropad(%07.3f) leftalign(%-7.3f)\n",-3.141,-3.141,-3.141,-3.141);
    fprintf(stderr,"String plain(%s) pad(%6s) leftalign(%-6s)\n","egg","egg","egg");
  }
  
  // Test prepopulating params.
  if (1) {
    char tmp[64];
    int tmpc=egg_store_get(tmp,sizeof(tmp),"helloMessage",12);
    if ((tmpc>0)&&(tmpc<=sizeof(tmp))) {
      fprintf(stderr,"Got helloMessage: '%.*s'\n",tmpc,tmp);
    }
  }

  return 0;
}

/* Notify.
 */
 
void egg_client_notify(int k,int v) {
  fprintf(stderr,"%s %d=%d\n",__func__,k,v);
}

/* Update.
 */

void egg_client_update(double elapsed) {
  if (g.modalc<1) {
    egg_terminate(0);
    return;
  }
  struct modal *modal=g.modalv[g.modalc-1];
  
  int input=egg_input_get_one(0);
  int pvinput=g.pvinput;
  if (input!=g.pvinput) {
    if (modal->input) {
      uint16_t bit=0x8000;
      for (;bit;bit>>=1) {
        if ((input&bit)&&!(g.pvinput&bit)) modal->input(modal,bit,1);
        else if (!(input&bit)&&(g.pvinput&bit)) modal->input(modal,bit,0);
      }
    }
    if (!modal->suppress_exit) {
      if ((input&EGG_BTN_WEST)&&!(g.pvinput&EGG_BTN_WEST)) {
        egg_play_sound(RID_sound_uicancel,0.5,0.0);
        modal->defunct=1;
      }
    }
    g.pvinput=input;
  }
  
  if (modal->update) {
    modal->update(modal,elapsed,input,pvinput);
  }
  
  modal_drop_defunct();
  if (g.modalc<1) {
    egg_terminate(0);
    return;
  }
  struct modal *nfocus=g.modalv[g.modalc-1];
  if ((nfocus!=modal)&&input) {
    if (modal_is_resident(modal)) { // A new modal got pushed. Report loss of input to the old one.
      if (modal->input) {
        uint16_t bit=0x8000;
        for (;bit;bit>>=1) {
          if (input&bit) modal->input(modal,bit,0);
        }
      }
      if (modal->update) {
        modal->update(modal,0.0,0,input);
      }
    }
    // No need to update (nfocus); they'll figure it out via regular updates.
  }
}

/* Render.
 */

void egg_client_render() {
  if (g.modalc<1) return;
  struct modal *modal=g.modalv[g.modalc-1];
  graf_reset(&g.graf);
  if (!modal->opaque) {
    //graf_gradient_rect(&g.graf,0,0,FBW,FBH,0x100808ff,0x302020ff,0x302020ff,0x100808ff);
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x204030ff);
  }
  if (modal->render) modal->render(modal);
  
  graf_flush(&g.graf);
}

/* Resource list.
 */
 
int demo_get_res(void *dstpp/*BORROW*/,int tid,int rid) {
  int p=demo_resv_search(tid,rid);
  if (p<0) return 0;
  const struct rom_entry *res=g.resv+p;
  *(const void**)dstpp=res->v;
  return res->c;
}

int demo_resv_search(int tid,int rid) {
  int lo=0,hi=g.resc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct rom_entry *q=g.resv+ck;
         if (tid<q->tid) hi=ck;
    else if (tid>q->tid) lo=ck+1;
    else if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}
