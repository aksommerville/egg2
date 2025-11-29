#include "umenu_internal.h"

/* Delete.
 */
 
void umenu_del(struct umenu *umenu) {
  if (!umenu) return;
  incfg_quit(umenu);
  render_texture_del(eggrt.render,umenu->texid_tiles);
  if (umenu->langv) free(umenu->langv);
  eggrt.input_mode=umenu->input_mode0;
  free(umenu);
}

/* Select tilesize for the give framebuffer and minimum grid dimensions.
 * One of (4,8,16,32) on success, or zero on error.
 */
 
static int umenu_select_tilesize(int fbw,int fbh,int colc,int rowc) {
  int colwmax=fbw/colc;
  int rowhmax=fbh/rowc;
  int tilesize=(colwmax<rowhmax)?colwmax:rowhmax;
  if (tilesize>=32) return 32;
  if (tilesize>=16) return 16;
  if (tilesize>=8) return 8;
  if (tilesize>=4) return 4;
  return 0;
}

/* Add widget.
 */
 
static struct umenu_widget *umenu_widget_new(struct umenu *umenu,int actionid) {
  if (umenu->widgetc>=UMENU_WIDGET_LIMIT) return 0;
  struct umenu_widget *widget=umenu->widgetv+umenu->widgetc++;
  widget->actionid=actionid;
  widget->x=0;
  widget->y=0;
  widget->w=umenu->tilesize;
  widget->nl=widget->nr=widget->nt=widget->nb=0;
  widget->tileid=25+actionid;
  return widget;
}

/* With all widgets sized and positioned against (0,0), select proper framebuffer positions.
 */
 
static void umenu_finish_layout(struct umenu *umenu) {
  int usew=umenu->tilesize,useh=umenu->tilesize;
  struct umenu_widget *widget=umenu->widgetv;
  int i=umenu->widgetc;
  for (;i-->0;widget++) {
    if (widget->x+widget->w>usew) usew=widget->x+widget->w;
    if (widget->y+umenu->tilesize>useh) useh=widget->y+umenu->tilesize;
  }
  int x0=(umenu->fbw>>1)-(usew>>1);
  int y0=(umenu->fbh>>1)-(useh>>1);
  for (widget=umenu->widgetv,i=umenu->widgetc;i-->0;widget++) {
    widget->x+=x0;
    widget->y+=y0;
  }
}

/* Load textures.
 */
 
static int umenu_load_texture(const void *src,int srcc,int black_is_transparent) {
  // Luckily, render texid are the same thing as Egg texid.
  // That's important because we need to load a PNG file that isn't in the ROM.
  int w=0,h=0;
  if (image_measure(&w,&h,src,srcc)<0) return -1;
  if ((w<1)||(h<1)||(w>EGG_TEXTURE_SIZE_LIMIT)||(h>EGG_TEXTURE_SIZE_LIMIT)) return -1;
  int stride=w<<2;
  int pixelslen=stride*h;
  void *pixels=malloc(pixelslen);
  if (!pixels) return -1;
  int texid=render_texture_new(eggrt.render);
  int err=image_decode(pixels,pixelslen,src,srcc);
  if (err>=0) {
    if (black_is_transparent) {
      unsigned int *p=pixels;
      int i=w*h;
      for (;i-->0;p++) if (*p==0xff000000) *p=0;
    }
    err=render_texture_load_raw(eggrt.render,texid,w,h,stride,pixels,pixelslen);
  }
  free(pixels);
  if (err<0) {
    render_texture_del(eggrt.render,texid);
    return -1;
  }
  return texid;
}
 
static int umenu_load_textures(struct umenu *umenu) {
  if ((umenu->texid_tiles=umenu_load_texture(menubits,menubits_length,1))<1) return -1;
  return 0;
}

/* Collect the ROM's language list into (umenu->langv).
 */
 
