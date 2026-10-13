#include "types.h"

// AX DSP microcode.
u8 axDspSlave[0x19E0] __attribute__((aligned(32))) = {
#include "dolphin/ax/axDspSlave.inc"
};

u16 axDspSlaveLength = sizeof(axDspSlave);
