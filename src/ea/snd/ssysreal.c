#include "ea/snd/sndi.h"
#include "ea/sys/systask.h"
#include "ea/sys/exit.h"

// SYNCTASK that services the sound system every run.
int SNDREAL_systemtask(int, int) {
    SNDSYS_service();
    return 0;
}

void SNDREAL_abortmsg(char*) {}

// Hook the sound system into the REAL library: abort messages, a service task, exit cleanup.
extern "C" int SNDSYS_vectortoreal(void) {
    static int systaskadded = 0;

    sndgs.opts.abortmsg = SNDREAL_abortmsg;
    if (systaskadded == 0) {
        SYNCTASK_add(SNDREAL_systemtask, 0, 1);
        systaskadded = 1;
    }
    REAL_addexit(SNDREAL_exithandler);
    return 0;
}
