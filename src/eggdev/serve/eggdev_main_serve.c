#include "eggdev_serve_internal.h"
#include "eggdev/build/builder.h"
#include "opt/zip/zip.h"
#include <sys/signal.h>
#include <unistd.h>

static int eggdev_cb_get_other(struct http_xfer *req,struct http_xfer *rsp);

/* Project name, for GET /api/webpath and GET /api/projname
 */
 
static int eggdev_get_proper_project_name(char *dst,int dsta) {

  // If (g.project) unset, we have no answer.
  if (!g.project) {
    if (dsta>0) dst[0]=0;
    return 0;
  }
  
  // If (g.project) is exactly ".", use the basename of the working directory.
  // This is the usual case; our default project launches with `--project=.` for `make edit`.
  if ((g.project[0]=='.')&&!g.project[1]) {
    char wd[1024];
    if (getcwd(wd,sizeof(wd))==wd) {
      const char *base=wd;
      int basec=0,srcp=0;
      for (;wd[srcp];srcp++) {
        if (wd[srcp]=='/') {
          base=wd+srcp+1;
          basec=0;
        } else {
          basec++;
        }
      }
      if (basec<=dsta) {
        memcpy(dst,base,basec);
        if (basec<dsta) dst[basec]=0;
      }
      return basec;
    }
  }
  
  // Extract basename from (g.project).
  const char *base=g.project;
  int basec=0,srcp=0;
  for (;g.project[srcp];srcp++) {
    if (g.project[srcp]=='/') {
      base=g.project+srcp+1;
      basec=0;
    } else {
      basec++;
    }
  }
  if (basec<=dsta) {
    memcpy(dst,base,basec);
    if (basec<dsta) dst[basec]=0;
  }
  return basec;
}

/* GET /api/webpath
 */

static int eggdev_cb_get_webpath(struct http_xfer *req,struct http_xfer *rsp) {
  char pname[64];
  int pnamec=eggdev_get_proper_project_name(pname,sizeof(pname));
  if ((pnamec>0)&&(pnamec<=sizeof(pname))) {
    /* Realistically, the target should always be "web".
     * But we'll do it right and check each configured target until we find one with "web" packaging,
     * then infer the output path from that.
     */
    const char *targets=0;
    int targetsc=eggdev_config_get(&targets,"EGG_TARGETS",11);
    int targetsp=0;
    while (targetsp<targetsc) {
      if ((unsigned char)targets[targetsp]<=0x20) {
        targetsp++;
        continue;
      }
      const char *token=targets+targetsp++;
      int tokenc=1;
      while ((targetsp<targetsc)&&((unsigned char)targets[targetsp++]>0x20)) tokenc++;
      const char *packaging=0;
      int packagingc=eggdev_config_get_sub(&packaging,token,tokenc,"PACKAGING",9);
      if ((packagingc==3)&&!memcmp(packaging,"web",3)) {
        struct sr_encoder *dst=http_xfer_get_body(rsp);
        if (sr_encode_fmt(dst,"/out/%.*s-%.*s.html\n",pnamec,pname,tokenc,token)<0) return -1;
        return http_xfer_set_status(rsp,200,"OK");
      }
    }
  }
  return http_xfer_set_status(rsp,404,"No suitable target");
}

/* GET /api/projname
 */
 
static int eggdev_cb_get_projname(struct http_xfer *req,struct http_xfer *rsp) {
  char pname[64];
  int pnamec=eggdev_get_proper_project_name(pname,sizeof(pname));
  if ((pnamec<1)||(pnamec>sizeof(pname))) return http_xfer_set_status(rsp,404,"No project established at command line.");
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  if (sr_encode_raw(dst,pname,pnamec)<0) return -1;
  return http_xfer_set_status(rsp,200,"OK");
}

/* GET /api/buildfirst/**
 */
 
