#include "../demo.h"
#include "../gui/gui.h"

struct modal_video_Clipping {
  struct modal hdr;
  int msg_texid,msg_w,msg_h;
  uint8_t xform;
  int dx,dy;
};

#define MODAL ((struct modal_video_Clipping*)modal)

/* Delete.
 */
 
static void _video_Clipping_del(struct modal *modal) {
  egg_texture_del(MODAL->msg_texid);
}

/* Adjust tiles position.
 */
 
static void Clipping_move(struct modal *modal,int dx,int dy) {
  MODAL->dx+=dx;
  MODAL->dy+=dy;
}

/* Update.
 */
 
static void _video_Clipping_update(struct modal *modal,double elapsed,int input,int pvinput) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: Clipping_move(modal,-1,0); break;
    case EGG_BTN_RIGHT: Clipping_move(modal,1,0); break;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: Clipping_move(modal,0,-1); break;
    case EGG_BTN_DOWN: Clipping_move(modal,0,1); break;
  }
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    MODAL->xform=(MODAL->xform+1)&7;
  }
}

/* Render.
 */
 
static void _video_Clipping_render(struct modal *modal) {
  graf_set_image(&g.graf,RID_image_tiles);

  /* TILE near each of the edges.
   */
  graf_tile(&g.graf,MODAL->dx,(FBH>>1)-10,0x00,MODAL->xform);
  graf_tile(&g.graf,(FBW>>1)-10,MODAL->dy,0x00,MODAL->xform);
  graf_tile(&g.graf,FBW+MODAL->dx,(FBH>>1)-10,0x00,MODAL->xform);
  graf_tile(&g.graf,(FBW>>1)-10,FBH+MODAL->dy,0x00,MODAL->xform);
   
  /* FANCY next to each of those tiles.
   */
  #define TILEISH_FANCY ,0,16,0,0x808080ff /* We're using fancies as if they were plain tiles, the extra bells and whistles don't matter. */
  graf_fancy(&g.graf,MODAL->dx,(FBH>>1)+10,0x00,MODAL->xform TILEISH_FANCY);
  graf_fancy(&g.graf,(FBW>>1)+10,MODAL->dy,0x00,MODAL->xform TILEISH_FANCY);
  graf_fancy(&g.graf,FBW+MODAL->dx,(FBH>>1)+10,0x00,MODAL->xform TILEISH_FANCY);
  graf_fancy(&g.graf,(FBW>>1)+10,FBH+MODAL->dy,0x00,MODAL->xform TILEISH_FANCY);
  #undef TILEISH_FANCY
   
  /* Static label in the middle, explaining it.
   */
  int dstx=(FBW>>1)-(MODAL->msg_w>>1);
  int dsty=(FBH>>1)-(MODAL->msg_h>>1);
  graf_set_input(&g.graf,MODAL->msg_texid);
  graf_decal(&g.graf,dstx,dsty,0,0,MODAL->msg_w,MODAL->msg_h);
}

/* Initialize.
 */
 
static int _video_Clipping_init(struct modal *modal) {
  modal->del=_video_Clipping_del;
  modal->update=_video_Clipping_update;
  modal->render=_video_Clipping_render;
  
  MODAL->msg_texid=font_render_to_texture(0,g.font,
    "Dpad to adjust position.\n"
    "SOUTH to change xform.\n"
    "Should see two identical tiles on each edge."
  ,-1,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->msg_w,&MODAL->msg_h,MODAL->msg_texid);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_video_Clipping() {
  struct modal *modal=modal_new(sizeof(struct modal_video_Clipping));
  if (!modal) return 0;
  if (
    (_video_Clipping_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
