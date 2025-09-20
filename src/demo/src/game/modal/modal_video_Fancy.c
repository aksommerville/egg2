#include "../demo.h"
#include "../gui/gui.h"

#define VTX_LIMIT 2048
#define HALFTILE 8
#define ANIMATE_RATE (1.0/1.000)
#define TILE_SIZE_MIN 1
#define TILE_SIZE_MAX 64

static const struct delta {
  int x,y;
} deltav[]={
  // px/frame. Safe to add and remove. There must be at least one.
  { 0, 0},
  {-2,-2},
  {-2,-1},
  {-2, 0},
  {-2, 1},
  {-2, 2},
  {-1,-2},
  {-1,-1},
  {-1, 0},
  {-1, 1},
  {-1, 2},
  { 0,-2},
  { 0,-1},
  { 0, 0},
  { 0, 1},
  { 0, 2},
  { 1,-2},
  { 1,-1},
  { 1, 0},
  { 1, 1},
  { 1, 2},
  { 2,-2},
  { 2,-1},
  { 2, 0},
  { 2, 1},
  { 2, 2},
};

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

struct modal_video_Fancy {
  struct modal hdr;
  struct egg_render_fancy vtxv[VTX_LIMIT];
  int vtxc;
  int unmodep;
  int filter;
  double animate; // For the zero-alpha unmodes.
  int msg_texid,msg_w,msg_h;
  int mode_texid,mode_w,mode_h;
};

#define MODAL ((struct modal_video_Fancy*)modal)

/* Delete.
 */
 
static void _video_Fancy_del(struct modal *modal) {
  egg_texture_del(MODAL->msg_texid);
  egg_texture_del(MODAL->mode_texid);
}

/* Add or remove vertices.
 */
 
static void Fancy_set_count(struct modal *modal,int nc) {
  if (nc<0) nc=0;
  else if (nc>VTX_LIMIT) nc=VTX_LIMIT;
  if (nc==MODAL->vtxc) return;
  
  // Adding tiles?
  if (nc>MODAL->vtxc) {
    int addc=nc-MODAL->vtxc;
    struct egg_render_fancy *vtx=MODAL->vtxv+MODAL->vtxc;
    for (;addc-->0;vtx++) {
      vtx->x=rand()%FBW;
      vtx->y=rand()%FBH;
      vtx->tileid=0x00; // Man in a hat, natural position.
      vtx->xform=rand()&7;
      vtx->rotation=rand(); // Will animate.
      vtx->size=TILE_SIZE_MIN+((rand()&0x7fff)*(TILE_SIZE_MAX-TILE_SIZE_MIN))/0x7fff;
      vtx->tr=rand();
      vtx->tg=rand();
      vtx->tb=rand();
      vtx->ta=rand();
      vtx->pr=rand();
      vtx->pg=rand();
      vtx->pb=rand();
      while (!(vtx->a=rand())) ; // Not animated, don't let it be zero.
    }
  }
  MODAL->vtxc=nc;
  
  // Update message.
  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"Dpad,L1,R1 to adjust parameters.\nSOUTH to pause, EAST to toggle filter.\n%d tiles.",MODAL->vtxc);
  if ((msgc<1)||(msgc>=sizeof(msg))) msgc=0;
  font_render_to_texture(MODAL->msg_texid,g.font,msg,msgc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->msg_w,&MODAL->msg_h,MODAL->msg_texid);
}

/* Change uniform mode.
 */
 
static void Fancy_adjust_unmode(struct modal *modal,int d) {
  if (d) egg_play_sound(RID_sound_uimotion,0.5,(d<0)?-0.5:0.5); // "if (d)" because we do this at init to poke the texture.
  MODAL->unmodep+=d;
  int c=sizeof(unmodev)/sizeof(struct unmode);
  if (MODAL->unmodep<0) MODAL->unmodep=c-1;
  else if (MODAL->unmodep>=c) MODAL->unmodep=0;
  
  char msg[256];
  int msgc=snprintf(msg,sizeof(msg),"Uniform mode %d/%d '%s' filter=%d",MODAL->unmodep,c,unmodev[MODAL->unmodep].name,MODAL->filter);
  if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
  font_render_to_texture(MODAL->mode_texid,g.font,msg,msgc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->mode_w,&MODAL->mode_h,MODAL->mode_texid);
}

/* Update.
 */
 