static int eggdev_cb_get_buildfirst(struct http_xfer *req,struct http_xfer *rsp) {
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if ((reqpathc>=15)&&!memcmp(reqpath,"/api/buildfirst",15)) {
    reqpath+=15;
    reqpathc-=15;
  }
  if (g.project) {
    struct builder builder={
      .log=http_xfer_get_body(rsp),
    };
    int logc0=builder.log->c;
    int err;
    if ((err=builder_set_root(&builder,g.project,-1))<0) {
      builder_cleanup(&builder);
      return http_xfer_set_status(rsp,500,"Internal error");
    }
    if ((err=builder_main(&builder))<0) {
      builder_cleanup(&builder);
      http_xfer_set_header(rsp,"Content-Type",12,"text/plain",10);
      return http_xfer_set_status(rsp,500,"Build failed");
    }
    builder.log->c=logc0;
    builder_cleanup(&builder);
  }
  char rerequest[256];
  int rerequestc=snprintf(rerequest,sizeof(rerequest),"GET %.*s HTTP/1.1",reqpathc,reqpath);
  if ((rerequestc<1)||(rerequestc>=sizeof(rerequest))) return http_xfer_set_status(rsp,404,"Not found");
  http_xfer_set_topline(req,rerequest,rerequestc);
  return eggdev_cb_get_other(req,rsp);
}

/* GET /api/symbols
 */
 
static int eggdev_cb_get_symbols(struct http_xfer *req,struct http_xfer *rsp) {
  eggdev_client_require();
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  if (sr_encode_json_array_start(dst,0,0)<0) return -1;
  const struct eggdev_ns *ns=g.client.nsv;
  int nsi=g.client.nsc;
  for (;nsi-->0;ns++) {
    const char *nstype=0;
    switch (ns->nstype) {
      case EGGDEV_NSTYPE_NS: nstype="NS"; break;
      case EGGDEV_NSTYPE_CMD: nstype="CMD"; break;
    }
    if (!nstype) continue; // Other things may be recorded in (g.client), we can ignore them.
    const struct eggdev_sym *sym=ns->symv;
    int symi=ns->symc;
    for (;symi-->0;sym++) {
      int jsonctx=sr_encode_json_object_start(dst,0,0);
      sr_encode_json_string(dst,"nstype",6,nstype,-1);
      sr_encode_json_string(dst,"ns",2,ns->name,ns->namec);
      sr_encode_json_string(dst,"k",1,sym->k,sym->kc);
      sr_encode_json_int(dst,"v",1,sym->v);
      if (sr_encode_json_end(dst,jsonctx)<0) return -1;
    }
  }
  if (sr_encode_json_end(dst,0)<0) return -1;
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",16);
  return http_xfer_set_status(rsp,200,"OK");
}

/* GET /api/instruments
 */
 
static int eggdev_cb_get_instruments(struct http_xfer *req,struct http_xfer *rsp) {
  void *src=0;
  int srcc=eggdev_config_get_instruments_text(&src);
  if (srcc<0) srcc=0;
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  int dstc0=dst->c;
  struct sr_encoder errmsg={0};
  struct eggdev_convert_context ctx={
    .dst=dst,
    .src=src,
    .srcc=srcc,
    .errmsg=&errmsg,
  };
  int err=eggdev_eau_from_eaut(&ctx);
  if (src) free(src);
  if (err>=0) {
    sr_encoder_cleanup(&errmsg);
    http_xfer_set_header(rsp,"Content-Type",12,"application/octet-stream",24);
    return http_xfer_set_status(rsp,200,"OK");
  } else {
    dst->c=dstc0;
    sr_encode_raw(dst,errmsg.v,errmsg.c);
    sr_encoder_cleanup(&errmsg);
    http_xfer_set_header(rsp,"Content-Type",12,"text/plain",10);
    return http_xfer_set_status(rsp,500,"Failed to convert");
  }
}

/* GET /api/toc/**
 */
 
struct eggdev_get_toc {
  struct sr_encoder *dst;
  struct eggdev_http_paths paths;
};

