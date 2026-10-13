#include <dolphin/os.h>
#include <dolphin/os_priv.h>

#define __MEMRegs ((volatile u16*)0xCC004000)

#define OS_PHYSICAL_MEM_SIZE (*(u32*)0x80000028)
#define OS_SIMULATED_MEM_SIZE (*(u32*)0x800000F0)

#define OS_INTERRUPTMASK_MEM_0_3 0xF0000000
#define OS_INTERRUPTMASK_MEM_ADDRESS 0x08000000

// SPR numbers for the BAT and save/restore registers.
#define SPR_SRR0 26
#define SPR_SRR1 27
#define SPR_IBAT0U 528
#define SPR_IBAT0L 529
#define SPR_IBAT2U 532
#define SPR_IBAT2L 533
#define SPR_DBAT0U 536
#define SPR_DBAT0L 537
#define SPR_DBAT2U 540
#define SPR_DBAT2L 541

static BOOL OnReset(BOOL final);

static OSResetFunctionInfo ResetFunctionInfo = {
    OnReset,
    127,
};

static BOOL OnReset(BOOL final) {
    if (final != FALSE) {
        __MEMRegs[8] = 0xFF;
        __OSMaskInterrupts(OS_INTERRUPTMASK_MEM_0_3);
    }
    return TRUE;
}

static void MEMIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    u32 addr;
    u32 cause;

    cause = __MEMRegs[0xF];
    addr = (((u32)__MEMRegs[0x12] & 0x3FF) << 16) | __MEMRegs[0x11];
    __MEMRegs[0x10] = 0;

    if (__OSErrorTable[OS_ERROR_PROTECTION]) {
        __OSErrorTable[OS_ERROR_PROTECTION](OS_ERROR_PROTECTION, context, cause, addr);
        return;
    }

    __OSUnhandledException(OS_ERROR_PROTECTION, context, cause, addr);
}

// Remaps BAT0/BAT2 so only the first 24MB of RAM is visible. Runs in real mode
// via RealMode and returns with rfi.
static asm void Config24MB(void) {
    nofralloc

    addi r7, r0, 0

    addis r4, r0, 0x00000002@ha
    addi r4, r4, 0x00000002@l
    addis r3, r0, 0x800001FF@ha
    addi r3, r3, 0x800001FF@l

    addis r6, r0, 0x01000002@ha
    addi r6, r6, 0x01000002@l
    addis r5, r0, 0x810000FF@ha
    addi r5, r5, 0x810000FF@l

    isync

    mtspr SPR_DBAT0U, r7
    mtspr SPR_DBAT0L, r4
    mtspr SPR_DBAT0U, r3
    isync
    mtspr SPR_IBAT0U, r7
    mtspr SPR_IBAT0L, r4
    mtspr SPR_IBAT0U, r3
    isync

    mtspr SPR_DBAT2U, r7
    mtspr SPR_DBAT2L, r6
    mtspr SPR_DBAT2U, r5
    isync
    mtspr SPR_IBAT2U, r7
    mtspr SPR_IBAT2L, r6
    mtspr SPR_IBAT2U, r5
    isync

    mfmsr r3
    ori r3, r3, 0x30
    mtspr SPR_SRR1, r3
    mflr r3
    mtspr SPR_SRR0, r3
    rfi
}

static asm void Config48MB(void) {
    nofralloc

    addi r7, r0, 0

    addis r4, r0, 0x00000002@ha
    addi r4, r4, 0x00000002@l
    addis r3, r0, 0x800003FF@ha
    addi r3, r3, 0x800003FF@l

    addis r6, r0, 0x02000002@ha
    addi r6, r6, 0x02000002@l
    addis r5, r0, 0x820001FF@ha
    addi r5, r5, 0x820001FF@l

    isync

    mtspr SPR_DBAT0U, r7
    mtspr SPR_DBAT0L, r4
    mtspr SPR_DBAT0U, r3
    isync
    mtspr SPR_IBAT0U, r7
    mtspr SPR_IBAT0L, r4
    mtspr SPR_IBAT0U, r3
    isync

    mtspr SPR_DBAT2U, r7
    mtspr SPR_DBAT2L, r6
    mtspr SPR_DBAT2U, r5
    isync
    mtspr SPR_IBAT2U, r7
    mtspr SPR_IBAT2L, r6
    mtspr SPR_IBAT2U, r5
    isync

    mfmsr r3
    ori r3, r3, 0x30
    mtspr SPR_SRR1, r3
    mflr r3
    mtspr SPR_SRR0, r3
    rfi
}

// Jumps to addr (a physical address) with address translation off.
static asm void RealMode(register u32 addr) {
    nofralloc
    clrlwi addr, addr, 2
    mtspr SPR_SRR0, addr
    mfmsr addr
    rlwinm addr, addr, 0, 28, 25
    mtspr SPR_SRR1, addr
    rfi
}

void __OSInitMemoryProtection(void) {
    u32 simulatedSize;
    BOOL enabled;

    simulatedSize = OS_SIMULATED_MEM_SIZE;
    enabled = OSDisableInterrupts();

    if (simulatedSize <= 0x1800000) {
        RealMode((u32)&Config24MB);
    } else if (simulatedSize <= 0x3000000) {
        RealMode((u32)&Config48MB);
    }

    __MEMRegs[16] = 0;
    __MEMRegs[8] = 0xFF;

    __OSMaskInterrupts(OS_INTERRUPTMASK_MEM_0_3);

    __OSSetInterruptHandler(0, MEMIntrruptHandler);
    __OSSetInterruptHandler(1, MEMIntrruptHandler);
    __OSSetInterruptHandler(2, MEMIntrruptHandler);
    __OSSetInterruptHandler(3, MEMIntrruptHandler);
    __OSSetInterruptHandler(4, MEMIntrruptHandler);

    OSRegisterResetFunction(&ResetFunctionInfo);

    if (OS_SIMULATED_MEM_SIZE < OS_PHYSICAL_MEM_SIZE && OS_SIMULATED_MEM_SIZE == 0x1800000) {
        __MEMRegs[20] = 2;
    }

    __OSUnmaskInterrupts(OS_INTERRUPTMASK_MEM_ADDRESS);

    OSRestoreInterrupts(enabled);
}
