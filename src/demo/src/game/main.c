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
  
  egg_play_song(RID_song_eternal_torment,0,1);
  
  g.texid_tiles=egg_texture_new();
  if (egg_texture_load_image(g.texid_tiles,RID_image_tiles)<0) {
    egg_log("Failed to decode tiles.");
    return -1;
  }

  //TODO

  return 0;
}

void egg_client_update(double elapsed) {
  //TODO
}

static uint32_t fb[FBW*FBH]={0};
#define SETPIXEL(x,y,r,g,b,a) fb[((y)*FBW)+(x)]=(r)|((g)<<8)|((b)<<16)|((a)<<24);

static uint8_t keyt=0;

void egg_client_render() {
  //graf_reset(&g.graf);
  //TODO Eventually the demo should use "graf", all Egg games should. But while building out the platform, I'll call the Egg Platform API directly.
  //graf_flush(&g.graf);
  
  egg_texture_clear(1);
  
  if (0) { //XXX TEMP Upload from a client-side RGBA framebuffer.
    int i=32; while (i-->0) {
      SETPIXEL(i,0    ,0x00,0x00,0xff,0xff) // blue on top
      SETPIXEL(i,FBH-1,0x00,0xff,0x00,0xff) // green on bottom
      SETPIXEL(0    ,i,0xff,0x00,0x00,0xff) // red left
      SETPIXEL(FBW-1,i,0xff,0xff,0xff,0xff) // white right
    }
    egg_texture_load_raw(1,FBW,FBH,FBW<<2,fb,sizeof(fb));
    return;
  }
  
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
  
  // Draw a textured quad.
  {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=1,
      .srctexid=g.texid_tiles,
      .alpha=0xff,
    };
    int16_t x=NS_sys_tilesize*2,y=NS_sys_tilesize*5,w=NS_sys_tilesize*3,h=NS_sys_tilesize*3;
    struct egg_render_raw vtxv[]={
      { 10, 10, x  ,y  },
      { 10, 58, x  ,y+h},
      { 58, 10, x+w,y  },
      { 58, 58, x+w,y+h},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // Draw a couple plain tiles.
  {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TILE,
      .dsttexid=1,
      .srctexid=g.texid_tiles,
      .tint=0,
      .alpha=0xff,
    };
    struct egg_render_tile vtxv[]={
      { 80, 20, 0x01,0},
      {100, 20, 0x02,0},
      {120, 20, 0x01,EGG_XFORM_XREV},
      {140, 20, 0x01,EGG_XFORM_YREV},
      {160, 20, 0x01,EGG_XFORM_XREV|EGG_XFORM_YREV},
      {180, 20, 0x01,EGG_XFORM_SWAP},
      {200, 20, 0x01,EGG_XFORM_SWAP|EGG_XFORM_XREV},
      {220, 20, 0x01,EGG_XFORM_SWAP|EGG_XFORM_YREV},
      {240, 20, 0x01,EGG_XFORM_SWAP|EGG_XFORM_XREV|EGG_XFORM_YREV},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // Draw a couple fancy tiles.
  if (1) {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_FANCY,
      .dsttexid=1,
      .srctexid=g.texid_tiles,
      .alpha=0xff,
      .filter=1,
    };
    keyt++;
    // x,y,tileid,xform,rotation,size,tr,tg,tb,ta,pr,pg,pb,a
    struct egg_render_fancy vtxv[]={
      { 80, 60,0x51,0,0x00,NS_sys_tilesize,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0xff},
      {100, 60,0x51,0,0x00,12             ,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff},
      {120, 60,0x51,0,0x00,20             ,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff},
      {140, 60,0x51,0,keyt,NS_sys_tilesize,0x00,0x00,0x00,0x00,0xff,0x00,0x80,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // Draw a fancy with each xform.
  {
    struct egg_render_uniform un={
      .mode=EGG_RENDER_FANCY,
      .dsttexid=1,
      .srctexid=g.texid_tiles,
      .alpha=0xff,
      .filter=0,
    };
    uint8_t t=keyt;
    struct egg_render_fancy vtxv[]={
      { 20,100,0x51,0,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0xff},
      { 60,100,0x51,1,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0xff,0x80,0x00,0xff},
      {100,100,0x51,2,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0xff,0xff,0x00,0xff},
      {140,100,0x51,3,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0xff},
      {180,100,0x51,4,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff},
      {220,100,0x51,5,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff},
      {260,100,0x51,6,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0xff,0x00,0xff,0xff},
      {300,100,0x51,7,t,NS_sys_tilesize*3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
}
