#include "ea/real.h"
#include "ea/sys/signals.h"
#include "ea/sys/timerthread.h"

OSThread* g_thMain;

// An alarm with the signal it sets right after it.
struct THREADSLEEP {
    /* 0x00 */ OSAlarm alarm;
    /* 0x28 */ SIGNAL signal;
};

extern "C" void THREAD_init(void) {
    InitAlarm();
    g_thMain = OSGetCurrentThread();
}

static void AlarmHandler(OSAlarm* alarm, OSContext*) {
    SIGNAL_set(&((THREADSLEEP*)alarm)->signal);
}

// Sleep for `ms` milliseconds (0: just yield).
extern "C" void THREAD_yield(int ms) {
    OSTime ticks = (OSTime)ms * (s32)(OS_TIMER_CLOCK / 1000);
    if (ticks == 0) {
        OSYieldThread();
    } else {
        THREADSLEEP sleep;
        SIGNAL_create(&sleep.signal);
        OSCreateAlarm(&sleep.alarm);
        OSSetAlarm(&sleep.alarm, ticks, AlarmHandler);
        SIGNAL_wait(&sleep.signal);
        OSCancelAlarm(&sleep.alarm);
    }
}

// 0: the main thread, -1: the current thread, otherwise an OSThread*.
extern "C" int THREAD_iscurrent(int thread) {
    OSThread* current = OSGetCurrentThread();
    OSThread* other;
    if (thread == 0) {
        other = g_thMain;
    } else if (thread == -1) {
        other = OSGetCurrentThread();
    } else {
        other = (OSThread*)thread;
    }
    return current == other;
}
