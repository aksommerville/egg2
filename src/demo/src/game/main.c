#include "demo.h"

struct g g={0};

void egg_client_quit(int status) {
}

char rom_tmp[128<<10];//TODO really need stdlib

int egg_client_init() {

  {
    int screenw=0,screenh=0;
    egg_video_get_screen_size(&screenw,&screenh);
    char msg[]={
      's','c','r','e','e','n',':',' ',
      '0'+(screenw/1000)%10,
      '0'+(screenw/ 100)%10,
      '0'+(screenw/  10)%10,
      '0'+(screenw     )%10,
      'x',
      '0'+(screenh/1000)%10,
      '0'+(screenh/ 100)%10,
      '0'+(screenh/  10)%10,
      '0'+(screenh     )%10,
      0,
    };
    egg_log(msg);
  }

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  {
    char msg[]={
      'f','b',':',' ',
      '0'+(fbw/1000)%10,
      '0'+(fbw/ 100)%10,
      '0'+(fbw/  10)%10,
      '0'+(fbw     )%10,
      'x',
      '0'+(fbh/1000)%10,
      '0'+(fbh/ 100)%10,
      '0'+(fbh/  10)%10,
      '0'+(fbh     )%10,
      0,
    };
    egg_log(msg);
  }
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Framebuffer size mismatch! metadata=%dx%d header=%dx%d\n",fbw,fbh,FBW,FBH);
    return -1;
  }

  g.romc=egg_rom_get(0,0);
  if (g.romc>sizeof(rom_tmp)) {
    egg_log("rom too large");
    return -1;
  }
  g.rom=rom_tmp;
  egg_rom_get(g.rom,g.romc);
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
  
  egg_play_song(RID_song_hold_your_fire,0,1);
  
  g.texid_tiles=egg_texture_new();
  if (egg_texture_load_image(g.texid_tiles,RID_image_tiles)<0) {
    egg_log("Failed to decode tiles.");
    return -1;
  }
  
  egg_event_enable(EGG_EVENT_KEY,0);
  egg_event_enable(EGG_EVENT_TEXT,0);
  egg_event_enable(EGG_EVENT_MMOTION,0);
  egg_event_enable(EGG_EVENT_MBUTTON,0);
  egg_event_enable(EGG_EVENT_MWHEEL,0);
  egg_event_enable(EGG_EVENT_TOUCH,0);
  egg_event_enable(EGG_EVENT_GAMEPAD,0);
  // Additional input features enabled like events (but are not really events per se):
  egg_event_enable(EGG_EVENT_HIDECURSOR,0);
  egg_event_enable(EGG_EVENT_LOCKCURSOR,0);
  egg_event_enable(EGG_EVENT_NOMAPCURSOR,0);

  //TODO

  return 0;
}

static int decsint_repr(char *dst,int v) {
  int dstc=0;
  if (v<0) {
    dst[dstc++]='-';
    int limit=-10,digitc=1;
    while (v<=limit) { digitc++; if (limit<INT_MIN/10) break; limit*=10; }
    int i=digitc; for (;i-->0;v/=10) dst[dstc+i]='0'-v%10;
    dstc+=digitc;
  } else {
    int limit=10,digitc=1;
    while (v>=limit) { digitc++; if (limit>INT_MAX/10) break; limit*=10; }
    int i=digitc; for (;i-->0;v/=10) dst[dstc+i]='0'+v%10;
    dstc+=digitc;
  }
  dst[dstc++]=' ';
  return dstc;
}

static void log_event(const char *name,int a,int b,int c,int d) {
  char msg[256];
  int msgc=0;
  for (;*name;name++) msg[msgc++]=*name;
  msg[msgc++]=' ';
  if (a||b||c||d) msgc+=decsint_repr(msg+msgc,a);
  if (b||c||d) msgc+=decsint_repr(msg+msgc,b);
  if (c||d) msgc+=decsint_repr(msg+msgc,c);
  if (d) msgc+=decsint_repr(msg+msgc,d);
  msg[msgc]=0;
  egg_log(msg);
}

static int pvinput=0;

void egg_client_update(double elapsed) {

  // Report aggregate input state.
  int input=egg_input_get_one(0);
  if (input!=pvinput) {
    char msg[32]="INPUT: ";
    int msgc=7;
    if (input&EGG_BTN_SOUTH) msg[msgc++]='s'; else msg[msgc++]='.';
    if (input&EGG_BTN_EAST ) msg[msgc++]='e'; else msg[msgc++]='.';
    if (input&EGG_BTN_WEST ) msg[msgc++]='w'; else msg[msgc++]='.';
    if (input&EGG_BTN_NORTH) msg[msgc++]='n'; else msg[msgc++]='.';
    if (input&EGG_BTN_L1   ) msg[msgc++]='l'; else msg[msgc++]='.';
    if (input&EGG_BTN_R1   ) msg[msgc++]='r'; else msg[msgc++]='.';
    if (input&EGG_BTN_L2   ) msg[msgc++]='L'; else msg[msgc++]='.';
    if (input&EGG_BTN_R2   ) msg[msgc++]='R'; else msg[msgc++]='.';
    if (input&EGG_BTN_AUX2 ) msg[msgc++]='2'; else msg[msgc++]='.';
    if (input&EGG_BTN_AUX1 ) msg[msgc++]='1'; else msg[msgc++]='.';
    if (input&EGG_BTN_AUX3 ) msg[msgc++]='3'; else msg[msgc++]='.';
    if (input&EGG_BTN_CD   ) msg[msgc++]='#'; else msg[msgc++]='.';
    if (input&EGG_BTN_UP   ) msg[msgc++]='u'; else msg[msgc++]='.';
    if (input&EGG_BTN_DOWN ) msg[msgc++]='d'; else msg[msgc++]='.';
    if (input&EGG_BTN_LEFT ) msg[msgc++]='l'; else msg[msgc++]='.';
    if (input&EGG_BTN_RIGHT) msg[msgc++]='r'; else msg[msgc++]='.';
    msg[msgc]=0;
    egg_log(msg);
    pvinput=input;
  }
  
  // Pop and log events.
  struct egg_event eventv[32];
  int eventc;
  for (;;) {
    eventc=egg_event_get(eventv,32);
    if (eventc<1) break;
    const struct egg_event *event=eventv;
    int i=eventc;
    for (;i-->0;event++) {
      switch (event->type) {
        case EGG_EVENT_KEY: log_event("KEY",event->key.keycode,event->key.value,0,0); break;
        case EGG_EVENT_TEXT: log_event("TEXT",event->text.codepoint,0,0,0); break;
        case EGG_EVENT_MMOTION: log_event("MMOTION",event->mmotion.x,event->mmotion.y,0,0); break;
        case EGG_EVENT_MBUTTON: log_event("MBUTTON",event->mbutton.x,event->mbutton.y,event->mbutton.btnid,event->mbutton.value); break;
        case EGG_EVENT_MWHEEL: log_event("MWHEEL",event->mwheel.x,event->mwheel.y,event->mwheel.dx,event->mwheel.dy); break;
        case EGG_EVENT_TOUCH: log_event("TOUCH",event->touch.x,event->touch.y,event->touch.touchid,event->touch.state); break;
        case EGG_EVENT_GAMEPAD: log_event("GAMEPAD",event->gamepad.devid,event->gamepad.btnid,event->gamepad.value,0); break;
        default: log_event("???",event->type,0,0,0); break;
      }
    }
    if (eventc<32) break;
  }
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
