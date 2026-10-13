#include <TRK_MINNOW_DOLPHIN/trk.h>

// Single-threaded: the mutexes are no-ops.

DSError TRKReleaseMutex(void* mutex) {
    return kNoError;
}

DSError TRKAcquireMutex(void* mutex) {
    return kNoError;
}

DSError TRKInitializeMutex(void* mutex) {
    return kNoError;
}