static int eggdev_get_toc_cb(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_get_toc *ctx=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return dir_read(path,eggdev_get_toc_cb,ctx);
  if (ftype!='f') return 0;
  
  const char *rptpath=path;
  int rptpathc=0;
  while (rptpath[rptpathc]) rptpathc++;
  if ((rptpathc<ctx->paths.localpfxc)||memcmp(rptpath,ctx->paths.localpfx,ctx->paths.localpfxc)) return 0;
  rptpath+=ctx->paths.localpfxc;
  rptpathc-=ctx->paths.localpfxc;
  int combinedc=ctx->paths.reqpfxc+rptpathc;
  char combined[1024];
  if (combinedc>sizeof(combined)) return 0;
  memcpy(combined,ctx->paths.reqpfx,ctx->paths.reqpfxc);
  memcpy(combined+ctx->paths.reqpfxc,rptpath,rptpathc);
  if (sr_encode_json_string(ctx->dst,0,0,combined,combinedc)<0) return -1;

  return 0;
}
 
static int eggdev_cb_get_toc(struct http_xfer *req,struct http_xfer *rsp) {
  struct eggdev_get_toc ctx={
    .dst=http_xfer_get_body(rsp),
    .paths={0},
  };
  int dstc0=ctx.dst->c;
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if ((reqpathc<8)||memcmp(reqpath,"/api/toc",8)) return -1;
  reqpath+=8;
  reqpathc-=8;
  if (eggdev_http_paths_resolve(&ctx.paths,reqpath,reqpathc,0)<0) {
    eggdev_http_paths_cleanup(&ctx.paths);
    return http_xfer_set_status(rsp,404,"Not found 1");
  }
  int jsonctx=sr_encode_json_array_start(ctx.dst,0,0);
  if (dir_read(ctx.paths.local,eggdev_get_toc_cb,&ctx)<0) {
    ctx.dst->c=dstc0;
    eggdev_http_paths_cleanup(&ctx.paths);
    return http_xfer_set_status(rsp,404,"Not found 2");
  }
  sr_encode_json_end(ctx.dst,jsonctx);
  sr_encode_u8(ctx.dst,0x0a);
  eggdev_http_paths_cleanup(&ctx.paths);
  http_xfer_set_header(rsp,"Content-Type",12,"application/json",16);
  return http_xfer_set_status(rsp,200,"OK");
}

/* GET /api/allcontent/**
 */
 
struct eggdev_allcontent {
  struct sr_encoder *dst;
  struct eggdev_http_paths paths;
};

static int eggdev_allcontent_cb(const char *path,const char *base,char ftype,void *userdata) {
  struct eggdev_allcontent *ctx=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype=='d') return dir_read(path,eggdev_allcontent_cb,ctx);
  if (ftype=='f') {
    const char *rptpath=path;
    int rptpathc=0;
    while (rptpath[rptpathc]) rptpathc++;
    if ((rptpathc<ctx->paths.localpfxc)||memcmp(rptpath,ctx->paths.localpfx,ctx->paths.localpfxc)) return 0;
    rptpath+=ctx->paths.localpfxc;
    rptpathc-=ctx->paths.localpfxc;
    if ((rptpathc<1)||(ctx->paths.reqpfxc+rptpathc>0xff)) return 0;
    void *src=0;
    int srcc=file_read(&src,path);
    if (srcc<0) return 0; // Skip, don't fail.
    int err=sr_encode_u8(ctx->dst,ctx->paths.reqpfxc+rptpathc);
    if (err>=0) err=sr_encode_raw(ctx->dst,ctx->paths.reqpfx,ctx->paths.reqpfxc);
    if (err>=0) err=sr_encode_raw(ctx->dst,rptpath,rptpathc);
    if (err>=0) err=sr_encode_intbelen(ctx->dst,src,srcc,3);
    free(src);
    if (err<0) return err;
    return 0;
  }
  return 0;
}
 
