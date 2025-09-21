#include "../demo.h"
#include "../gui/gui.h"

#define OPTIONID_STORE1 1
#define OPTIONID_STORE2 2
#define OPTIONID_STORE3 3
#define OPTIONID_SPACE 4
#define OPTIONID_RES 5 /* +index */

#define K1 "firstStoredThing"
#define K2 "anotherThing"
#define K3 "thingNumberC"

struct modal_storage {
  struct modal hdr;
  struct gui_list *list;
};

#define MODAL ((struct modal_storage*)modal)

/* Delete.
 */
 
static void _storage_del(struct modal *modal) {
  gui_list_del(MODAL->list);
}

/* Update.
 */
 
static void _storage_update(struct modal *modal,double elapsed,int input,int pvinput) {
  gui_list_update(MODAL->list,elapsed,input,pvinput);
}

/* Render.
 */
 
static void _storage_render(struct modal *modal) {
  gui_list_render(MODAL->list);
}

/* Update label for a store field, reading it from Egg Platform API.
 */
 
static void modal_storage_refresh_label(struct modal *modal,int optionid,const char *k) {
  int kc=0;
  while (k[kc]) kc++;
  char msg[256];
  if (kc>=sizeof(msg)-2) return;
  memcpy(msg,k,kc);
  int msgc=kc;
  msg[msgc++]=':';
  msg[msgc++]=' ';
  int vc=egg_store_get(msg+msgc,sizeof(msg)-msgc,k,kc);
  if (vc<0) vc=0;
  else if (vc>1024) vc=1024; // sanity limit, don't let it overflow
  msgc+=vc;
  if (msgc>sizeof(msg)) return;
  gui_list_replace(MODAL->list,optionid,msg,msgc,1);
}

/* Grab a text timestamp or whatever, store it in Egg's persistence, then update the list.
 */
 
static void modal_storage_poke_label(struct modal *modal,int optionid,const char *k) {
  int kc=0;
  while (k[kc]) kc++;
  int tv[7]={0};
  egg_time_local(tv,7);
  char v[32];
  int vc=snprintf(v,sizeof(v),"%04d-%02d-%02dT%02d:%02d:%02d.%03d",tv[0],tv[1],tv[2],tv[3],tv[4],tv[5],tv[6]);
  if ((vc<1)||(vc>=sizeof(v))) vc=0;
  if (egg_store_set(k,kc,v,vc)<0) {
    fprintf(stderr,"egg_store_set failed! '%.*s' = '%.*s'\n",kc,k,vc,v);
  }
  modal_storage_refresh_label(modal,optionid,k);
}

/* Generate list text for a resource.
 */
 
static int storage_res_repr(char *dst,int dsta,int tid,int rid,const void *v,int c) {
  const char *tname=0;
  switch (tid) {
    #define _(tag) case EGG_TID_##tag: tname=#tag; break;
    EGG_TID_FOR_EACH
    #undef _
  }
  if ((tid==EGG_TID_strings)&&(rid&~0x3f)) {
    int lang=rid>>6;
    rid&=0x3f;
    char lname[2];
    EGG_STRING_FROM_LANG(lname,lang)
    EGG_LANG_STRING_SANITIZE(lname)
    if (tname&&tname[0]) {
      return snprintf(dst,dsta,"%s:%.2s-%d, %d",tname,lname,rid,c);
    } else {
      return snprintf(dst,dsta,"%d:%.2s-%d, %d",tid,lname,rid,c);
    }
  }
  if (tname&&tname[0]) {
    return snprintf(dst,dsta,"%s:%d, %d",tname,rid,c);
  } else {
    return snprintf(dst,dsta,"%d:%d, %d",tid,rid,c);
  }
}

/* Activate.
 */
 
static void modal_storage_cb_activate(struct gui_list *list,int optionid) {
  struct modal *modal=gui_list_get_userdata(list);
  switch (optionid) {
    case OPTIONID_STORE1: modal_storage_poke_label(modal,optionid,K1); break;
    case OPTIONID_STORE2: modal_storage_poke_label(modal,optionid,K2); break;
    case OPTIONID_STORE3: modal_storage_poke_label(modal,optionid,K3); break;
    default: if (optionid>=OPTIONID_RES) {
        int p=optionid-OPTIONID_RES;
        if ((p>=0)&&(p<g.resc)) {
          const struct rom_entry *res=g.resv+p;
          char desc[256];
          int descc=storage_res_repr(desc,sizeof(desc),res->tid,res->rid,res->v,res->c);
          if ((descc<1)||(descc>=sizeof(desc))) descc=0;
          modal_new_hexdump(desc,descc,res->v,res->c);
        }
      }
  }
}

/* Initialize.
 */
 
static int _storage_init(struct modal *modal) {
  modal->del=_storage_del;
  modal->update=_storage_update;
  modal->render=_storage_render;
  
  if (!(MODAL->list=gui_list_new(0,0,FBW,FBH))) return -1;
  gui_list_set_userdata(MODAL->list,modal);
  gui_list_cb_activate(MODAL->list,0,modal_storage_cb_activate);
  gui_list_insert(MODAL->list,-1,OPTIONID_STORE1,"a",-1,1);
  gui_list_insert(MODAL->list,-1,OPTIONID_STORE2,"b",-1,1);
  gui_list_insert(MODAL->list,-1,OPTIONID_STORE3,"c",-1,1);
  modal_storage_refresh_label(modal,OPTIONID_STORE1,K1);
  modal_storage_refresh_label(modal,OPTIONID_STORE2,K2);
  modal_storage_refresh_label(modal,OPTIONID_STORE3,K3);
  gui_list_insert(MODAL->list,-1,OPTIONID_SPACE,"Resources:",-1,0);
  
  const struct rom_entry *res=g.resv;
  int i=0;
  for (;i<g.resc;i++,res++) {
    char desc[64];
    int descc=storage_res_repr(desc,sizeof(desc),res->tid,res->rid,res->v,res->c);
    if ((descc<1)||(descc>=sizeof(desc))) descc=0;
    gui_list_insert(MODAL->list,-1,OPTIONID_RES+i,desc,descc,1);
  }
  
  return 0;
}

/* New.
 */
 
struct modal *modal_new_storage() {
  struct modal *modal=modal_new(sizeof(struct modal_storage));
  if (!modal) return 0;
  if (
    (_storage_init(modal)<0)||
    (modal_push(modal)<0)
  ) {
    modal_del(modal);
    return 0;
  }
  return modal;
}
