#include <dolphin/ar.h>
#include <dolphin/os.h>
#include <string.h>

#define __DSPRegs ((volatile u16*)0xCC005000)

void PPCSync(void);

static ARCallback __AR_Callback;
static u32 __AR_Size;
static u32 __AR_InternalSize;
static u32 __AR_ExpansionSize;
static u32 __AR_StackPointer;
static u32 __AR_FreeBlocks;
static u32* __AR_BlockLength;
static volatile BOOL __AR_init_flag;

static void __ARHandler(__OSInterrupt interrupt, OSContext* context);
static void __ARChecksize(void);

ARCallback ARRegisterDMACallback(ARCallback callback) {
    ARCallback old = __AR_Callback;
    BOOL enabled = OSDisableInterrupts();
    __AR_Callback = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length) {
    BOOL enabled = OSDisableInterrupts();
    __DSPRegs[16] = (u16)((__DSPRegs[16] & ~0x3FF) | (u16)(mainmem_addr >> 16));
    __DSPRegs[17] = (u16)((__DSPRegs[17] & ~0xFFE0) | (u16)(mainmem_addr & 0xFFFF));
    __DSPRegs[18] = (u16)((__DSPRegs[18] & ~0x3FF) | (u16)(aram_addr >> 16));
    __DSPRegs[19] = (u16)((__DSPRegs[19] & ~0xFFE0) | (u16)(aram_addr & 0xFFFF));
    __DSPRegs[20] = (u16)((__DSPRegs[20] & ~0x8000) | (type << 15));
    __DSPRegs[20] = (u16)((__DSPRegs[20] & ~0x3FF) | (u16)(length >> 16));
    __DSPRegs[21] = (u16)((__DSPRegs[21] & ~0xFFE0) | (u16)(length & 0xFFFF));
    OSRestoreInterrupts(enabled);
}

BOOL ARCheckInit(void) {
    return __AR_init_flag;
}

u32 ARInit(u32* stack_index_addr, u32 num_entries) {
    BOOL enabled;
    u16 refresh;
    if (__AR_init_flag == TRUE) {
        return 0x4000;
    }
    enabled = OSDisableInterrupts();
    __AR_Callback = NULL;
    __OSSetInterruptHandler(6, __ARHandler);
    __OSUnmaskInterrupts(0x02000000);
    __AR_FreeBlocks = num_entries;
    __AR_StackPointer = 0x4000;
    __AR_BlockLength = stack_index_addr;
    refresh = (u16)(__DSPRegs[13] & 0xFF);
    __DSPRegs[13] = (u16)((__DSPRegs[13] & ~0xFF) | (refresh & 0xFF));
    __ARChecksize();
    __AR_init_flag = TRUE;
    OSRestoreInterrupts(enabled);
    return __AR_StackPointer;
}

u32 ARGetBaseAddress(void) {
    return 0x4000;
}