static int eggdev_cb_get_allcontent(struct http_xfer *req,struct http_xfer *rsp) {
  struct eggdev_allcontent ctx={
    .dst=http_xfer_get_body(rsp),
  };
  int dstc0=ctx.dst->c;
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if ((reqpathc<15)||memcmp(reqpath,"/api/allcontent",15)) return http_xfer_set_status(rsp,500,"Internal error");
  reqpath+=15;
  reqpathc-=15;
  if (eggdev_http_paths_resolve(&ctx.paths,reqpath,reqpathc,0)<0) {
    eggdev_http_paths_cleanup(&ctx.paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  if (dir_read(ctx.paths.local,eggdev_allcontent_cb,&ctx)<0) {
    ctx.dst->c=dstc0;
    eggdev_http_paths_cleanup(&ctx.paths);
    return http_xfer_set_status(rsp,500,"Internal error");
  }
  eggdev_http_paths_cleanup(&ctx.paths);
  return http_xfer_set_status(rsp,200,"OK");
}

/* POST /api/convert
 */
 
static int eggdev_cb_post_convert(struct http_xfer *req,struct http_xfer *rsp) {
  struct sr_encoder *src=http_xfer_get_body(req);
  struct sr_encoder *dst=http_xfer_get_body(rsp);
  int dstc0=dst->c;
  char srcfmtstr[32],dstfmtstr[32],ns[32];
  int srcfmtc=http_xfer_get_param(srcfmtstr,sizeof(srcfmtstr),req,"srcfmt",6);
  int dstfmtc=http_xfer_get_param(dstfmtstr,sizeof(dstfmtstr),req,"dstfmt",6);
  int nsc=http_xfer_get_param(ns,sizeof(ns),req,"ns",2);
  if ((srcfmtc<0)||(srcfmtc>sizeof(srcfmtstr))) srcfmtc=0;
  if ((dstfmtc<0)||(dstfmtc>sizeof(dstfmtstr))) dstfmtc=0;
  if ((nsc<0)||(nsc>sizeof(ns))) nsc=0;

  int srcfmt=eggdev_fmt_eval(srcfmtstr,srcfmtc);
  int dstfmt=eggdev_fmt_eval(dstfmtstr,dstfmtc);
  if (srcfmt<1) srcfmt=eggdev_fmt_by_signature(src->v,src->c);
  eggdev_convert_fn convert=eggdev_get_converter(dstfmt,srcfmt);
  if (!convert) return http_xfer_set_status(rsp,500,"No converter for formats %d=>%d",srcfmt,dstfmt);
  
  // Put the resolved formats in the response, whether pass or fail.
  srcfmtc=eggdev_fmt_repr(srcfmtstr,sizeof(srcfmtstr),srcfmt);
  dstfmtc=eggdev_fmt_repr(dstfmtstr,sizeof(dstfmtstr),dstfmt);
  if ((srcfmtc>0)&&(srcfmtc<=sizeof(srcfmtstr))) http_xfer_set_header(rsp,"X-srcfmt",8,srcfmtstr,srcfmtc);
  if ((dstfmtc>0)&&(dstfmtc<=sizeof(dstfmtstr))) http_xfer_set_header(rsp,"X-dstfmt",8,dstfmtstr,dstfmtc);

  struct sr_encoder errmsg={0};
  struct eggdev_convert_context ctx={
    .dst=dst,
    .src=src->v,
    .srcc=src->c,
    .ns=ns,
    .nsc=nsc,
    .errmsg=&errmsg,
  };
  int err=convert(&ctx);
  if (err>=0) {
    sr_encoder_cleanup(&errmsg);
    return http_xfer_set_status(rsp,200,"OK");
  }

  dst->c=dstc0;
  int i=errmsg.c; while (i-->0) { // errmsg will always contain a linefeed, and could be more than one.
    char ch=((char*)errmsg.v)[i];
    if ((ch<0x20)||(ch>0x7e)) ch=' ';
  }
  err=http_xfer_set_status(rsp,500,"Conversion %d=>%d failed: %.*s",srcfmt,dstfmt,errmsg.c,errmsg.v);
  sr_encoder_cleanup(&errmsg);
  return err;
}

/* GET **, extracting from Zip container.
 */
 
static int eggdev_http_get_from_zip(
  struct http_xfer *rsp,
  const void *src,int srcc,
  const char *subpath,int subpathc,
  const char *zippath
) {
  struct zip_reader reader={0};
  if (zip_reader_init(&reader,src,srcc)<0) return -1;
  reader.udata_on_demand_only=1;
  struct zip_file file={0};
  while (zip_reader_next(&file,&reader)>0) {
    if (file.namec!=subpathc) continue;
    if (memcmp(file.name,subpath,subpathc)) continue;
    if (zip_file_uncompress(&file,&reader)<0) {
      zip_reader_cleanup(&reader);
      return http_xfer_set_status(rsp,500,"Failed to uncompress Zip member");
    }
    http_xfer_set_header(rsp,"Content-Type",12,eggdev_guess_mime_type(file.udata,file.usize,subpath,0),-1);
    int err=sr_encode_raw(http_xfer_get_body(rsp),file.udata,file.usize);
    zip_reader_cleanup(&reader);
    if (err<0) return http_xfer_set_status(rsp,500,"Internal error");
    return http_xfer_set_status(rsp,200,"OK");
  }
  zip_reader_cleanup(&reader);
  return http_xfer_set_status(rsp,404,"Not found in Zip");
}

/* GET **
 */
 
static int eggdev_cb_get_other(struct http_xfer *req,struct http_xfer *rsp) {
  struct eggdev_http_paths paths={0};
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if (eggdev_http_paths_resolve(&paths,reqpath,reqpathc,0)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  void *src=0;
  int srcc=file_read(&src,paths.local);
  if (srcc<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  if (paths.innerc) {
    int err=eggdev_http_get_from_zip(rsp,src,srcc,paths.inner,paths.innerc,paths.local);
    free(src);
    eggdev_http_paths_cleanup(&paths);
    return err;
  }
  http_xfer_set_header(rsp,"Content-Type",12,eggdev_guess_mime_type(src,srcc,paths.local,0),-1);
  int err=sr_encode_raw(http_xfer_get_body(rsp),src,srcc);
  free(src);
  eggdev_http_paths_cleanup(&paths);
  if (err<0) return http_xfer_set_status(rsp,500,"Internal error");
  return http_xfer_set_status(rsp,200,"OK");
}

/* PUT **
 */
 
static int eggdev_cb_put_other(struct http_xfer *req,struct http_xfer *rsp) {
  struct eggdev_http_paths paths={0};
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if (eggdev_http_paths_resolve(&paths,reqpath,reqpathc,1)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  if (dir_mkdirp_parent(paths.local)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,500,"mkdir failed");
  }
  struct sr_encoder *src=http_xfer_get_body(req);
  if (file_write(paths.local,src->v,src->c)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,500,"write failed");
  }
  eggdev_http_paths_cleanup(&paths);
  return http_xfer_set_status(rsp,200,"OK");
}

/* DELETE **
 */
 
static int eggdev_cb_delete_other(struct http_xfer *req,struct http_xfer *rsp) {
  struct eggdev_http_paths paths={0};
  const char *reqpath=0;
  int reqpathc=http_xfer_get_path(&reqpath,req);
  if (eggdev_http_paths_resolve(&paths,reqpath,reqpathc,1)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  char ftype=file_get_type(paths.local);
  if (!ftype) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,404,"Not found");
  }
  if (ftype!='f') {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,500,"Refusing to delete file of type '%c'",ftype);
  }
  if (unlink(paths.local)<0) {
    eggdev_http_paths_cleanup(&paths);
    return http_xfer_set_status(rsp,500,"unlink failed");
  }
  eggdev_http_paths_cleanup(&paths);
  return http_xfer_set_status(rsp,200,"OK");
}

/* Default handler, no method+path matched.
 */
 
static int eggdev_cb_unmatched(struct http_xfer *req,struct http_xfer *rsp) {
  return http_xfer_set_status(rsp,404,"Not found");
}

/* Serve callback.
 */
 
static int eggdev_cb_serve(struct http_xfer *req,struct http_xfer *rsp,void *userdata) {
  /**
  char method[32];
  int methodc=http_xfer_get_method(method,sizeof(method),req);
  if ((methodc<0)||(methodc>sizeof(method))) methodc=0;
  const char *path=0;
  int pathc=http_xfer_get_path(&path,req);
  fprintf(stderr,"%s: %.*s %.*s\n",__func__,methodc,method,pathc,path);
  /**/
  return http_dispatch(req,rsp,
    HTTP_METHOD_GET,"/api/webpath",eggdev_cb_get_webpath,
    HTTP_METHOD_GET,"/api/projname",eggdev_cb_get_projname,
    HTTP_METHOD_GET,"/api/buildfirst**",eggdev_cb_get_buildfirst,
    HTTP_METHOD_GET,"/api/symbols",eggdev_cb_get_symbols,
    HTTP_METHOD_GET,"/api/instruments",eggdev_cb_get_instruments,
    HTTP_METHOD_GET,"/api/toc**",eggdev_cb_get_toc,
    HTTP_METHOD_GET,"/api/allcontent**",eggdev_cb_get_allcontent,
    HTTP_METHOD_POST,"/api/convert",eggdev_cb_post_convert,
    HTTP_METHOD_GET,"",eggdev_cb_get_other,
    HTTP_METHOD_PUT,"",eggdev_cb_put_other,
    HTTP_METHOD_DELETE,"",eggdev_cb_delete_other,
    0,"",eggdev_cb_unmatched
  );
}

/* Signal handler.
 */
 
static void eggdev_cb_signal(int sigid) {
  switch (sigid) {
    case SIGINT: if (++(g.sigc)>=3) {
        fprintf(stderr,"Too many unprocessed signals.\n");
        exit(1);
      } break;
  }
}

/* Any htdocs whose local path begins "EGG_SDK/", replace with the SDK path.
 */
 
static int eggdev_serve_massage_htdocs() {
  int i=g.htdocsc;
  while (i-->0) {
    const char *src=g.htdocsv[i];
    int srcc=0,sepp=-1;
    for (;src[srcc];srcc++) {
      if ((sepp<0)&&(src[srcc]==':')) sepp=srcc;
    }
    const char *local=src;
    int localc=srcc;
    if (sepp>=0) {
      local+=sepp+1;
      localc-=sepp+1;
    }
    if ((localc>=8)&&!memcmp(local,"EGG_SDK/",8)) {
      local+=8;
      localc-=8;
      char nv[1024];
      int nc;
      if (sepp>=0) {
        nc=snprintf(nv,sizeof(nv),"%.*s:%s/%.*s",sepp,src,g.sdkpath,localc,local);
      } else {
        nc=snprintf(nv,sizeof(nv),"%s/%.*s",g.sdkpath,localc,local);
      }
      if ((nc<1)||(nc>=sizeof(nv))) return -1;
      char *cp=malloc(nc+1);
      if (!cp) return -1;
      memcpy(cp,nv,nc);
      cp[nc]=0;
      free(g.htdocsv[i]);
      g.htdocsv[i]=cp;
    }
  }
  return 0;
}

/* Serve, main entry point.
 */
 
int eggdev_main_serve() {
  int err=0;
  if (g.http) return -1;
  signal(SIGINT,eggdev_cb_signal);
  eggdev_serve_massage_htdocs();
  if (!g.port) g.port=8080;
  if (g.project) {
    if ((err=eggdev_client_set_root(g.project,-1))<0) return err;
  }
  struct http_context_delegate delegate={
    .cb_serve=eggdev_cb_serve,
  };
  if (!(g.http=http_context_new(&delegate))) return -1;
  if (http_listen(g.http,g.unsafe_external?0:1,g.port)<0) {
    fprintf(stderr,"%s: Failed to open TCP server on port %d\n",g.exename,g.port);
    http_context_del(g.http);
    g.http=0;
    return -2;
  }
  fprintf(stderr,"%s: Serving HTTP on port %d\n",g.exename,g.port);
  while (!g.terminate&&!g.sigc) {
    if ((err=http_update(g.http,1000))<0) {
      if (err!=-2) fprintf(stderr,"%s: Unspecified error updating HTTP stack.\n",g.exename);
      break;
    }
  }
  http_context_del(g.http);
  g.http=0;
  return err;
}
