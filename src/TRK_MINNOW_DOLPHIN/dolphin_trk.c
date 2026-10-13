#include <TRK_MINNOW_DOLPHIN/trk.h>

// Exception vector offsets the debugger hooks.
static u32 TRK_ISR_OFFSETS[15] = {
    0x00000100, 0x00000200, 0x00000300, 0x00000400, 0x00000500, 0x00000600, 0x00000700, 0x00000800,
    0x00000900, 0x00000C00, 0x00000D00, 0x00000F00, 0x00001300, 0x00001400, 0x00001700,
};

// Base of the locked cache, when the debugger maps it with DBAT3.
static u32 lc_base;

extern u8 _db_stack_addr[];

// Copies the monitor's handlers over the exception vectors selected by the
// mask at 0x44. Runs from .init before the rest of the image is in place.
__declspec(section ".init") void __TRK_reset(void) {
    u32 mask;
    int i;
    u32* isrOffsetPtr;
    u32* maskPtr;
    void* destPtr;

    if (lc_base <= 0x44 && lc_base + 0x4000 > 0x44 && (gTRKCPUState.Extended1.DBAT3U & 3)) {
        maskPtr = (u32*)0x44;
    } else {
        maskPtr = (u32*)0x80000044;
    }

    mask = *maskPtr;
    isrOffsetPtr = TRK_ISR_OFFSETS;
    for (i = 0; i <= 14; i++, isrOffsetPtr++) {
        if (mask & (1 << i)) {
            destPtr = (void*)TRKTargetTranslate(*isrOffsetPtr);
            TRK_memcpy(destPtr, gTRKInterruptVectorTable + *isrOffsetPtr, 0x100);
            TRK_flush_cache(destPtr, 0x100);
        }
    }
}

// Entry point from __start when a debugger is attached: saves the caller's
// state, switches to the monitor stack and runs TRK_main (or returns if the
// communications driver is not present).
asm void InitMetroTRK(void) {
    nofralloc
    subi r1, r1, 0x4
    stw r3, 0x0(r1)
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    stmw r0, 0x0(r3)
    lwz r4, 0x0(r1)
    addi r1, r1, 0x4
    stw r1, 0x4(r3)
    stw r4, 0xc(r3)
    mflr r4
    stw r4, 0x84(r3)
    stw r4, 0x80(r3)
    mfcr r4
    stw r4, 0x88(r3)
    mfmsr r4
    ori r3, r4, 0x8000
    xori r3, r3, 0x8000
    mtmsr r3
    mtspr 27, r4
    bl TRKSaveExtended1Block
    lis r3, gTRKCPUState@h
    ori r3, r3, gTRKCPUState@l
    opword 0xB8030000
    li r0, 0x0
    mtspr 1010, r0
    mtspr 1013, r0
    lis r1, _db_stack_addr@h
    ori r1, r1, _db_stack_addr@l
    mr r3, r5
    bl InitMetroTRKCommTable
    cmpwi r3, 0x1
    bne L_80137110
    lwz r4, 0x84(r3)
    mtlr r4
    opword 0xB8030000
    blr
L_80137110:
    b TRK_main
}

DSError TRKInitializeTarget(void) {
    gTRKState.isStopped = TRUE;
    gTRKState.MSR = __TRK_get_MSR();
    lc_base = 0xE0000000;
    return kNoError;
}

// Maps an address to its cached alias unless it lies in the locked cache.
u32 TRKTargetTranslate(u32 addr) {
    if (addr >= lc_base) {
        if (addr < lc_base + 0x4000 && (gTRKCPUState.Extended1.DBAT3U & 3)) {
            return addr;
        }
    }
    return (addr & 0x3FFFFFFF) | 0x80000000;
}

void EnableMetroTRKInterrupts(void) {
    EnableEXI2Interrupts();
}
