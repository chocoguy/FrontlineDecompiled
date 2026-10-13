#include "types.h"

// Stand-ins for the AMC DDH (debug hardware) EXI2 driver; this build has no
// AMC hardware, so every call does nothing.

void EXI2_Init(void) {}

void EXI2_EnableInterrupts(void) {}

int EXI2_Poll(void) {
    return 0;
}

int EXI2_ReadN(void) {
    return 0;
}

int EXI2_WriteN(void) {
    return 0;
}

void EXI2_Reserve(void) {}

void EXI2_Unreserve(void) {}

BOOL AMC_IsStub(void) {
    return TRUE;
}
