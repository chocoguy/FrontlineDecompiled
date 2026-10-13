.include "macros.inc"
.file "targsupp.s"

# 0x80137060..0x80137080 | size: 0x20
.text
.balign 32
# .text:0x0 | 0x80137060 | size: 0x0
.sym TRKAccessFile, global

# .text:0x0 | 0x80137060 | size: 0x8
.fn fn_80137060, global
/* 80137060 001227E0  0F E0 00 00 */	twui r0, 0x0
/* 80137064 001227E4  4E 80 00 20 */	blr
.endfn fn_80137060
# .text:0x8 | 0x80137068 | size: 0x0
.sym TRKOpenFile, global

# .text:0x8 | 0x80137068 | size: 0x8
.fn fn_80137068, global
/* 80137068 001227E8  0F E0 00 00 */	twui r0, 0x0
/* 8013706C 001227EC  4E 80 00 20 */	blr
.endfn fn_80137068
# .text:0x10 | 0x80137070 | size: 0x0
.sym TRKCloseFile, global

# .text:0x10 | 0x80137070 | size: 0x8
.fn fn_80137070, global
/* 80137070 001227F0  0F E0 00 00 */	twui r0, 0x0
/* 80137074 001227F4  4E 80 00 20 */	blr
.endfn fn_80137070
# .text:0x18 | 0x80137078 | size: 0x0
.sym TRKPositionFile, global

# .text:0x18 | 0x80137078 | size: 0x8
.fn fn_80137078, global
/* 80137078 001227F8  0F E0 00 00 */	twui r0, 0x0
/* 8013707C 001227FC  4E 80 00 20 */	blr
.endfn fn_80137078