static int umenu_list_languages(struct umenu *umenu) {
  if (umenu->incfg_only) return 0; // No need to load these if we're not showing the Universal Menu.
  const char *src=eggrt.metadata.lang; // comma-delimited
  int srcc=eggrt.metadata.langc;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]==',') { srcp++; continue; }
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&(src[srcp++]!=',')) tokenc++;
    while (tokenc&&((unsigned char)token[tokenc-1]<=0x20)) tokenc--;
    if ((tokenc==2)&&EGG_STRING_IS_LANG(token)) {
      int lang=EGG_LANG_FROM_STRING(token);
      if (umenu->langc>=umenu->langa) {
        int na=umenu->langa+4;
        if (na>INT_MAX/sizeof(int)) return -1;
        void *nv=realloc(umenu->langv,sizeof(int)*na);
        if (!nv) return -1;
        umenu->langv=nv;
        umenu->langa=na;
      }
      umenu->langv[umenu->langc++]=lang;
    }
  }
  return 0;
}

/* New.
 */

struct umenu *umenu_new(int incfg_only) {
  struct umenu *umenu=calloc(1,sizeof(struct umenu));
  if (!umenu) return 0;
  
  // We must be in GAMEPAD mode. Remember to turn it back when done.
  umenu->input_mode0=eggrt.input_mode;
  eggrt.input_mode=EGG_INPUT_MODE_GAMEPAD;
  
  if (umenu->incfg_only=incfg_only) {
    umenu->incfg=1;
    incfg_begin(umenu);
  }
  umenu->fbw=eggrt.metadata.fbw;
  umenu->fbh=eggrt.metadata.fbh;
  
  if (umenu_list_languages(umenu)<0) {
    umenu_del(umenu);
    return 0;
  }
  
  if (umenu_load_textures(umenu)<0) {
    umenu_del(umenu);
    return 0;
  }
  
  /* Determine tile size. One of 4,8,16,32.
   * First compute based on 11x7 cells, but if that's too many, try the bare minimum 5x4.
   */
  int colc,rowc;
  if ((umenu->tilesize=umenu_select_tilesize(umenu->fbw,umenu->fbh,colc=11,rowc=7))>0) {
  } else if ((umenu->tilesize=umenu_select_tilesize(umenu->fbw,umenu->fbh,colc=5,rowc=4))>0) {
  } else {
    fprintf(stderr,"%s: %dx%d framebuffer is just too small for us.\n",__func__,umenu->fbw,umenu->fbh);
    umenu_del(umenu);
    return 0;
  }
  
  struct umenu_widget *widget;
  int fullh=rowc*umenu->tilesize;
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_MUSIC)) {
    widget->w=colc*umenu->tilesize;
    widget->nt=UMENU_ACTIONID_RESUME;
    widget->nb=UMENU_ACTIONID_SOUND;
  }
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_SOUND)) {
    widget->w=colc*umenu->tilesize;
    widget->y=(1*fullh)/4;
    widget->nt=UMENU_ACTIONID_MUSIC;
    widget->nb=UMENU_ACTIONID_LANG;
  }
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_LANG)) {
    widget->w=colc*umenu->tilesize;
    widget->y=(2*fullh)/4;
    widget->nt=UMENU_ACTIONID_SOUND;
    widget->nb=UMENU_ACTIONID_RESUME;
  }
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_INPUT)) {
    widget->w=umenu->tilesize;
    widget->y=(3*fullh)/4;
    widget->nt=UMENU_ACTIONID_LANG;
    widget->nb=UMENU_ACTIONID_MUSIC;
    widget->nl=UMENU_ACTIONID_QUIT;
    widget->nr=UMENU_ACTIONID_RESUME;
  }
  umenu->widgetp=umenu->widgetc;
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_RESUME)) {
    widget->w=umenu->tilesize;
    widget->x=(colc>>1)*umenu->tilesize;
    widget->y=(3*fullh)/4;
    widget->nt=UMENU_ACTIONID_LANG;
    widget->nb=UMENU_ACTIONID_MUSIC;
    widget->nl=UMENU_ACTIONID_INPUT;
    widget->nr=UMENU_ACTIONID_QUIT;
  }
  if (widget=umenu_widget_new(umenu,UMENU_ACTIONID_QUIT)) {
    widget->w=umenu->tilesize;
    widget->x=(colc-1)*umenu->tilesize;
    widget->y=(3*fullh)/4;
    widget->nt=UMENU_ACTIONID_LANG;
    widget->nb=UMENU_ACTIONID_MUSIC;
    widget->nl=UMENU_ACTIONID_RESUME;
    widget->nr=UMENU_ACTIONID_INPUT;
  }
  
  umenu_finish_layout(umenu);
  
  return umenu;
}

/* Widget index by actionid.
 */
 
