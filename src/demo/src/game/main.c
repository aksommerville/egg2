#include "demo.h"

struct g g={0};

void egg_client_quit(int status) {
  egg_log("egg_client_quit...");
}

int egg_client_init() {
  egg_log("egg_client_init...");

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBH)||(fbh!=FBH)) {
    fprintf(stderr,"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\n",fbw,fbh,FBW,FBH);
    return -1;
  }

  g.romc=egg_rom_get(0,0);
  if (!(g.rom=malloc(g.romc))) return -1;
  egg_rom_get(g.rom,g.romc);

  //TODO

  return 0;
}

void egg_client_update(double elapsed) {
  egg_log("egg_client_update...");
  //TODO
}

void egg_client_render() {
  egg_log("egg_client_render...");
  graf_reset(&g.graf);
  //TODO
  graf_flush(&g.graf);
}
