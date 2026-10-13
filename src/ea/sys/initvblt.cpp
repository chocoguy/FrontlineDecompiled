#include "ea/sys/timerthread.h"
#include "ea/sys/systemvars.h"

void ttDoVTimerMsg() {
    int i;
    vblticks++;
    for (i = 0; i < 8; i++) {
        if (vbltmrsub[i] != NULL) {
            vbltmrsub[i]();
        }
    }
}
