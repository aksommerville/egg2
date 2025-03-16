#include "eggdev/eggdev_internal.h"
#include "builder.h"
#include <sys/wait.h>
#include <unistd.h>

/* Cleanup process.
 */
 
void builder_process_cleanup(struct builder_process *process) {
  if (process->pid&&process->running) kill(process->pid,SIGKILL);
  process->running=0;
  if (process->fd>0) close(process->fd);
  if (process->cmd) free(process->cmd);
}

/* Split command into the child process's argv.
 * On success, there is always at least one valid string, and it's always terminated with a NULL.
 */
 
static int builder_cmd_split(void *dstpp,const char *src,int srcc) {
  int argc=0,arga=16;
  char **argv=malloc(sizeof(void*)*arga);
  if (!argv) return -1;
  int srcp=0;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    // I'm trying not to need quotes... Can we get by without them?
    const char *token=src+srcp;
    int tokenc=0;
    while ((srcp<srcc)&&((unsigned char)src[srcp++]>0x20)) tokenc++;
    if (argc>arga-2) {
      arga+=16;
      void *nv;
      if ((arga>INT_MAX/sizeof(void*))||!(nv=realloc(argv,sizeof(void*)*arga))) {
        while (argc-->0) free(argv[argc]);
        free(argv);
        return -1;
      }
      argv=nv;
    }
    char *nv=malloc(tokenc+1);
    if (!nv) {
      while (argc-->0) free(argv[argc]);
      free(argv);
      return -1;
    }
    memcpy(nv,token,tokenc);
    nv[tokenc]=0;
    argv[argc++]=nv;
  }
  if (!argc) {
    free(argv);
    return -1;
  }
  argv[argc]=0;
  *(void**)dstpp=argv;
  return argc;
}

/* Launch process.
 */
 
int builder_begin_command(struct builder *builder,struct builder_step *step,const char *cmd,int cmdc,const void *in,int inc) {

  if (builder->processc>=builder->job_limit) return -1;
  if (builder->processc>=builder->processa) {
    int na=builder->processa+8;
    if (na>INT_MAX/sizeof(struct builder_process)) return -1;
    void *nv=realloc(builder->processv,sizeof(struct builder_process)*na);
    if (!nv) return -1;
    builder->processv=nv;
    builder->processa=na;
  }
  struct builder_process *process=builder->processv+builder->processc++;
  memset(process,0,sizeof(struct builder_process));
  process->step=step;
  if (process->cmd=malloc(cmdc+1)) {
    memcpy(process->cmd,cmd,cmdc);
    process->cmd[cmdc]=0;
    process->cmdc=cmdc;
  }
  
  char **argv=0;
  int argc=builder_cmd_split(&argv,cmd,cmdc);
  if (argc<0) {
    builder->processc--;
    return -1;
  }
  #define FREEARGV { while (argc-->0) free(argv[argc]); free(argv); }
  
  int fdv[2]={0};
  if (inc) {
    if (pipe(fdv)) ;
  }
  int sofdv[2]={0};
  if (pipe(sofdv)) ;
  
  process->pid=fork();
  if (process->pid<0) {
    FREEARGV
    builder->processc--;
    if (inc) { close(fdv[0]); close(fdv[1]); }
    close(sofdv[0]);
    close(sofdv[1]);
    return -1;
  }
  
  // Parent? Tidy up and get out.
  if (process->pid) {
    if (inc) {
      int err=write(fdv[1],in,inc);
      close(fdv[0]);
      close(fdv[1]);
    }
    close(sofdv[1]);
    process->fd=sofdv[0];
    FREEARGV
    return 0;
  }
  
  dup2(sofdv[1],STDOUT_FILENO);
  dup2(sofdv[1],STDERR_FILENO);
  close(sofdv[0]);
  if (inc) {
    close(fdv[1]);
    dup2(fdv[0],STDIN_FILENO);
  }
  
  execvp(argv[0],argv);
  #undef FREEARGV
  exit(1);
  return -1;
}

/* Get the process running a given step, if there is one.
 */
 
static struct builder_process *builder_process_for_step(struct builder *builder,const struct builder_step *step) {
  struct builder_process *process=builder->processv;
  int i=builder->processc;
  for (;i-->0;process++) {
    if (process->step==step) return process;
  }
  return 0;
}

/* Choose a step from within the given indices (lo inclusive, hi exclusive).
 * We return (lo<=n<hi) if a step is ready to begin, or <0 if we're stalled.
 */
 
static int builder_choose_next_step(struct builder *builder,int lo,int hi) {
  const struct builder_step *step=builder->stepv+lo;
  int p=lo; for (;p<hi;p++,step++) {
    if (step->file->ready) continue;
    if (builder_process_for_step(builder,step)) continue;
    int reqs_ready=1;
    struct builder_file **req=step->file->reqv;
    int reqi=step->file->reqc;
    for (;reqi-->0;req++) {
      if (!(*req)->ready) {
        reqs_ready=0;
        break;
      }
    }
    if (reqs_ready) return p;
  }
  return -1;
}