static void _video_Fancy_update(struct modal *modal,double elapsed,int input,int pvinput) {

  // Advance animation, but hold SOUTH to pause it.
  if (!(input&EGG_BTN_SOUTH)) {
    MODAL->animate+=elapsed*ANIMATE_RATE;
    if (MODAL->animate>=1.0) MODAL->animate-=1.0;
  }

  /* Move all the tiles.
   * In real life, your sprites should have floating-point coordinates, and move based on (elapsed).
   * I don't want a second list, so we're moving a fixed amount per frame, naturally quantized to 1 px/frame.
   * This also means we don't have anywhere to store a sprite's delta,
   * so their delta is a function of their index, and they wrap around after leaving the screen.
   * This counts as animation too, so pause when SOUTH is held.
   */
  if (!(input&EGG_BTN_SOUTH)) {
    int deltac=sizeof(deltav)/sizeof(struct delta);
    struct egg_render_fancy *vtx=MODAL->vtxv;
    int i=0;
    for (;i<MODAL->vtxc;i++,vtx++) {
      const struct delta *delta=deltav+i%deltac;
      vtx->x+=delta->x;
      if (vtx->x<-HALFTILE) vtx->x=FBW+HALFTILE;
      else if (vtx->x>FBW+HALFTILE) vtx->x=-HALFTILE;
      vtx->y+=delta->y;
      if (vtx->y<-HALFTILE) vtx->y=FBH+HALFTILE;
      else if (vtx->y>FBH+HALFTILE) vtx->y=-HALFTILE;
      vtx->rotation++;
    }
  }
  
  /* UP or DOWN to adjust the tile count.
   * Hold L1 and/or R1 to up the delta.
   */
  int d;
  switch (input&(EGG_BTN_L1|EGG_BTN_R1)) {
    case 0: d=1; break;
    case EGG_BTN_L1: d=10; break;
    case EGG_BTN_R1: d=50; break;
    case EGG_BTN_L1|EGG_BTN_R1: d=500; break;
  }
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) Fancy_set_count(modal,MODAL->vtxc+d);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) Fancy_set_count(modal,MODAL->vtxc-d);
  
  /* LEFT or RIGHT to adjust the uniform mode.
   */
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) Fancy_adjust_unmode(modal,-1);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) Fancy_adjust_unmode(modal,1);
  
  /* EAST to toggle filter.
   * Do a noop unmode adjustment after, to update the message.
   */
  if ((input&EGG_BTN_EAST)&&!(pvinput&EGG_BTN_EAST)) {
    MODAL->filter=MODAL->filter?0:1;
    Fancy_adjust_unmode(modal,0);
  }
}

/* Render.
 */
 
static void _video_Fancy_render(struct modal *modal) {
  graf_flush(&g.graf);
  
  // Back-and-forth position, derived from (MODAL->animate).
  double t=MODAL->animate*2.0;
  if (t>=1.0) t=2.0-t;
  
  const struct unmode *unmode=unmodev+MODAL->unmodep;
  struct egg_render_uniform un={
    .mode=EGG_RENDER_FANCY,
    .dsttexid=1,
    .srctexid=graf_tex(&g.graf,RID_image_tiles),
    .tint=unmode->tint,
    .alpha=unmode->alpha,
    .filter=MODAL->filter,
  };
  if (!un.alpha) un.alpha=(int)(t*255.0f);
  egg_render(&un,MODAL->vtxv,sizeof(struct egg_render_fancy)*MODAL->vtxc);
  
  // And finally, two text labels that are not part of the test. Resume using graf.
  graf_set_input(&g.graf,MODAL->msg_texid);
  graf_decal(&g.graf,10,130,0,0,MODAL->msg_w,MODAL->msg_h);
  graf_set_input(&g.graf,MODAL->mode_texid);
  graf_decal(&g.graf,10,130+MODAL->msg_h+10,0,0,MODAL->mode_w,MODAL->mode_h);
}

/* Initialize.
 */
 
static int _video_Fancy_init(struct modal *modal) {
  modal->del=_video_Fancy_del;
  modal->update=_video_Fancy_update;
  modal->render=_video_Fancy_render;
  
  MODAL->filter=1;
  MODAL->mode_texid=egg_texture_new();
  Fancy_adjust_unmode(modal,0); // To generate the initial message texture.
  MODAL->msg_texid=egg_texture_new();
  Fancy_set_count(modal,100);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_video_Fancy() {
  struct modal *modal=modal_new(sizeof(struct modal_video_Fancy));
  if (!modal) return 0;
  if (
    (_video_Fancy_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