static void __ARHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 tmp = __DSPRegs[5];
    tmp = (u16)((tmp & ~0x88) | 0x20);
    __DSPRegs[5] = tmp;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AR_Callback != NULL) {
        __AR_Callback();
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static inline void __ARWaitForDMA(void) {
    while (__DSPRegs[5] & 0x200) {
    }
    __DSPRegs[5] = (u16)((__DSPRegs[5] & ~0x88) | 0x20);
}

static inline void __ARDMA(u32 dir, u32 mmem_addr, u32 aram_addr, u32 length) {
    __DSPRegs[16] = (u16)((__DSPRegs[16] & ~0x3FF) | (u16)(mmem_addr >> 16));
    __DSPRegs[17] = (u16)((__DSPRegs[17] & ~0xFFE0) | (u16)(mmem_addr & 0xFFFF));
    __DSPRegs[18] = (u16)((__DSPRegs[18] & ~0x3FF) | (u16)(aram_addr >> 16));
    __DSPRegs[19] = (u16)((__DSPRegs[19] & ~0xFFE0) | (u16)(aram_addr & 0xFFFF));
    if (dir) {
        __DSPRegs[20] = (u16)(__DSPRegs[20] | 0x8000);
    } else {
        __DSPRegs[20] = (u16)(__DSPRegs[20] & ~0x8000);
    }
    __DSPRegs[20] = (u16)((__DSPRegs[20] & ~0x3FF) | (u16)(length >> 16));
    __DSPRegs[21] = (u16)((__DSPRegs[21] & ~0xFFE0) | (u16)(length & 0xFFFF));
    __ARWaitForDMA();
}

#define __ARWriteDMA(m, a, l) __ARDMA(0, m, a, l)
#define __ARReadDMA(m, a, l) __ARDMA(1, m, a, l)

#define ROUND_UP_32(p) ((u32)(p) + 31 & ~31)

// Finds how much expansion ARAM is fitted by writing a pattern and checking where it mirrors.
static void __ARChecksize(void) {
    u8 test_data_pad[0x20 + 31];
    u8 dummy_data_pad[0x20 + 31];
    u8 buffer_pad[0x20 + 31];
    u32* test_data;
    u32* dummy_data;
    u32* buffer;
    u16 ARAM_mode;
    u32 ARAM_size;
    u32 i;

    while (!(__DSPRegs[11] & 1)) {
    }
    ARAM_mode = 3;
    ARAM_size = __AR_InternalSize = 0x1000000;
    __DSPRegs[9] = (u16)((__DSPRegs[9] & ~0x3F) | 0x23);
    test_data = (u32*)ROUND_UP_32(test_data_pad);
    dummy_data = (u32*)ROUND_UP_32(dummy_data_pad);
    buffer = (u32*)ROUND_UP_32(buffer_pad);
    for (i = 0; i < 8; i++) {
        test_data[i] = 0xDEADBEEF;
        dummy_data[i] = 0xBAD0BAD0;
    }
    DCFlushRange(test_data, 0x20);
    DCFlushRange(dummy_data, 0x20);
    __AR_ExpansionSize = 0;
    __ARWriteDMA((u32)dummy_data, ARAM_size + 0, 0x20);
    __ARWriteDMA((u32)dummy_data, ARAM_size + 0x200000, 0x20);
    __ARWriteDMA((u32)dummy_data, ARAM_size + 0x1000000, 0x20);
    __ARWriteDMA((u32)dummy_data, ARAM_size + 0x200, 0x20);
    __ARWriteDMA((u32)dummy_data, ARAM_size + 0x400000, 0x20);
    memset(buffer, 0, 0x20);
    DCFlushRange(buffer, 0x20);
    __ARWriteDMA((u32)test_data, ARAM_size + 0, 0x20);
    DCInvalidateRange(buffer, 0x20);
    __ARReadDMA((u32)buffer, ARAM_size + 0, 0x20);
    PPCSync();
    if (buffer[0] == test_data[0]) {
        memset(buffer, 0, 0x20);
        DCFlushRange(buffer, 0x20);
        __ARReadDMA((u32)buffer, ARAM_size + 0x200000, 0x20);
        PPCSync();
        if (buffer[0] == test_data[0]) {
            __AR_ExpansionSize = 0x200000;
            ARAM_size += 0x200000;
        } else {
            memset(buffer, 0, 0x20);
            DCFlushRange(buffer, 0x20);
            __ARReadDMA((u32)buffer, ARAM_size + 0x1000000, 0x20);
            PPCSync();
            if (buffer[0] == test_data[0]) {
                __AR_ExpansionSize = 0x400000;
                ARAM_mode |= 1 << 3;
                ARAM_size += 0x400000;
            } else {
                memset(buffer, 0, 0x20);
                DCFlushRange(buffer, 0x20);
                __ARReadDMA((u32)buffer, ARAM_size + 0x200, 0x20);
                PPCSync();
                if (buffer[0] == test_data[0]) {
                    __AR_ExpansionSize = 0x800000;
                    ARAM_mode |= 2 << 3;
                    ARAM_size += 0x800000;
                } else {
                    memset(buffer, 0, 0x20);
                    DCFlushRange(buffer, 0x20);
                    __ARReadDMA((u32)buffer, ARAM_size + 0x400000, 0x20);
                    PPCSync();
                    if (buffer[0] == test_data[0]) {
                        __AR_ExpansionSize = 0x1000000;
                        ARAM_mode |= 3 << 3;
                        ARAM_size += 0x1000000;
                    } else {
                        __AR_ExpansionSize = 0x2000000;
                        ARAM_mode |= 4 << 3;
                        ARAM_size += 0x2000000;
                    }
                }
            }
        }
        __DSPRegs[9] = (u16)((__DSPRegs[9] & ~0x3F) | ARAM_mode);
    }
    *(u32*)0xC00000D0 = ARAM_size;
    __AR_Size = ARAM_size;
}
