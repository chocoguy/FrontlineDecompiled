#include "ea/sys/timerthread.h"
#include "dolphin/os.h"

// The original's block is 32-byte aligned (thread context and stack want 8).
OSMessageQueue TimerThreadMsgQ __attribute__((aligned(32)));
OSMessage TimerThreadMsgData[0x20];
OSThread TimerThread __attribute__((aligned(32)));
u8 TimerThreadStack[0x1000] __attribute__((aligned(32)));
int TimesInited;

static void* TimerThreadFunc(void*);

int ttInit() {
    if (TimesInited == 0) {
        OSCreateThread(&TimerThread, TimerThreadFunc, NULL, TimerThreadStack + sizeof(TimerThreadStack),
                       sizeof(TimerThreadStack), 4, 1);
        OSInitMessageQueue(&TimerThreadMsgQ, TimerThreadMsgData, 0x20);
        OSResumeThread(&TimerThread);
    }
    return TimesInited++;
}

void ttKill() {
    if (--TimesInited == 0) {
        ttMsg(TIMERMSG_QUIT);
    }
}

void ttMsg(TIMERMSG msg) {
    OSSendMessage(&TimerThreadMsgQ, (OSMessage)msg, OS_MESSAGE_NOBLOCK);
}

static void* TimerThreadFunc(void*) {
    OSMessage msg = (OSMessage)TIMERMSG_TICK;
    while ((sptr)(msg) != TIMERMSG_QUIT) {
        OSReceiveMessage(&TimerThreadMsgQ, &msg, OS_MESSAGE_BLOCK);
        if ((sptr)(msg) == TIMERMSG_TICK) {
            ttDoTimerMsg();
        } else if ((sptr)(msg) == TIMERMSG_VBLANK) {
            ttDoVTimerMsg();
        }
    }
    return NULL;
}
