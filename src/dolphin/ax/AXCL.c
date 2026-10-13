#include <dolphin/ax.h>
#include <dolphin/os.h>

static u16 __AXCommandList[2][384] __attribute__((aligned(32)));
static u32 __AXCommandListPosition;
static u16* __AXClWrite;
static u32 __AXCommandListCycles;
u32 __AXClMode;

u32 __AXGetCommandListCycles(void) {
    return __AXCommandListCycles;
}

// Returns this frame's list and switches writing to the other one.
u32 __AXGetCommandListAddress(void) {
    u32 addr = (u32)__AXCommandList[__AXCommandListPosition];
    __AXCommandListPosition++;
    __AXCommandListPosition &= 1;
    __AXClWrite = __AXCommandList[__AXCommandListPosition];
    return addr;
}

#define CL_WRITE(x) (*__AXClWrite++ = (u16)(x))

void __AXNextFrame(void* sbuffer, void* buffer) {
    u32 data;
    u16* list = __AXClWrite;

    __AXCommandListCycles = 0x1A9;
    data = (u32)__AXGetStudio();
    CL_WRITE(0);
    CL_WRITE(data >> 16);
    CL_WRITE(data);
    __AXCommandListCycles += 0x2E44;
    switch (__AXClMode) {
    case AX_MODE_STEREO:
        CL_WRITE(7);
        CL_WRITE((u32)sbuffer >> 16);
        CL_WRITE((u32)sbuffer);
        __AXCommandListCycles += 0x546;
        break;
    case AX_MODE_SURROUND:
    case 4:
        CL_WRITE(0x11);
        CL_WRITE((u32)sbuffer >> 16);
        CL_WRITE((u32)sbuffer);
        __AXCommandListCycles += 0x5E6;
        break;
    }
    data = (u32)__AXGetPBs();
    CL_WRITE(2);
    CL_WRITE(data >> 16);
    CL_WRITE(data);
    CL_WRITE(3);
    __AXGetAuxAInput(&data);
    if (data != 0) {
        CL_WRITE(4);
        CL_WRITE(data >> 16);
        CL_WRITE(data);
        __AXGetAuxAOutput(&data);
        CL_WRITE(data >> 16);
        CL_WRITE(data);
        __AXCommandListCycles += 0xDED;
    }
    __AXGetAuxBInput(&data);
    if (data != 0) {
        if (__AXClMode == 4) {
            CL_WRITE(0x10);
            __AXCommandListCycles += 0xDED;
        } else {
            CL_WRITE(5);
            __AXCommandListCycles += 0xDED;
        }
        CL_WRITE(data >> 16);
        CL_WRITE(data);
        __AXGetAuxBOutput(&data);
        CL_WRITE(data >> 16);
        CL_WRITE(data);
    }
    CL_WRITE(0xE);
    CL_WRITE((u32)sbuffer >> 16);
    CL_WRITE((u32)sbuffer);
    CL_WRITE((u32)buffer >> 16);
    CL_WRITE((u32)buffer);
    __AXCommandListCycles += 0x2710;
    CL_WRITE(0xF);
    __AXCommandListCycles += 2;
    DCFlushRange(list, sizeof(__AXCommandList[0]));
}

void __AXClInit(void) {
    __AXClMode = AX_MODE_STEREO;
    __AXCommandListPosition = 0;
    __AXClWrite = __AXCommandList[0];
}

void __AXClQuit(void) {
}

void AXSetMode(u32 mode) {
    if (__AXClMode != mode) {
        __AXClMode = mode;
    }
}