static int umenu_find_widget(const struct umenu *umenu,int actionid) {
  const struct umenu_widget *widget=umenu->widgetv;
  int i=0;
  for (;i<umenu->widgetc;i++,widget++) {
    if (widget->actionid==actionid) return i;
  }
  return -1;
}

/* Activate selection.
 */
 
static void umenu_activate(struct umenu *umenu) {
  if ((umenu->widgetp<0)||(umenu->widgetp>=umenu->widgetc)) return;
  switch (umenu->widgetv[umenu->widgetp].actionid) {
    case UMENU_ACTIONID_INPUT: incfg_begin(umenu); break;
    case UMENU_ACTIONID_RESUME: {
        umenu->defunct=1;
        inmgr_artificial_event(0,EGG_BTN_SOUTH,0);
        inmgr_artificial_event(1,EGG_BTN_SOUTH,0);
      } break;
    case UMENU_ACTIONID_QUIT: egg_terminate(0); break;
  }
}

/* Change language.
 */
 
static void umenu_change_lang(struct umenu *umenu,int d) {
  if (umenu->langc<1) return;
  int lang=egg_prefs_get(EGG_PREF_LANG);
  int p=-1;
  int i=0;
  for (;i<umenu->langc;i++) {
    if (umenu->langv[i]==lang) {
      p=i;
      break;
    }
  }
  p+=d;
  if (p<0) p=umenu->langc-1;
  else if (p>=umenu->langc) p=0;
  egg_prefs_set(EGG_PREF_LANG,umenu->langv[p]);
}

/* Move cursor.
 */
 
static void umenu_move(struct umenu *umenu,int dx,int dy) {
  if ((umenu->widgetp<0)||(umenu->widgetp>=umenu->widgetc)) return;
  struct umenu_widget *widget=umenu->widgetv+umenu->widgetp;
  int nextaction=0;
  if (dx<0) nextaction=widget->nl;
  else if (dx>0) nextaction=widget->nr;
  else if (dy<0) nextaction=widget->nt;
  else if (dy>0) nextaction=widget->nb;
  if (!nextaction) {
    if (dx) switch (widget->actionid) {
      //TODO Can we also hold left/right and move more continuously?
      case UMENU_ACTIONID_MUSIC: egg_prefs_set(EGG_PREF_MUSIC,egg_prefs_get(EGG_PREF_MUSIC)+10*dx); break;
      case UMENU_ACTIONID_SOUND: egg_prefs_set(EGG_PREF_SOUND,egg_prefs_get(EGG_PREF_SOUND)+10*dx); break;
      case UMENU_ACTIONID_LANG: umenu_change_lang(umenu,dx); break;
    }
    return;
  }
  int p=umenu_find_widget(umenu,nextaction);
  if (p>=0) umenu->widgetp=p;
}

/* Update.
 */

int umenu_update(struct umenu *umenu,double elapsed) {
  if (!umenu) return -1;
  
  if (umenu->incfg) {
    incfg_update(umenu,elapsed);
    
  } else {
    int input=egg_input_get_one(0);
    if (input!=umenu->pvinput) {
      if ((input&EGG_BTN_LEFT)&&!(umenu->pvinput&EGG_BTN_LEFT)) umenu_move(umenu,-1,0);
      if ((input&EGG_BTN_RIGHT)&&!(umenu->pvinput&EGG_BTN_RIGHT)) umenu_move(umenu,1,0);
      if ((input&EGG_BTN_UP)&&!(umenu->pvinput&EGG_BTN_UP)) umenu_move(umenu,0,-1);
      if ((input&EGG_BTN_DOWN)&&!(umenu->pvinput&EGG_BTN_DOWN)) umenu_move(umenu,0,1);
      if ((input&EGG_BTN_SOUTH)&&!(umenu->pvinput&EGG_BTN_SOUTH)) umenu_activate(umenu);
      umenu->pvinput=input;
    }
  }

  if (umenu->defunct) {
    umenu_del(umenu);
    if (umenu==eggrt.umenu) eggrt.umenu=0;
  }
  return 0;
}

/* Render.
 * The client does render too, before us.
 */
 
