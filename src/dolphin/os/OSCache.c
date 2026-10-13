#include <dolphin/os.h>
#include <dolphin/base/PPCArch.h>

void DBPrintf(char* format, ...);

asm void DCEnable(void) {
    nofralloc
    sync
    mfspr r3, HID0
    ori r3, r3, 0x4000
    mtspr HID0, r3
    blr
}

asm void DCInvalidateRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbi r0, addr
    addi addr, addr, 0x20
    bdnz loop
    blr
}

// The trailing sc runs the system call vector, which drains the write-gather pipe.
asm void DCFlushRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbf r0, addr
    addi addr, addr, 0x20
    bdnz loop
    sc
    blr
}

asm void DCStoreRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbst r0, addr
    addi addr, addr, 0x20
    bdnz loop
    sc
    blr
}

asm void DCFlushRangeNoSync(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbf r0, addr
    addi addr, addr, 0x20
    bdnz loop
    blr
}

asm void DCStoreRangeNoSync(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    dcbst r0, addr
    addi addr, addr, 0x20
    bdnz loop
    blr
}

asm void ICInvalidateRange(register void* addr, register u32 nBytes) {
    nofralloc
    cmplwi nBytes, 0
    blelr
    clrlwi. r5, addr, 27
    beq aligned
    addi nBytes, nBytes, 0x20
aligned:
    addi nBytes, nBytes, 0x1f
    srwi nBytes, nBytes, 5
    mtctr nBytes
loop:
    icbi r0, addr
    addi addr, addr, 0x20
    bdnz loop
    sync
    isync
    blr
}

asm void ICFlashInvalidate(void) {
    nofralloc
    mfspr r3, HID0
    ori r3, r3, 0x800
    mtspr HID0, r3
    blr
}

asm void ICEnable(void) {
    nofralloc
    isync
    mfspr r3, HID0
    ori r3, r3, 0x8000
    mtspr HID0, r3
    blr
}

// Invalidates the 16KB locked cache region and turns locked cache off.
asm void LCDisable(void) {
    nofralloc
    lis r3, 0xE000
    li r4, 0x200
    mtctr r4
loop:
    dcbi r0, r3
    addi r3, r3, 0x20
    bdnz loop
    mfspr r4, SPR_HID2
    rlwinm r4, r4, 0, 4, 2
    mtspr SPR_HID2, r4
    blr
}

#define L2Disable()                          \
    do {                                     \
        __sync();                            \
        PPCMtl2cr(PPCMfl2cr() & ~L2CR_L2E);  \
        __sync();                            \
    } while (0)

void L2GlobalInvalidate(void) {
    L2Disable();
    PPCMtl2cr(PPCMfl2cr() | L2CR_L2I);
    while (PPCMfl2cr() & L2CR_L2IP) {
    }
    PPCMtl2cr(PPCMfl2cr() & ~L2CR_L2I);
    while (PPCMfl2cr() & L2CR_L2IP) {
        DBPrintf(">>> L2 INVALIDATE : SHOULD NEVER HAPPEN\n");
    }
}

void DMAErrorHandler(OSError error, OSContext* context, ...) {
    u32 hid2;

    hid2 = PPCMfhid2();

    OSReport("Machine check received\n");
    OSReport("HID2 = 0x%x   SRR1 = 0x%x\n", hid2, context->srr1);
    if (!(hid2 & (HID2_DCHERR | HID2_DNCERR | HID2_DCMERR | HID2_DQOERR)) || !(context->srr1 & SRR1_DMA_BIT)) {
        OSReport("Machine check was not DMA/locked cache related\n");
        OSDumpContext(context);
        PPCHalt();
    }

    OSReport("DMAErrorHandler(): An error occurred while processing DMA.\n");
    OSReport("The following errors have been detected and cleared :\n");

    if (hid2 & HID2_DCHERR) {
        OSReport("\t- Requested a locked cache tag that was already in the cache\n");
    }
    if (hid2 & HID2_DNCERR) {
        OSReport("\t- DMA attempted to access normal cache\n");
    }
    if (hid2 & HID2_DCMERR) {
        OSReport("\t- DMA missed in data cache\n");
    }
    if (hid2 & HID2_DQOERR) {
        OSReport("\t- DMA queue overflowed\n");
    }

    // Writing the error bits back clears them.
    PPCMthid2(hid2);
}

#pragma dont_inline on
void __OSCacheInit(void) {
    if (!(PPCMfhid0() & HID0_ICE)) {
        ICEnable();
        DBPrintf("L1 i-caches initialized\n");
    }
    if (!(PPCMfhid0() & HID0_DCE)) {
        DCEnable();
        DBPrintf("L1 d-caches initialized\n");
    }

    if (!(PPCMfl2cr() & L2CR_L2E)) {
        u32 oldMSR;

        oldMSR = PPCMfmsr();
        __sync();
        PPCMtmsr(MSR_IR | MSR_DR);
        __sync();
        L2Disable();
        L2GlobalInvalidate();
        PPCMtmsr(oldMSR);
        PPCMtl2cr((PPCMfl2cr() | L2CR_L2E) & ~L2CR_L2I);
        DBPrintf("L2 cache initialized\n");
    }

    OSSetErrorHandler(OS_ERROR_MACHINE_CHECK, DMAErrorHandler);
    DBPrintf("Locked cache machine check handler installed\n");
}
#pragma dont_inline reset
