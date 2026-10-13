#include <dolphin/os.h>

// Host-debugger (USB "ODEMU" / GDEV) link over EXI channel 2: a mailbox for
// control words plus a shared buffer window read/written in 4-byte transfers.

#define __EXIRegs ((volatile u32*)0xCC006800)
#define __PIRegs ((volatile u32*)0xCC003000)

// EXI channel 2 registers
#define EXI2_CPR 10
#define EXI2_CR 13
#define EXI2_DATA 14

#define IS_FALSE(x) ((x) == FALSE)
#define ROUND_UP(x, align) (((x) + (align) - 1) & (-(align)))

typedef void (*MTRCallbackType)(int);

static MTRCallbackType MTRCallback;
static void (*DBGCallback)(u32, OSContext*);
static u32 SendMailData;
static s32 RecvDataLeng;
static u8* pEXIInputFlag;
static u8 EXIInputFlag;
static u8 SendCount = 0x80;

#define DBGEXISelect(v) (__EXIRegs[EXI2_CPR] = (__EXIRegs[EXI2_CPR] & 0x405) | (v))
#define DBGEXIDeselect() (__EXIRegs[EXI2_CPR] &= 0x405)
#define DBGEXISync()                    \
    do {                                \
    } while (__EXIRegs[EXI2_CR] & 1)

// One immediate transfer of up to 4 bytes.
static BOOL DBGEXIImm(void* buffer, s32 bytecounter, u32 write) {
    u8* tempPointer;
    u32 writeOutValue;
    int i;

    if (write) {
        tempPointer = buffer;
        writeOutValue = 0;
        for (i = 0; i < bytecounter; i++) {
            u8* temp = ((u8*)buffer) + i;
            writeOutValue |= *temp << ((3 - i) << 3);
        }
        __EXIRegs[EXI2_DATA] = writeOutValue;
    }

    __EXIRegs[EXI2_CR] = 1 | write << 2 | (bytecounter - 1) << 4;
    DBGEXISync();

    if (!write) {
        writeOutValue = __EXIRegs[EXI2_DATA];
        tempPointer = buffer;
        for (i = 0; i < bytecounter; i++) {
            *tempPointer++ = writeOutValue >> ((3 - i) << 3);
        }
    }

    return TRUE;
}

static BOOL DBGReadMailbox(u32* p1) {
    BOOL total = FALSE;
    u32 v;

    DBGEXISelect(0xC0);

    v = 0x60000000;
    total |= IS_FALSE(DBGEXIImm(&v, 2, 1));
    DBGEXISync();

    total |= IS_FALSE(DBGEXIImm(p1, 4, 0));
    DBGEXISync();

    DBGEXIDeselect();
    return IS_FALSE(total);
}

static BOOL DBGRead(u32 count, u32* buffer, s32 param3) {
    BOOL total = FALSE;
    u32* buf_p = (u32*)buffer;
    u32 v1;
    u32 v;

    DBGEXISelect(0xC0);

    v1 = (count & 0x1FFFC) << 8 | 0x20000000;
    total |= IS_FALSE(DBGEXIImm(&v1, 4, 1));
    DBGEXISync();

    while (param3) {
        total |= IS_FALSE(DBGEXIImm(&v, 4, 0));
        DBGEXISync();

        *buf_p++ = v;

        param3 -= 4;
        if (param3 < 0) {
            param3 = 0;
        }
    }

    DBGEXIDeselect();
    return IS_FALSE(total);
}

static BOOL DBGWrite(u32 count, void* buffer, s32 param3) {
    BOOL total = FALSE;
    u32* buf_p = (u32*)buffer;
    u32 v1;
    u32 v;

    DBGEXISelect(0xC0);

    v1 = (count & 0x1FFFC) << 8 | 0xA0000000;
    total |= IS_FALSE(DBGEXIImm(&v1, 4, 1));
    DBGEXISync();

    while (param3) {
        v = *buf_p++;

        total |= IS_FALSE(DBGEXIImm(&v, 4, 1));
        DBGEXISync();

        param3 -= 4;
        if (param3 < 0) {
            param3 = 0;
        }
    }

    DBGEXIDeselect();
    return IS_FALSE(total);
}