void umenu_fill_rect(int x,int y,int w,int h,uint32_t rgba) {
  uint8_t r=rgba>>24,g=rgba>>16,b=rgba>>8,a=rgba;
  struct egg_render_raw vtxv[]={
    {x  ,y  ,0,0,r,g,b,a},
    {x  ,y+h,0,0,r,g,b,a},
    {x+w,y  ,0,0,r,g,b,a},
    {x+w,y+h,0,0,r,g,b,a},
  };
  struct egg_render_uniform un={
    .mode=EGG_RENDER_TRIANGLE_STRIP,
    .dsttexid=1,
    .srctexid=0,
    .alpha=0xff,
  };
  egg_render(&un,vtxv,sizeof(vtxv));
}

// (dstx,dsty) is the top left. We don't actually use TILE render mode.
void umenu_blit_tile(struct umenu *umenu,int dstx,int dsty,uint8_t tileid) {
  int srcx,srcy;
  switch (umenu->tilesize) {
    case 32: srcx=0; srcy=0; break;
    case 16: srcx=0; srcy=128; break;
    case 8: srcx=128; srcy=128; break;
    case 4: srcx=192; srcy=128; break;
    default: return;
  }
  srcx+=(tileid&7)*umenu->tilesize;
  srcy+=(tileid>>3)*umenu->tilesize;
  struct egg_render_raw vtxv[]={
    {dstx,dsty,srcx,srcy},
    {dstx,dsty+umenu->tilesize,srcx,srcy+umenu->tilesize},
    {dstx+umenu->tilesize,dsty,srcx+umenu->tilesize,srcy},
    {dstx+umenu->tilesize,dsty+umenu->tilesize,srcx+umenu->tilesize,srcy+umenu->tilesize},
  };
  struct egg_render_uniform un={
    .mode=EGG_RENDER_TRIANGLE_STRIP,
    .dsttexid=1,
    .srctexid=umenu->texid_tiles,
    .alpha=0xff,
  };
  egg_render(&un,vtxv,sizeof(vtxv));
}

static void umenu_render_bar(struct umenu *umenu,struct umenu_widget *widget,int v,int limit) {
  if (v<0) v=0; else if (v>limit) v=limit;
  int x=widget->x+umenu->tilesize;
  int y=widget->y;
  int w=widget->w-umenu->tilesize-1;
  int h=umenu->tilesize-1;
  int onw=(v*w)/limit;
  if (v<=0) onw=0;
  else if (!onw) onw=1;
  else if (v>=limit) onw=w;
  else if (onw>=w) onw=w-1;
  umenu_fill_rect(x,y,onw,h,0xffffffff);
  umenu_fill_rect(x+onw,y,w-onw,h,0x404040ff);
}

static void umenu_render_lang(struct umenu *umenu,struct umenu_widget *widget,int lang) {
  char s[2];
  EGG_STRING_FROM_LANG(s,lang)
  EGG_LANG_STRING_SANITIZE(s)
  umenu_blit_tile(umenu,widget->x+umenu->tilesize*1,widget->y,s[0]-'a');
  umenu_blit_tile(umenu,widget->x+umenu->tilesize*2,widget->y,s[1]-'a');
}
 
int umenu_render(struct umenu *umenu) {
  if (!umenu) return -1;
  umenu_fill_rect(0,0,eggrt.metadata.fbw,eggrt.metadata.fbh,0x000000e0);
  
  if (umenu->incfg) {
    incfg_render(umenu);
    return 0;
  }
  
  struct umenu_widget *widget=umenu->widgetv;
  int i=0;
  for (;i<umenu->widgetc;i++,widget++) {
    if (i==umenu->widgetp) {
      // Tiles all have one black column and row, on the right and bottom.
      umenu_fill_rect(widget->x-1,widget->y-1,widget->w+1,umenu->tilesize+1,0x0080a0ff);
    }
    umenu_blit_tile(umenu,widget->x,widget->y,widget->tileid);
    switch (widget->actionid) {
      case UMENU_ACTIONID_MUSIC: umenu_render_bar(umenu,widget,egg_prefs_get(EGG_PREF_MUSIC),99); break;
      case UMENU_ACTIONID_SOUND: umenu_render_bar(umenu,widget,egg_prefs_get(EGG_PREF_SOUND),99); break;
      case UMENU_ACTIONID_LANG: umenu_render_lang(umenu,widget,egg_prefs_get(EGG_PREF_LANG)); break;
    }
  }
  return 0;
}
