#include <TRK_MINNOW_DOLPHIN/trk.h>

// Writes back and invalidates the data cache and invalidates the instruction
// cache over [address, address + size).
asm void TRK_flush_cache(register void* address, register u32 size) {
    nofralloc
    lis r5, 0xffff
    ori r5, r5, 0xfff1
    and r5, r5, r3
    subf r3, r5, r3
    add r4, r4, r3
L_801356EC:
    dcbst r0, r5
    dcbf r0, r5
    sync
    icbi r0, r5
    addic r5, r5, 0x8
    subic. r4, r4, 0x8
    bge L_801356EC
    isync
    blr
}
