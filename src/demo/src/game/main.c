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
  
  if ((g.label_texid=font_render_to_texture(0,g.font,
    "The quick brown fox jumps over the lazy dog 1234567890 times.\n"
    "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 1234567890 TIMES!\n"
    "Those first two lines were terminated by LFs. The remainder of this text is not, and we should see it breaking sensibly."
  ,-1,FBW,FBH,0xffffffff))<1) return -1;
  egg_texture_get_size(&g.label_w,&g.label_h,g.label_texid);

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
  
  { // Text, rendered at init.
    int dstx=(FBW>>1)-(g.label_w>>1);
    int dsty=(FBH>>1)-(g.label_h>>1)+30;
    graf_set_input(&g.graf,g.label_texid);
    graf_set_tint(&g.graf,0x000000ff);
    graf_set_alpha(&g.graf,0x80);
    graf_decal(&g.graf,dstx,dsty,0,0,g.label_w,g.label_h);
    graf_set_tint(&g.graf,0);
    graf_set_alpha(&g.graf,0xff);
    graf_decal(&g.graf,dstx-1,dsty-1,0,0,g.label_w,g.label_h);
  }

  graf_flush(&g.graf);
}
