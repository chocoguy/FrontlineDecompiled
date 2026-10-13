#include "game/real_bridge.h"

#include "ea/real.h"
#include "game/memory.h"

void ShutdownREAL() {
    ASYNCFILE_restore();
    FILESYS_restore();
    TIMER_restore();
    MEM_restore();
    REAL_restore();
}

void UpdateREAL() {
    SYNCTASK_run(0);
}

void InitREAL(int) {
    CPU_detect();
    TIMER_init(TIMERhz);
    THREAD_init();
    SYNCTASK_init();
    PAD_init();
    // The game polls the pads itself instead of from the task list.
    SYNCTASK_del(PAD_update);
    FILESYS_setmemcallbacks(DWI_alloc, (FILESYS_FreeFn)MEM_free);
    FILESYS_init(0, 0, 0);
    ASYNCFILE_init(6, 0);
}
