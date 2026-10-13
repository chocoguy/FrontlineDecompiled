// EA timer thread: runs the timer subroutines on messages from the TIMER alarm.
#ifndef _EA_SYS_TIMERTHREAD_H_
#define _EA_SYS_TIMERTHREAD_H_

#include "types.h"

enum TIMERMSG {
    TIMERMSG_TICK = 0,   // ttDoTimerMsg
    TIMERMSG_VBLANK = 1, // ttDoVTimerMsg
    TIMERMSG_QUIT = 2,
};

int ttInit();
void ttKill();
void ttMsg(TIMERMSG msg);
void ttDoTimerMsg();
void ttDoVTimerMsg();
void InitAlarm();

#endif
