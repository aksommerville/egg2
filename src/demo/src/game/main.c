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
  
  //TODO Prep font and graphics.

  return 0;
}

void egg_client_update(double elapsed) {
  //TODO
}

void egg_client_render() {
  graf_reset(&g.graf);
  
  // Fill background with a gradient.
  graf_triangle_strip_begin(&g.graf,
    0,0,  0xa06060ff,
    FBW,0,0x60a060ff,
    0,FBH,0x6060a0ff
  );
  graf_triangle_strip_more(&g.graf,
    FBW,FBH,0x808080ff
  );
  
  // Tiles.
  graf_set_image(&g.graf,RID_image_tiles);
  graf_tile(&g.graf, 40, 40,0x00,0); // Top row: Reference images (pre-transformed)
  graf_tile(&g.graf, 60, 40,0x01,0);
  graf_tile(&g.graf, 80, 40,0x02,0);
  graf_tile(&g.graf,100, 40,0x03,0);
  graf_tile(&g.graf,120, 40,0x04,0);
  graf_tile(&g.graf,140, 40,0x05,0);
  graf_tile(&g.graf,160, 40,0x06,0);
  graf_tile(&g.graf,180, 40,0x07,0);
  graf_tile(&g.graf, 40, 60,0x00,0); // Second row: Exact same images effected with a transform.
  graf_tile(&g.graf, 60, 60,0x00,1);
  graf_tile(&g.graf, 80, 60,0x00,2);
  graf_tile(&g.graf,100, 60,0x00,3);
  graf_tile(&g.graf,120, 60,0x00,4);
  graf_tile(&g.graf,140, 60,0x00,5);
  graf_tile(&g.graf,160, 60,0x00,6);
  graf_tile(&g.graf,180, 60,0x00,7);
  graf_fancy(&g.graf, 40, 80,0x00,0,0,16,0,0xff0000ff); // Third row: Exact same images using "fancy" tiles and primary color.
  graf_fancy(&g.graf, 60, 80,0x00,1,0,16,0,0x00ff00ff);
  graf_fancy(&g.graf, 80, 80,0x00,2,0,16,0,0x0000ffff);
  graf_fancy(&g.graf,100, 80,0x00,3,0,16,0,0xffff00ff);
  graf_fancy(&g.graf,120, 80,0x00,4,0,16,0,0xff00ffff);
  graf_fancy(&g.graf,140, 80,0x00,5,0,16,0,0x00ffffff);
  graf_fancy(&g.graf,160, 80,0x00,6,0,16,0,0xc0c0c0ff);
  graf_fancy(&g.graf,180, 80,0x00,7,0,16,0,0x404040ff);

  graf_flush(&g.graf);
}
