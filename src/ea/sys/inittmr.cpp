#include "ea/sys/timerthread.h"
#include "ea/sys/systemvars.h"
#include "ea/sys/exit.h"
#include "dolphin/os.h"

extern "C" void TIMER_restore(void);

static OSAlarm Alarm;
static s8 bIsAlarmInited;
static s8 bIsTimerInited;

static void AlarmHandler(OSAlarm*, OSContext*);

void InitAlarm() {
    if (!bIsAlarmInited) {
        OSInitAlarm();
        bIsAlarmInited++;
    }
}

// A periodic alarm at `hz` that wakes the timer thread.
extern "C" s32 TIMER_init(s32 hz) {
    OSTime start;
    int period;

    if (bIsTimerInited) {
        return TIMERhz;
    }
    if (hz == 0) {
        hz = 100;
    }
    TIMERhz = hz;
    start = OSGetTime();
    period = (f32)OS_TIMER_CLOCK * (1.0f / (f32)hz);
    ttInit();
    OSSetPeriodicAlarm(&Alarm, start, period, AlarmHandler);
    bIsTimerInited = 1;
    REAL_addexit(TIMER_restore);
    return TIMERhz;
}

extern "C" void TIMER_restore(void) {
    if (bIsTimerInited) {
        bIsTimerInited = 0;
        OSCancelAlarm(&Alarm);
        ttKill();
    }
}

void ttDoTimerMsg() {
    int i;
    ticks++;
    libticks++;
    for (i = 0; i < 8; i++) {
        if (tmrsub[i] != NULL) {
            tmrsub[i]();
        }
    }
}

static void AlarmHandler(OSAlarm*, OSContext*) {
    ttMsg(TIMERMSG_TICK);
}
