#include "demo.h"

struct g g={0};

void egg_client_quit(int status) {
}

int egg_client_init() {

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\n",fbw,fbh,FBW,FBH);
    return -1;
  }
  fprintf(stderr,"framebuffer %dx%d\n",fbw,fbh);

  g.romc=egg_rom_get(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  egg_rom_get(g.rom,g.romc);
  fprintf(stderr,"rom size %d\n",g.romc);
  
  if (!(g.font=font_new())) return -1;
  const char *msg;
  if (msg=font_add_image(g.font,RID_image_font9_0020,0x0020)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  if (msg=font_add_image(g.font,RID_image_font9_00a1,0x00a1)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  if (msg=font_add_image(g.font,RID_image_font9_0400,0x0400)) { fprintf(stderr,"Font error: %s\n",msg); return -1; }
  
  if (!modal_new_home()) return -1;

  return 0;
}

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
      if ((input&EGG_BTN_WEST)&&!(g.pvinput&EGG_BTN_WEST)) modal->defunct=1;
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
    if (nfocus->input) {
      uint16_t bit=0x8000;
      for (;bit;bit>>=1) {
        if (input&bit) modal->input(modal,bit,1);
      }
    }
    // No need to update (nfocus); they'll figure it out via regular updates.
  }
}

void egg_client_render() {
  if (g.modalc<1) return;
  struct modal *modal=g.modalv[g.modalc-1];
  graf_reset(&g.graf);
  if (!modal->opaque) {
    graf_gradient_rect(&g.graf,0,0,FBW,FBH,0x100808ff,0x302020ff,0x302020ff,0x100808ff);
  }
  if (modal->render) modal->render(modal);
  graf_flush(&g.graf);
}
