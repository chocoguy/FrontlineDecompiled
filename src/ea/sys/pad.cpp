#include "ea/sys/pad.h"
#include "ea/sys/ipad.h"

extern "C" void PAD_init(void) {
    iPAD_init();
}

extern "C" PADStatus* PAD_getdataptr(int port) {
    return &gPadStatus[port];
}

extern "C" void PAD_update(void) {
    iPAD_update();
}
