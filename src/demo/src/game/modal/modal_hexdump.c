#include "../demo.h"
#include "../gui/gui.h"

// In canonical form, this works out to just under 40 columns, which is what we should get. Nice and neat.
#define BYTES_PER_ROW 8

#define AUTOREPEAT_TIME 0.125

struct modal_hexdump {
  struct modal hdr;
  struct gui_term *term;
  int colc,rowc;
  int scroll; // Offset at first visible row.
  const uint8_t *v;
  int c;
  int dy; // If UP or DOWN is held, -1 or 1.
  double dyclock; // Auto scroll while holding.
};

#define MODAL ((struct modal_hexdump*)modal)

/* Delete.
 */
 
static void _hexdump_del(struct modal *modal) {
  gui_term_del(MODAL->term);
}

/* Rewrite the entire payload content.
 */
 
static void modal_hexdump_rewrite(struct modal *modal) {
  int row=1,p=MODAL->scroll;
  char *dst=gui_term_manual_edit(MODAL->term,0,1,MODAL->colc*(MODAL->rowc-1));
  if (!dst) return;
  for (;row<MODAL->rowc;row++,p+=BYTES_PER_ROW,dst+=MODAL->colc) {
    if (p>=MODAL->c) {
      memset(dst,0,MODAL->colc);
      continue;
    }
    
    // Offset, with inserted "!" if over 64k.
    int dstp=0;
    if (p>0xffff) {
      dst[dstp++]='!';
    } else {
      dst[dstp++]="0123456789abcdef"[(p>>12)&15];
    }
    dst[dstp++]="0123456789abcdef"[(p>>8)&15];
    dst[dstp++]="0123456789abcdef"[(p>>4)&15];
    dst[dstp++]="0123456789abcdef"[p&15];
    dst[dstp++]=0;
    dst[dstp++]=0;
    
    // Hex bytes.
    int i=0;
    for (;i<BYTES_PER_ROW;i++) {
      if (p+i<MODAL->c) {
        uint8_t v=MODAL->v[p+i];
        dst[dstp++]="0123456789abcdef"[v>>4];
        dst[dstp++]="0123456789abcdef"[v&15];
      } else {
        dst[dstp++]=0;
        dst[dstp++]=0;
      }
      dst[dstp++]=0;
    }
    dst[dstp++]=0;
    
    // ASCII.
    for (i=0;i<BYTES_PER_ROW;i++) {
      if (p+i<MODAL->c) {
        uint8_t v=MODAL->v[p+i];
        if ((v>=0x20)&&(v<0x7f)) {
          dst[dstp++]=v;
        } else {
          dst[dstp++]='.';
        }
      } else {
        dst[dstp++]=0;
      }
    }
  }
}

/* Update.
 */
 
static void _hexdump_update(struct modal *modal,double elapsed,int input,int pvinput) {

  int dy=0;
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) { dy=-1; MODAL->dyclock=AUTOREPEAT_TIME; }
  else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) { dy=1; MODAL->dyclock=AUTOREPEAT_TIME; }
  else if (MODAL->dy) {
    if ((MODAL->dyclock-=elapsed)<=0.0) {
      switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
        case EGG_BTN_UP: dy=-1; MODAL->dyclock+=AUTOREPEAT_TIME; break;
        case EGG_BTN_DOWN: dy=1; MODAL->dyclock+=AUTOREPEAT_TIME; break;
        default: MODAL->dy=0;
      }
    }
  }
  if (dy) {
    MODAL->dy=dy;
    int np=MODAL->scroll+dy*BYTES_PER_ROW;
    if (np<0) np=0; // Clamp at zero, but allow it to scroll past the end, whatever.
    if (np!=MODAL->scroll) {
      MODAL->scroll=np;
      modal_hexdump_rewrite(modal);
    }
  }
  
  gui_term_update(MODAL->term,elapsed);
}

/* Render.
 */
 
static void _hexdump_render(struct modal *modal) {
  gui_term_render(MODAL->term);
}

/* Initialize.
 */
 
static int _hexdump_init(struct modal *modal,const char *desc,int descc,const uint8_t *v,int c) {
  modal->del=_hexdump_del;
  modal->update=_hexdump_update;
  modal->render=_hexdump_render;
  
  MODAL->v=v;
  MODAL->c=c;
  
  if (!(MODAL->term=gui_term_new(0,0,FBW,FBH))) return -1;
  gui_term_set_background(MODAL->term,0);
  gui_term_get_size(&MODAL->colc,&MODAL->rowc,MODAL->term);
  
  // Center the description in row zero.
  if (desc) {
    if (descc<0) { descc=0; while (desc[descc]) descc++; }
    if (descc>MODAL->colc) descc=MODAL->colc;
    gui_term_write(MODAL->term,(MODAL->colc>>1)-(descc>>1),0,desc,descc);
  }
  
  modal_hexdump_rewrite(modal);
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_hexdump(const char *desc,int descc,const void *v,int c) {
  struct modal *modal=modal_new(sizeof(struct modal_hexdump));
  if (!modal) return 0;
  if (
    (_hexdump_init(modal,desc,descc,v,c)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
