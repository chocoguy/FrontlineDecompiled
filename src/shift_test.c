/*
 * Relocation safety check, only built with `configure.py --shift-test`.
 *
 * Linked ahead of every other object so all original code and data land at
 * new addresses. If the game still behaves the same, every pointer in the
 * binary is covered by a relocation and non-matching code can be linked
 * safely. Sizes are deliberately odd so alignment padding changes too.
 */

#pragma force_active on

#define NOP4 nop; nop; nop; nop
#define NOP16 NOP4; NOP4; NOP4; NOP4
#define NOP64 NOP16; NOP16; NOP16; NOP16
#define NOP256 NOP64; NOP64; NOP64; NOP64

/* 0x1004 bytes of .text */
asm void ShiftTest_PadText(void) {
    nofralloc
    NOP256
    NOP256
    NOP256
    NOP256
    blr
}

char ShiftTest_PadData[0x124] = { 1 };
const char ShiftTest_PadRodata[0x88] = { 1 };
char ShiftTest_PadBss[0x456];
int ShiftTest_PadSdata = 1;
int ShiftTest_PadSbss;
const float ShiftTest_PadSdata2 = 1.5f;