static BOOL DBGReadStatus(u32* p1) {
    BOOL total = FALSE;
    u32 v;

    DBGEXISelect(0xC0);

    v = 1 << 30;
    total |= IS_FALSE(DBGEXIImm(&v, 2, 1));
    DBGEXISync();

    total |= IS_FALSE(DBGEXIImm(p1, 4, 0));
    DBGEXISync();

    DBGEXIDeselect();
    return IS_FALSE(total);
}

static void MWCallback(u32 a, OSContext* b) {
    EXIInputFlag = TRUE;
    if (MTRCallback) {
        MTRCallback(0);
    }
}

static void DBGHandler(s16 a, OSContext* b) {
    __PIRegs[0] = 0x1000;
    if (DBGCallback) {
        DBGCallback(a, b);
    }
}

void DBInitComm(u8** a, MTRCallbackType b) {
    BOOL interrupts = OSDisableInterrupts();

    pEXIInputFlag = (u8*)&EXIInputFlag;
    *a = pEXIInputFlag;
    MTRCallback = b;

    __OSMaskInterrupts(0x18000);
    __EXIRegs[EXI2_CPR] = 0;

    OSRestoreInterrupts(interrupts);
}

void DBInitInterrupts(void) {
    __OSMaskInterrupts(0x18000);
    __OSMaskInterrupts(0x40);
    DBGCallback = &MWCallback;
    __OSSetInterruptHandler(0x19, (__OSInterruptHandler)DBGHandler);
    __OSUnmaskInterrupts(0x40);
}

static void CheckMailBox(void) {
    u32 v;

    DBGReadStatus(&v);
    if (v & 1) {
        DBGReadMailbox(&v);
        v &= 0x1FFFFFFF;

        if ((v & 0x1F000000) == 0x1F000000) {
            SendMailData = v;
            RecvDataLeng = v & 0x7FFF;
            EXIInputFlag = 1;
        }
    }
}

u32 DBQueryData(void) {
    BOOL interrupts;

    EXIInputFlag = 0;
    if (!RecvDataLeng) {
        interrupts = OSDisableInterrupts();
        CheckMailBox();
        OSRestoreInterrupts(interrupts);
    }

    return RecvDataLeng;
}

BOOL DBRead(u32* buffer, s32 count) {
    u32 interrupts = OSDisableInterrupts();
    u32 v = SendMailData & 0x10000 ? 0x1000 : 0;

    DBGRead(v + 0x1E000, buffer, ROUND_UP(count, 4));

    RecvDataLeng = 0;
    EXIInputFlag = 0;

    OSRestoreInterrupts(interrupts);
    return 0;
}

static BOOL DBGWriteMailbox(u32 p1) {
    BOOL total = FALSE;
    u32 v;

    DBGEXISelect(0xC0);

    v = (p1 & 0x1FFFFFFF) | 0xC0000000;
    total |= IS_FALSE(DBGEXIImm(&v, sizeof(v), 1));
    DBGEXISync();

    DBGEXIDeselect();
    return IS_FALSE(total);
}

BOOL DBWrite(void* src, u32 size) {
    u32 v;
    u32 busyFlag;
    BOOL interrupts = OSDisableInterrupts();

    do {
        DBGReadStatus(&busyFlag);
    } while (busyFlag & 2);

    SendCount++;
    v = ((SendCount & 1) ? 0x1000 : 0);

    while (!DBGWrite(v | 0x1C000, src, ROUND_UP(size, 4))) {
    }

    do {
        DBGReadStatus(&busyFlag);
    } while (busyFlag & 2);

    v = SendCount;
    while (!DBGWriteMailbox((0x1F000000) | v << 0x10 | size)) {
    }

    do {
        while (!DBGReadStatus(&busyFlag)) {
        }
    } while (busyFlag & 2);

    OSRestoreInterrupts(interrupts);
    return 0;
}

void DBOpen(void) {}

void DBClose(void) {}
