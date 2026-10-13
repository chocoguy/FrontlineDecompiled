#include <TRK_MINNOW_DOLPHIN/trk.h>

static u8 bUseSerialIO;

u8 GetUseSerialIO(void) {
    return bUseSerialIO;
}

void SetUseSerialIO(u8 sio) {
    bUseSerialIO = sio;
}
