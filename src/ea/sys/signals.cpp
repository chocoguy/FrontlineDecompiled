#include "ea/sys/signals.h"

extern "C" int SIGNAL_create(SIGNAL* signal) {
    OSInitMessageQueue(&signal->queue, &signal->message, 1);
    return 1;
}

extern "C" BOOL SIGNAL_set(SIGNAL* signal) {
    int message = 0;
    return OSSendMessage(&signal->queue, &message, OS_MESSAGE_NOBLOCK);
}

extern "C" BOOL SIGNAL_wait(SIGNAL* signal) {
    OSMessage message;
    return OSReceiveMessage(&signal->queue, &message, OS_MESSAGE_BLOCK);
}
