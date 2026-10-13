#include <TRK_MINNOW_DOLPHIN/trk.h>

void TRKNubMainLoop(void) {
    TRKBuffer* msg;
    TRKEvent event;
    BOOL isShutdownRequested;
    BOOL isNewInput;

    isShutdownRequested = FALSE;
    isNewInput = FALSE;

    while (isShutdownRequested == FALSE) {
        if (TRKGetNextEvent(&event) != FALSE) {
            isNewInput = FALSE;

            switch (event.eventType) {
            case kNullEvent:
                break;
            case kRequestEvent:
                msg = TRKGetBuffer(event.msgBufID);
                TRKDispatchMessage(msg);
                break;
            case kShutdownEvent:
                isShutdownRequested = TRUE;
                break;
            case kBreakpointEvent:
            case kExceptionEvent:
                TRKTargetInterrupt(&event);
                break;
            case kSupportEvent:
                TRKTargetSupportRequest();
                break;
            }

            TRKDestructEvent(&event);
        } else if (isNewInput == FALSE || *(u8*)gTRKInputPendingPtr != '\0') {
            isNewInput = TRUE;
            TRKGetInput();
        } else {
            if (TRKTargetStopped() == FALSE) {
                TRKTargetContinue();
            }
            isNewInput = FALSE;
        }
    }
}
