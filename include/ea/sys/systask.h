// EA library tasks (SYNCTASK_*), run from the main loop.
#ifndef _EA_SYS_SYSTASK_H_
#define _EA_SYS_SYSTASK_H_

#include "types.h"

// arg: SYNCTASK_run's argument; late: libticks past the due time.
typedef int (*SYNCTASKFUNC)(int arg, int late);

#ifdef __cplusplus
extern "C" {
#endif
void SYNCTASK_init(void);
// Registers or reschedules a task: period in libticks (-1: every run), first run after `delay`.
void SYNCTASK_add(SYNCTASKFUNC task, int period, int delay);
void SYNCTASK_del(void (*task)(void));
int SYNCTASK_run(int arg);
#ifdef __cplusplus
}
#endif

#endif
