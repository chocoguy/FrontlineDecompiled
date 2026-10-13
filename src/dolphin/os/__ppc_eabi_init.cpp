#include <dolphin/os.h>
#include <dolphin/os_priv.h>
#include <dolphin/base/PPCArch.h>

typedef void (*voidfunctionptr)(void);

// Static constructor table, null-terminated (.ctors).
extern voidfunctionptr _ctors[];

extern "C" {

void __init_hardware(void);
void __flush_cache(void* address, unsigned int size);
void __init_user(void);
void _ExitProcess(void);

static void __init_cpp(void);

// Turns on the FPU and paired singles, and sets up the caches. Runs before
// the stack exists, so the return address is kept in r31.
__declspec(section ".init") asm void __init_hardware(void) {
    nofralloc
    mfmsr r0
    ori r0, r0, 0x2000
    mtmsr r0
    mflr r31
    bl __OSPSInit
    bl __OSCacheInit
    mtlr r31
    blr
}

// Writes back the data cache and invalidates the instruction cache over a
// freshly copied code range.
__declspec(section ".init") asm void __flush_cache(void* address, unsigned int size) {
    nofralloc
    lis r5, 0xffff
    ori r5, r5, 0xfff1
    and r5, r5, r3
    subf r3, r5, r3
    add r4, r4, r3
L_80007364:
    dcbst r0, r5
    sync
    icbi r0, r5
    addic r5, r5, 0x8
    subic. r4, r4, 0x8
    bge L_80007364
    isync
    blr
}

void __init_user(void) {
    __init_cpp();
}

static void __init_cpp(void) {
    voidfunctionptr* constructor;

    for (constructor = _ctors; *constructor; constructor++) {
        (*constructor)();
    }
}

void _ExitProcess(void) {
    PPCHalt();
}

}
