#include "../demo.h"
#include "../gui/gui.h"

struct modal_video_Transforms {
  struct modal hdr;
};

#define MODAL ((struct modal_video_Transforms*)modal)

/* Delete.
 */
 
static void _video_Transforms_del(struct modal *modal) {
}

/* Update.
 */
 
static void _video_Transforms_update(struct modal *modal,double elapsed,int input,int pvinput) {
}

/* Render.
 */
 
static void _video_Transforms_render(struct modal *modal) {
  const int colc=8; // One per xform.
  const int rowc=4; // Ref,Tile,Fancy,Decal.
  const int colstride=20;
  const int rowstride=20;
  const int fullw=colstride*colc;
  const int fullh=rowstride*rowc;
  const int x0=(FBW>>1)-(fullw>>1);
  const int y0=(FBH>>1)-(fullh>>1);
  const int xa=x0+(colstride>>1);
  const int ya=y0+(rowstride>>1);
  #define POS(col,row) xa+(col)*colstride,ya+(row)*rowstride /* center */
  #define POS0(col,row) xa+(col)*colstride-8,ya+(row)*rowstride-8 /* top-left */
  
  graf_set_image(&g.graf,RID_image_tiles);
  
  /* Reference row on top: Tiles 0x00..0x07.
   */
  graf_tile(&g.graf,POS(0,0),0x00,0);
  graf_tile(&g.graf,POS(1,0),0x01,0);
  graf_tile(&g.graf,POS(2,0),0x02,0);
  graf_tile(&g.graf,POS(3,0),0x03,0);
  graf_tile(&g.graf,POS(4,0),0x04,0);
  graf_tile(&g.graf,POS(5,0),0x05,0);
  graf_tile(&g.graf,POS(6,0),0x06,0);
  graf_tile(&g.graf,POS(7,0),0x07,0);
  
  /* Regular TILE 0x00 with xforms.
   */
  graf_tile(&g.graf,POS(0,1),0x00,0);
  graf_tile(&g.graf,POS(1,1),0x00,1);
  graf_tile(&g.graf,POS(2,1),0x00,2);
  graf_tile(&g.graf,POS(3,1),0x00,3);
  graf_tile(&g.graf,POS(4,1),0x00,4);
  graf_tile(&g.graf,POS(5,1),0x00,5);
  graf_tile(&g.graf,POS(6,1),0x00,6);
  graf_tile(&g.graf,POS(7,1),0x00,7);
  
  /* FANCY 0x00 with xforms.
   */
  #define TILEISH_FANCY ,0,16,0,0x808080ff /* We're using fancies as if they were plain tiles, the extra bells and whistles don't matter. */
  graf_fancy(&g.graf,POS(0,2),0x00,0 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(1,2),0x00,1 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(2,2),0x00,2 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(3,2),0x00,3 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(4,2),0x00,4 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(5,2),0x00,5 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(6,2),0x00,6 TILEISH_FANCY);
  graf_fancy(&g.graf,POS(7,2),0x00,7 TILEISH_FANCY);
  #undef TILEISH_FANCY
  
  /* graf_decal_xform().
   */
  graf_decal_xform(&g.graf,POS0(0,3),0,0,16,16,0);
  graf_decal_xform(&g.graf,POS0(1,3),0,0,16,16,1);
  graf_decal_xform(&g.graf,POS0(2,3),0,0,16,16,2);
  graf_decal_xform(&g.graf,POS0(3,3),0,0,16,16,3);
  graf_decal_xform(&g.graf,POS0(4,3),0,0,16,16,4);
  graf_decal_xform(&g.graf,POS0(5,3),0,0,16,16,5);
  graf_decal_xform(&g.graf,POS0(6,3),0,0,16,16,6);
  graf_decal_xform(&g.graf,POS0(7,3),0,0,16,16,7);
  
  #undef POS
}

/* Initialize.
 */
 
static int _video_Transforms_init(struct modal *modal) {
  modal->del=_video_Transforms_del;
  modal->update=_video_Transforms_update;
  modal->render=_video_Transforms_render;
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_video_Transforms() {
  struct modal *modal=modal_new(sizeof(struct modal_video_Transforms));
  if (!modal) return 0;
  if (
    (_video_Transforms_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
