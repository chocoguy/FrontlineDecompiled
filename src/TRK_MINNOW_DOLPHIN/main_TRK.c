#include <TRK_MINNOW_DOLPHIN/trk.h>

static DSError TRK_mainError;

DSError TRK_main(void) {
    TRK_mainError = TRKInitializeNub();
    if (TRK_mainError == kNoError) {
        TRKNubWelcome();
        TRKNubMainLoop();
    }
    TRK_mainError = TRKTerminateNub();
    return TRK_mainError;
}
