#include <dolphin/os.h>
#include <string.h>

// Program entry point and early C runtime setup, all in the .init section.

typedef struct __rom_copy_info {
    char* rom;
    char* addr;
    unsigned int size;
} __rom_copy_info;

typedef struct __bss_init_info {
    char* addr;
    unsigned int size;
} __bss_init_info;

// Linker-generated tables, terminated by a zero size.
extern __rom_copy_info _rom_copy_info[];
extern __bss_init_info _bss_init_info[];

extern u8 _stack_addr[];
extern u8 _SDA_BASE_[];
extern u8 _SDA2_BASE_[];

void __init_hardware(void);
void __flush_cache(void* address, unsigned int size);
void __init_user(void);
void DBInit(void);
void InitMetroTRK(void);
int main(int argc, char** argv);
void exit(int status);

static void __init_registers(void);
static void __init_data(void);

// Holding the pad 3 buttons down at boot (debug builds) restarts to the menu.
__declspec(section ".init") static void __check_pad3(void) {
    if ((*(u16*)0x800030E4 & 0x0EEF) == 0x0EEF) {
        OSResetSystem(OS_RESET_RESTART, 0, FALSE);
    }
}

// Sets up registers, clears .bss, starts the debugger stub if the BI2 asks
// for it, relocates argv from the BI2, then runs OSInit, the constructors and
// main.
__declspec(section ".init") __declspec(weak) asm void __start(void) {
    nofralloc
    bl __init_registers
    bl __init_hardware
    li r0, -0x1
    stwu r1, -0x8(r1)
    stw r0, 0x4(r1)
    stw r0, 0x0(r1)
    bl __init_data
    li r0, 0x0
    lis r6, 0x8000
    addi r6, r6, 0x44
    stw r0, 0x0(r6)
    lis r6, 0x8000
    addi r6, r6, 0xf4
    lwz r6, 0x0(r6)
    cmplwi r6, 0x0
    beq L_800071AC
    lwz r7, 0xc(r6)
    li r5, 0x0
    cmplwi r7, 0x2
    beq L_8000719C
    cmplwi r7, 0x3
    bne L_800071AC
    li r5, 0x1
L_8000719C:
    lis r6, InitMetroTRK@ha
    addi r6, r6, InitMetroTRK@l
    mtlr r6
    blrl
L_800071AC:
    lis r6, 0x8000
    addi r6, r6, 0xf4
    lwz r5, 0x0(r6)
    cmplwi r5, 0x0
    beq+ L_8000720C
    lwz r6, 0x8(r5)
    cmplwi r6, 0x0
    beq+ L_8000720C
    add r6, r5, r6
    lwz r14, 0x0(r6)
    cmplwi r14, 0x0
    beq L_8000720C
    addi r15, r6, 0x4
    mtctr r14
L_800071E4:
    addi r6, r6, 0x4
    lwz r7, 0x0(r6)
    add r7, r7, r5
    stw r7, 0x0(r6)
    bdnz L_800071E4
    lis r5, 0x8000
    addi r5, r5, 0x34
    clrrwi r7, r15, 5
    stw r7, 0x0(r5)
    b L_80007214
L_8000720C:
    li r14, 0x0
    li r15, 0x0
L_80007214:
    bl DBInit
    bl OSInit
    lis r4, 0x8000
    addi r4, r4, 0x30e6
    lhz r3, 0x0(r4)
    andi. r5, r3, 0x8000
    beq L_8000723C
    andi. r3, r3, 0x7fff
    cmplwi r3, 0x1
    bne L_80007240
L_8000723C:
    bl __check_pad3
L_80007240:
    bl __init_user
    mr r3, r14
    mr r4, r15
    bl main
    b exit
}

__declspec(section ".init") static asm void __init_registers(void) {
    nofralloc
    lis r1, _stack_addr@h
    ori r1, r1, _stack_addr@l
    lis r2, _SDA2_BASE_@h
    ori r2, r2, _SDA2_BASE_@l
    lis r13, _SDA_BASE_@h
    ori r13, r13, _SDA_BASE_@l
    blr
}

static inline void __copy_rom_section(void* dst, const void* src, unsigned long size) {
    if (size && (dst != src)) {
        memcpy(dst, src, size);
        __flush_cache(dst, size);
    }
}

static inline void __init_bss_section(void* dst, unsigned long size) {
    if (size) {
        memset(dst, 0, size);
    }
}

__declspec(section ".init") static void __init_data(void) {
    __rom_copy_info* dci;
    __bss_init_info* bii;

    dci = _rom_copy_info;
    while (1) {
        if (dci->size == 0) {
            break;
        }
        __copy_rom_section(dci->addr, dci->rom, dci->size);
        dci++;
    }

    bii = _bss_init_info;
    while (1) {
        if (bii->size == 0) {
            break;
        }
        __init_bss_section(bii->addr, bii->size);
        bii++;
    }
}
