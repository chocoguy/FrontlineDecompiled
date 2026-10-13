#include <TRK_MINNOW_DOLPHIN/trk.h>

TRKEventQueue gTRKEventQueue;

void TRKDestructEvent(TRKEvent* event) {
    TRKReleaseBuffer(event->msgBufID);
}

void TRKConstructEvent(TRKEvent* event, int eventType) {
    event->eventType = eventType;
    event->eventID = 0;
    event->msgBufID = -1;
}

DSError TRKPostEvent(TRKEvent* event) {
    DSError ret;
    int nextEventID;

    ret = kNoError;
    TRKAcquireMutex(&gTRKEventQueue);

    if (gTRKEventQueue.count == 2) {
        ret = kEventQueueFull;
    } else {
        nextEventID = (gTRKEventQueue.next + gTRKEventQueue.count) % 2;
        TRK_memcpy(&gTRKEventQueue.events[nextEventID], event, sizeof(TRKEvent));
        gTRKEventQueue.events[nextEventID].eventID = gTRKEventQueue.eventID;
        gTRKEventQueue.eventID++;
        if (gTRKEventQueue.eventID < 0x100) {
            gTRKEventQueue.eventID = 0x100;
        }
        gTRKEventQueue.count++;
    }

    TRKReleaseMutex(&gTRKEventQueue);
    return ret;
}

BOOL TRKGetNextEvent(TRKEvent* event) {
    BOOL retVal;

    retVal = FALSE;
    TRKAcquireMutex(&gTRKEventQueue);

    if (0 < gTRKEventQueue.count) {
        TRK_memcpy(event, &gTRKEventQueue.events[gTRKEventQueue.next], sizeof(TRKEvent));
        gTRKEventQueue.count--;
        if (++gTRKEventQueue.next == 2) {
            gTRKEventQueue.next = 0;
        }
        retVal = TRUE;
    }

    TRKReleaseMutex(&gTRKEventQueue);
    return retVal;
}

DSError TRKInitializeEventQueue(void) {
    TRKInitializeMutex(&gTRKEventQueue);
    TRKAcquireMutex(&gTRKEventQueue);
    gTRKEventQueue.count = 0;
    gTRKEventQueue.next = 0;
    gTRKEventQueue.eventID = 0x100;
    TRKReleaseMutex(&gTRKEventQueue);
    return kNoError;
}
