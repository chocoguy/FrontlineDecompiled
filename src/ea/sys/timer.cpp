#include "ea/sys/timer.h"
#include "ea/sys/systemvars.h"

extern "C" int TIMER_gettick(void) {
    return ticks;
}

extern "C" int TIMER_getfrequency(void) {
    return TIMERhz;
}
