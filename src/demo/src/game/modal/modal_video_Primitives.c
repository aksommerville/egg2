#include "../demo.h"
#include "../gui/gui.h"

#define PRIMITIVES_ANIMATE_RATE (1.0/1.000) /* Hz. (denominator is period) */

static const struct unmode {
  const char *name;
  uint32_t tint;
  uint8_t alpha;
} unmodev[]={
  // It's safe to add, remove, and reorder modes.
  // Alpha zero causes the renderer to pingpong alpha between zero and one.
  {"Default",    0x00000000,0xff},
  {"Var Alpha",  0x00000000,0x00},
  {"Half Alpha", 0x00000000,0x80},
  {"Tint Red",   0xff0000ff,0xff},
  {"Half Red",   0xff000080,0xff},
  {"Tint+Alpha", 0x00ff00ff,0x80},
};

struct modal_video_Primitives {
  struct modal hdr;
  double animate; // 0..1, regardless of period.
  int unmodep;
  int msg_texid,msg_w,msg_h;
  int mode_texid,mode_w,mode_h;
};

#define MODAL ((struct modal_video_Primitives*)modal)

/* Delete.
 */
 
static void _video_Primitives_del(struct modal *modal) {
  egg_texture_del(MODAL->msg_texid);
  egg_texture_del(MODAL->mode_texid);
}

/* Change uniform mode.
 */
 
static void Primitives_adjust_unmode(struct modal *modal,int d) {
  if (d) egg_play_sound(RID_sound_uimotion,0.5,(d<0)?-0.5:0.5); // "if (d)" because we do this at init to poke the texture.
  MODAL->unmodep+=d;
  int c=sizeof(unmodev)/sizeof(struct unmode);
  if (MODAL->unmodep<0) MODAL->unmodep=c-1;
  else if (MODAL->unmodep>=c) MODAL->unmodep=0;
  
  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"Uniform mode %d/%d '%s'",MODAL->unmodep,c,unmodev[MODAL->unmodep].name);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  font_render_to_texture(MODAL->mode_texid,g.font,msg,msgc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->mode_w,&MODAL->mode_h,MODAL->mode_texid);
}

/* Update.
 */
 
static void _video_Primitives_update(struct modal *modal,double elapsed,int input,int pvinput) {

  // Tap L1 to advance one frame while paused.
  // Do this by fudging (input) to pretend it's not pressed.
  if (input&EGG_BTN_SOUTH) {
    if ((input&EGG_BTN_L1)&&!(pvinput&EGG_BTN_L1)) input&=~EGG_BTN_SOUTH;
  }

  // Hold SOUTH to pause animation.
  if (!(input&EGG_BTN_SOUTH)) {
    MODAL->animate+=elapsed*PRIMITIVES_ANIMATE_RATE;
    if (MODAL->animate>=1.0) MODAL->animate-=1.0;
  }
  
  // LEFT or RIGHT to change the uniform mode.
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) Primitives_adjust_unmode(modal,-1);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) Primitives_adjust_unmode(modal,1);
}

/* Render.
 */
 
