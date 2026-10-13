#include <dolphin/os.h>
#include <string.h>

#define __DSPRegs ((volatile u16*)0xCC005000)
#define __DSPWorkBuffer ((void*)0x81000000)

// Tiny DSP program run at boot to bring the DSP to a known state.
static u8 DSPInitCode[128] = {
#include "dolphin/os/DSPInitCode.inc"
};

void __OSInitAudioSystem(void) {
    u16 reg16;
    u32 start_tick;

    // The init program is DMAed from 0x81000000; save what lives there.
    memcpy((u8*)OSGetArenaHi() - 128, __DSPWorkBuffer, 128);
    memcpy(__DSPWorkBuffer, DSPInitCode, 128);
    DCFlushRange(__DSPWorkBuffer, 128);

    __DSPRegs[9] = 0x43;
    __DSPRegs[5] = 0x8AC;
    __DSPRegs[5] |= 1;
    while (__DSPRegs[5] & 1) {
    }

    __DSPRegs[0] = 0;
    while (((__DSPRegs[2] << 16) | __DSPRegs[3]) & 0x80000000) {
    }

    // ARAM DMA: 0x20 bytes of main memory 0x01000000 to ARAM 0.
    *(volatile u32*)&__DSPRegs[16] = 0x1000000;
    *(volatile u32*)&__DSPRegs[18] = 0;
    *(volatile u32*)&__DSPRegs[20] = 0x20;

    reg16 = __DSPRegs[5];
    while (!(reg16 & 0x20)) {
        reg16 = __DSPRegs[5];
    }
    __DSPRegs[5] = reg16;

    start_tick = OSGetTick();
    while ((s32)(OSGetTick() - start_tick) < 0x892) {
    }

    *(volatile u32*)&__DSPRegs[16] = 0x1000000;
    *(volatile u32*)&__DSPRegs[18] = 0;
    *(volatile u32*)&__DSPRegs[20] = 0x20;

    reg16 = __DSPRegs[5];
    while (!(reg16 & 0x20)) {
        reg16 = __DSPRegs[5];
    }
    __DSPRegs[5] = reg16;

    __DSPRegs[5] &= ~0x800;
    while (__DSPRegs[5] & 0x400) {
    }
    __DSPRegs[5] &= ~4;

    // Wait for the DSP's first mail, then discard it.
    reg16 = __DSPRegs[2];
    while (!(reg16 & 0x8000)) {
        reg16 = __DSPRegs[2];
    }
    (void)__DSPRegs[3];

    __DSPRegs[5] |= 4;
    __DSPRegs[5] = 0x8AC;
    __DSPRegs[5] |= 1;
    while (__DSPRegs[5] & 1) {
    }

    memcpy(__DSPWorkBuffer, (u8*)OSGetArenaHi() - 128, 128);
}

void __OSStopAudioSystem(void) {
    u16 reg16;
    u32 start_tick;

    __DSPRegs[5] = 0x804;
    __DSPRegs[27] &= ~0x8000;  // stop audio DMA

    reg16 = __DSPRegs[5];
    while (reg16 & 0x400) {
        reg16 = __DSPRegs[5];
    }

    reg16 = __DSPRegs[5];
    while (reg16 & 0x200) {
        reg16 = __DSPRegs[5];
    }

    __DSPRegs[5] = 0x8AC;
    __DSPRegs[0] = 0;
    while (((__DSPRegs[2] << 16) | __DSPRegs[3]) & 0x80000000) {
    }

    start_tick = OSGetTick();
    while ((s32)(OSGetTick() - start_tick) < 0x2C) {
    }

    reg16 = __DSPRegs[5];
    __DSPRegs[5] = reg16 | 1;
    reg16 = __DSPRegs[5];
    while (reg16 & 1) {
        reg16 = __DSPRegs[5];
    }
}