/* If this step can be done synchronously, do it.
 * Otherwise launch the process for it and prepare the necessary bookkeeping.
 * Call only when all prereqs are ready.
 * Fail with -3 if the job would be asynchronous but we don't have an available process slot.
 */
 
static int builder_begin_step(struct builder *builder,struct builder_step *step) {
  switch (step->file->hint) {
  
    // sync...
    #define SYNC builder_log(builder,"  %s\n",step->file->path);
    case BUILDER_FILE_HINT_DATAROM: SYNC return build_datarom(builder,step->file);
    case BUILDER_FILE_HINT_FULLROM: SYNC return build_fullrom(builder,step->file);
    case BUILDER_FILE_HINT_STANDALONE: SYNC return build_standalone(builder,step->file);
    case BUILDER_FILE_HINT_SEPARATE: SYNC return build_separate(builder,step->file);
    #undef SYNC

    // async...
    #define CKASYNC if (builder->processc>=builder->job_limit) return -3; builder_log(builder,"  %s\n",step->file->path);
    case BUILDER_FILE_HINT_CODE1: CKASYNC return builder_schedule_link(builder,step);
    case BUILDER_FILE_HINT_OBJ: CKASYNC return builder_schedule_compile(builder,step);
    case BUILDER_FILE_HINT_EXE: CKASYNC return builder_schedule_link(builder,step);
    case BUILDER_FILE_HINT_DATAO: CKASYNC return builder_schedule_datao(builder,step);
    #undef CKASYNC
    
    default: return builder_error(builder,"%s: No rule to build file. (hint=%d)\n",step->file->path,step->file->hint);
  }
  return -1;
}

/* Dump the log for a failed process.
 * If it's empty, print a generic failure message.
 */
 
static void builder_dump_process_log(struct builder *builder,struct builder_process *process) {
  int ok=0;
  if (process->fd>0) {
    char msg[1024];
    for (;;) {
      int msgc=read(process->fd,msg,sizeof(msg));
      if (msgc<1) break;
      ok=1;
      int msgp=0;
      while (msgp<msgc) {
        const char *line=msg+msgp;
        int linec=0;
        while ((msgp<msgc)&&(msg[msgp++]!=0x0a)) linec++;
        builder_error(builder,line,linec);
      }
    }
  }
  if (!ok) {
    builder_error(builder,"%s: Child process exitted abnormally.\n",process->step->file->path);
  }
  builder_error(builder,"%s: Failed command:\n%.*s\n",process->step->file->path,process->cmdc,process->cmd);
}

/* No steps are ready to build.
 * If we have processes in flight, wait for at least one to finish.
 * Otherwise fail.
 */
 
static int builder_wait(struct builder *builder) {
  for (;;) {
    if (builder->processc<1) return builder_error(builder,"%s: PANIC! Entered %s with no jobs running.\n",g.exename,__func__);
    int wstatus=0;
    int pid=wait(&wstatus);
    if (pid<=0) return -1;
    int status=1;
    if (WIFEXITED(wstatus)) status=WEXITSTATUS(wstatus);
    int i=builder->processc;
    while (i-->0) {
      struct builder_process *process=builder->processv+i;
      if (process->pid!=pid) continue;
      if (status) {
        builder_dump_process_log(builder,process);
      }
      process->running=0;
      struct builder_step *step=process->step;
      step->file->ready=1;
      builder_process_cleanup(process);
      builder->processc--;
      memmove(process,process+1,sizeof(struct builder_process)*(builder->processc-i));
      return status?-2:0;
    }
    // The terminated process was not in our list. Weird. Wait for another one.
  }
}

/* Execute plan, main entry point.
 */
 
int builder_execute_plan(struct builder *builder) {
  int stepplo=0,stepphi=builder->stepc,err;
  while (stepplo<stepphi) {
    int stepp=builder_choose_next_step(builder,stepplo,stepphi);
    if (stepp>=0) {
      if ((err=builder_begin_step(builder,builder->stepv+stepp))<0) {
        if (err==-3) { // Signal that we need to stall.
          if ((err=builder_wait(builder))<0) return err;
        } else {
          return err;
        }
      }
    } else {
      if ((err=builder_wait(builder))<0) return err;
    }
    while ((stepplo<stepphi)&&builder->stepv[stepplo].file->ready) stepplo++;
    while ((stepplo<stepphi)&&builder->stepv[stepphi-1].file->ready) stepphi--;
  }
  if (builder->processc) return builder_error(builder,"%s: Finished %s with %d processes still running.\n",g.exename,__func__,builder->processc);
  return 0;
}