static void _video_Primitives_render(struct modal *modal) {
  graf_flush(&g.graf); // We won't use graf; we want to test Egg Platform API directly.
  
  { // Fill background with black. This is not part of the test, and does not use the shared uniforms.
    struct egg_render_uniform un={
      .mode=EGG_RENDER_TRIANGLE_STRIP,
      .dsttexid=1,
      .alpha=0xff,
    };
    struct egg_render_raw vtxv[]={
      {  0,  0, 0,0, 0x00,0x00,0x00,0xff},
      {FBW,  0, 0,0, 0x00,0x00,0x00,0xff},
      {  0,FBH, 0,0, 0x00,0x00,0x00,0xff},
      {FBW,FBH, 0,0, 0x00,0x00,0x00,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // Back-and-forth position, derived from (MODAL->animate).
  double t=MODAL->animate*2.0;
  if (t>=1.0) t=2.0-t;
  
  const struct unmode *unmode=unmodev+MODAL->unmodep;
  struct egg_render_uniform un={
    .mode=0,
    .dsttexid=1,
    .tint=unmode->tint,
    .alpha=unmode->alpha,
  };
  if (!un.alpha) { // Var Alpha: Ping-pong the global alpha.
    un.alpha=(int)(t*255.0f);
  }
  
  {
    un.mode=EGG_RENDER_POINTS;
    struct egg_render_raw vtxv[]={
      {  1,  1, 0,0, 0xff,0xff,0xff,0xff},
      {  3,  1, 0,0, 0xff,0x00,0x00,0xff},
      {  1,  3, 0,0, 0x00,0xff,0x00,0xff},
      {  3,  3, 0,0, 0x00,0x00,0xff,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  {
    un.mode=EGG_RENDER_LINES;
    const int lx=10;
    const int rx=60;
    const int ty=1;
    const int by=50;
    int ax=lx+(rx-lx)*t;
    int bx=rx+(lx-rx)*t;
    struct egg_render_raw vtxv[]={
      {bx,ty, 0,0, 0x80,0x00,0xff,0xff},
      {ax,by, 0,0, 0x00,0x00,0xff,0xff},
      {ax,ty, 0,0, 0xff,0xff,0xff,0xff},
      {bx,by, 0,0, 0x00,0xff,0x00,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  {
    un.mode=EGG_RENDER_LINE_STRIP;
    const int lx=100;
    const int rx=150;
    const int ty=1;
    const int by=50;
    struct egg_render_raw vtxv[]={
      {lx   ,ty   , 0,0, 0xff,0xff,0xff,0xff},
      {rx   ,ty   , 0,0, 0xff,0x00,0x00,0xff},
      {rx   ,by   , 0,0, 0x00,0xff,0x00,0xff},
      {lx   ,by   , 0,0, 0x00,0x00,0xff,0xff},
      {lx   ,ty+2 , 0,0, 0xc0,0xc0,0xc0,0xff},
      {rx-2 ,ty+2 , 0,0, 0xc0,0x00,0x00,0xff},
      {rx-2 ,by-2 , 0,0, 0x00,0xc0,0x00,0xff},
      {lx+2 ,by-2 , 0,0, 0x00,0x00,0xc0,0xff},
      {lx+2 ,ty+4 , 0,0, 0x80,0x80,0x80,0xff},
      {rx-4 ,ty+4 , 0,0, 0x80,0x00,0x00,0xff},
      {rx-4 ,by-4 , 0,0, 0x00,0x80,0x00,0xff},
      {lx+4 ,by-4 , 0,0, 0x00,0x00,0x80,0xff},
      {lx+4 ,ty+6 , 0,0, 0x40,0x40,0x40,0xff},
      {rx-6 ,ty+6 , 0,0, 0x40,0x00,0x00,0xff},
      {rx-6 ,by-6 , 0,0, 0x00,0x40,0x00,0xff},
      {lx+6 ,by-6 , 0,0, 0x00,0x00,0x40,0xff},
     };
     egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  {
    un.mode=EGG_RENDER_TRIANGLES;
    const int lx=200;
    const int rx=250;
    const int ty=1;
    const int by=50;
    int ax=lx+(rx-lx)*t;
    struct egg_render_raw vtxv[]={
      {ax,ty,0,0,0xff,0x00,0x00,0xff},
      {rx,by,0,0,0x00,0xff,0x00,0xff},
      {lx,by,0,0,0x00,0x00,0xff,0xff},
      // A faint second triangle, to ensure we're not making a strip or anything:
      {lx,ty,0,0,0xff,0xff,0xff,0x40},
      {rx,ty,0,0,0xff,0xff,0xff,0x40},
      {rx,by,0,0,0xff,0xff,0xff,0x40},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  {
    un.mode=EGG_RENDER_TRIANGLE_STRIP;
    const int lx=280;
    const int rx=310;
    const int ty=1;
    const int ystep=10;
    struct egg_render_raw vtxv[]={
      {lx,ty+ystep*0 ,0,0, 0xff,0xff,0xff,0xff},
      {rx,ty+ystep*0 ,0,0, 0xff,0xff,0xff,0xff},
      {lx,ty+ystep*1 ,0,0, 0xff,0x00,0x00,0xff},
      {rx,ty+ystep*1 ,0,0, 0xff,0x00,0x00,0xff},
      {lx,ty+ystep*2 ,0,0, 0x00,0xff,0x00,0xff},
      {rx,ty+ystep*2 ,0,0, 0x00,0xff,0x00,0xff},
      {lx,ty+ystep*3 ,0,0, 0x00,0x00,0xff,0xff},
      {rx,ty+ystep*3 ,0,0, 0x00,0x00,0xff,0xff},
      {lx,ty+ystep*4 ,0,0, 0xff,0xff,0x00,0xff},
      {rx,ty+ystep*4 ,0,0, 0xff,0xff,0x00,0xff},
      {lx,ty+ystep*5 ,0,0, 0xff,0x00,0xff,0xff},
      {rx,ty+ystep*5 ,0,0, 0xff,0x00,0xff,0xff},
      {lx,ty+ystep*6 ,0,0, 0x00,0xff,0xff,0xff},
      {rx,ty+ystep*6 ,0,0, 0x00,0xff,0xff,0xff},
      {lx,ty+ystep*7 ,0,0, 0x80,0x00,0x00,0xff},
      {rx,ty+ystep*7 ,0,0, 0x80,0x00,0x00,0xff},
      {lx,ty+ystep*8 ,0,0, 0x80,0x80,0x80,0xff},
      {rx,ty+ystep*8 ,0,0, 0x80,0x80,0x80,0xff},
      {lx,ty+ystep*9 ,0,0, 0x00,0x00,0x00,0xff},
      {rx,ty+ystep*9 ,0,0, 0x00,0x00,0x00,0xff},
    };
    egg_render(&un,vtxv,sizeof(vtxv));
  }
  
  // And finally, two text labels that are not part of the test. Resume using graf.
  graf_set_input(&g.graf,MODAL->msg_texid);
  graf_decal(&g.graf,10,100,0,0,MODAL->msg_w,MODAL->msg_h);
  graf_set_input(&g.graf,MODAL->mode_texid);
  graf_decal(&g.graf,10,100+MODAL->msg_h+10,0,0,MODAL->mode_w,MODAL->mode_h);
}

/* Initialize.
 */
 
static int _video_Primitives_init(struct modal *modal) {
  modal->del=_video_Primitives_del;
  modal->update=_video_Primitives_update;
  modal->render=_video_Primitives_render;
  modal->opaque=1;
  
  MODAL->msg_texid=egg_texture_new();
  font_render_to_texture(MODAL->msg_texid,g.font,
    "Hold SOUTH to pause animation.\n"
    "Tap L1 while paused to step one frame.\n"
    "LEFT and RIGHT to change uniform mode.\n"
  ,-1,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->msg_w,&MODAL->msg_h,MODAL->msg_texid);
  
  MODAL->mode_texid=egg_texture_new();
  Primitives_adjust_unmode(modal,0); // To generate the initial message texture.
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_video_Primitives() {
  struct modal *modal=modal_new(sizeof(struct modal_video_Primitives));
  if (!modal) return 0;
  if (
    (_video_Primitives_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
