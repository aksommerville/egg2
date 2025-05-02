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

  g.romc=egg_rom_get(0,0);
  //if (!(g.rom=malloc(g.romc))) return -1;//TODO need stdlib
  //egg_rom_get(g.rom,g.romc);
  
  // TEMP: Report rom size.
  char msg[]="rom size 0000000";
  msg[ 9]='0'+(g.romc/1000000)%10;
  msg[10]='0'+(g.romc/ 100000)%10;
  msg[11]='0'+(g.romc/  10000)%10;
  msg[12]='0'+(g.romc/   1000)%10;
  msg[13]='0'+(g.romc/    100)%10;
  msg[14]='0'+(g.romc/     10)%10;
  msg[15]='0'+(g.romc        )%10;
  egg_log(msg);
  
  // TEMP: Try load and save.
  char v[256];
  int vc=egg_store_get(v,sizeof(v),"mySavedGame",11);
  if ((vc>0)&&(vc<sizeof(v))) {
    v[vc]=0;
    egg_log("Acquired saved game:");
    egg_log(v);
  } else {
    egg_log("Failed to load saved game.");
  }
  if (egg_store_set("mySavedGame",11,"Abcdefghi",9)<0) {
    egg_log("Failed to save game.");
  } else {
    egg_log("Saved game.");
  }
  
  egg_play_song(RID_song_in_thru_the_window,0,1);

  //TODO

  return 0;
}

void egg_client_update(double elapsed) {
  //TODO
}

void egg_client_render() {
  //graf_reset(&g.graf);
  //TODO Eventually the demo should use "graf", all Egg games should. But while building out the platform, I'll call the Egg Platform API directly.
  //graf_flush(&g.graf);
  
  egg_texture_clear(1);
  
  // Fill the framebuffer with brown. And you can see why "graf" needs to exist, this is way too much ceremony!
  {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=1,
      .alpha=0xff,
    };
    struct egg_render_raw vtxv[]={
      {  0,  0, 0,0, 0x80,0x40,0x20,0xff},
      {  0,FBH, 0,0, 0x80,0x40,0x20,0xff},
      {FBW,  0, 0,0, 0x80,0x40,0x20,0xff},
      {FBW,FBH, 0,0, 0x80,0x40,0x20,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // Draw a gradient-filled triangle: Red on top, green lower left, and blue lower right.
  {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLES,
      .dsttexid=1,
      .alpha=0xff,
    };
    const int margin=10;
    struct egg_render_raw vtxv[]={
      {    FBW>>1,    margin,0,0,0xff,0x00,0x00,0xff},
      {    margin,FBH-margin,0,0,0x00,0xff,0x00,0xff},
      {FBW-margin,FBH-margin,0,0,0x00,0x00,0xff,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
}
