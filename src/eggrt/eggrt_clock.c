#include "eggrt_internal.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define EGGRT_TARGET_PERIOD (1.0/60.0)
#define EGGRT_MIN_PERIOD    0.012 /* ~83 hz */
#define EGGRT_MAX_PERIOD    0.020 /*  50 hz */

/* Primitives.
 */
 
static double eggrt_now_real() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}
 
static double eggrt_now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

static void eggrt_sleep(double s) {
  int us=(int)(s*1000000.0)+1; // Add one to round up, so they don't call again due to a few dangling nanoseconds.
  if (us<1) return;
  usleep(us);
}

/* Init.
 */
 
void eggrt_clock_init() {
  eggrt.updframec=0;
  eggrt.clockfaultc=0;
  eggrt.clockclampc=0;
  eggrt.starttime_cpu=eggrt_now_cpu();
  eggrt.starttime_real=eggrt_now_real();
  // Cheat the "previous time" back by one frame so the first update doesn't sleep.
  eggrt.last_update_time=eggrt.starttime_real-EGGRT_TARGET_PERIOD;
}

/* Update.
 */
 
double eggrt_clock_update() {
  if (eggrt.updframec<INT_MAX) eggrt.updframec++;
  double elapsed;
  switch (eggrt.clockmode) {
  
    case EGGRT_CLOCKMODE_REDLINE: {
        elapsed=EGGRT_TARGET_PERIOD;
        // Don't even bother updating (last_update_time) or the rest. We're a dummy clock.
      } break;
    
    case EGGRT_CLOCKMODE_UNIFORM: {
        elapsed=EGGRT_TARGET_PERIOD;
        eggrt.last_update_time+=EGGRT_TARGET_PERIOD;
        double now=eggrt_now_real();
        for (;;) {
          double sleeptime=eggrt.last_update_time-now;
          if (sleeptime<-1.0) {
            // last_update_time is unreasonably far in the past. Clock is broken.
            eggrt.clockfaultc++;
            eggrt.last_update_time=now;
            break;
          }
          if (sleeptime<=0.0) break;
          if (sleeptime>EGGRT_TARGET_PERIOD) {
            // last_update_time is unreasonably far in the future. Clock is broken.
            eggrt.clockfaultc++;
            eggrt.last_update_time=now;
            break;
          }
          eggrt_sleep(sleeptime);
          now=eggrt_now_real();
        }
      } break;
  
    case EGGRT_CLOCKMODE_NORMAL:
    default: {
        double next_update_time=eggrt.last_update_time+EGGRT_TARGET_PERIOD;
        double now=eggrt_now_real();
        for (;;) {
          double sleeptime=next_update_time-now;
          if (sleeptime<-1.0) {
            eggrt.clockfaultc++;
            break;
          }
          if (sleeptime<=0.0) break;
          if (sleeptime>EGGRT_TARGET_PERIOD) {
            eggrt.clockfaultc++;
            break;
          }
          eggrt_sleep(sleeptime);
          now=eggrt_now_real();
        }
        elapsed=now-eggrt.last_update_time;
        eggrt.last_update_time=now;
        if (elapsed<EGGRT_MIN_PERIOD) {
          elapsed=EGGRT_MIN_PERIOD;
          eggrt.clockclampc++;
        } else if (elapsed>EGGRT_MAX_PERIOD) {
          elapsed=EGGRT_MAX_PERIOD;
          eggrt.clockclampc++;
        }
      }
  }
  return elapsed;
}

/* Report.
 */
 
void eggrt_clock_report() {
  if (eggrt.updframec<1) return;
  double elapsed_real=eggrt_now_real()-eggrt.starttime_real;
  double elapsed_cpu=eggrt_now_cpu()-eggrt.starttime_cpu;
  double avgrate=(double)eggrt.updframec/elapsed_real;
  double cpuload=elapsed_cpu/elapsed_real;
  fprintf(stderr,
    "%d frames in %.03f s, average %.03f Hz, CPU load %.06f, fault=%d, clamp=%d\n",
    eggrt.updframec,elapsed_real,avgrate,cpuload,eggrt.clockfaultc,eggrt.clockclampc
  );
}
