#include <TRK_MINNOW_DOLPHIN/trk.h>
#include <dolphin/os.h>

#define HARDWARE_GDEV 1

#define kUARTNoData 4

typedef void (*TRKCommCallback)(__OSInterrupt interrupt, OSContext* context);

// Driver for whichever debugger link hardware is present.
typedef struct DBCommTable {
    int (*initialize_func)(volatile u8** inputPendingPtrRef, TRKCommCallback cb);
    void (*init_interrupts_func)(void);
    int (*peek_func)(void);
    int (*read_func)(void* buf, u32 length);
    int (*write_func)(const void* buf, u32 length);
    int (*open_func)(void);
    int (*close_func)(void);
} DBCommTable;

// GDEV (development hardware) and AMC DDH (EXI2 debug adapter) drivers.
int Hu_IsStub(void);
int AMC_IsStub(void);
int DBInitComm(volatile u8** inputPendingPtrRef, TRKCommCallback cb);
void DBInitInterrupts(void);
int DBQueryData(void);
int DBRead(void* buf, u32 length);
int DBWrite(const void* buf, u32 length);
int DBOpen(void);
int DBClose(void);
int EXI2_Init(volatile u8** inputPendingPtrRef, TRKCommCallback cb);
void EXI2_EnableInterrupts(void);
int EXI2_Poll(void);
int EXI2_ReadN(void* buf, u32 length);
int EXI2_WriteN(const void* buf, u32 length);
int EXI2_Reserve(void);
int EXI2_Unreserve(void);

void TRKInterruptHandler(void);
asm void TRKLoadContext(register OSContext* ctx, register u32 exceptionID);

DBCommTable gDBCommTable = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static int gWritePos;
static int gReadPos;
static int gReadCount;
BOOL _MetroTRK_Has_Framing;
static u8 gReadBuf[0x110A];
static u8 gWriteBuf[0x110A];

void TRKEXICallBack(__OSInterrupt interrupt, OSContext* context) {
    OSEnableScheduler();
    TRKLoadContext(context, 0x500);
}

// Returns non-zero if the selected driver is only a stub (no debugger).
int InitMetroTRKCommTable(int hwId) {
    int isStub;

    if (hwId == HARDWARE_GDEV) {
        OSReport("MetroTRK : Set to GDEV hardware\n");
        isStub = Hu_IsStub();
        gDBCommTable.initialize_func = DBInitComm;
        gDBCommTable.init_interrupts_func = DBInitInterrupts;
        gDBCommTable.peek_func = DBQueryData;
        gDBCommTable.read_func = DBRead;
        gDBCommTable.write_func = DBWrite;
        gDBCommTable.open_func = DBOpen;
        gDBCommTable.close_func = DBClose;
    } else {
        OSReport("MetroTRK : Set to AMC DDH hardware\n");
        isStub = AMC_IsStub();
        gDBCommTable.initialize_func = EXI2_Init;
        gDBCommTable.init_interrupts_func = EXI2_EnableInterrupts;
        gDBCommTable.peek_func = EXI2_Poll;
        gDBCommTable.read_func = EXI2_ReadN;
        gDBCommTable.write_func = EXI2_WriteN;
        gDBCommTable.open_func = EXI2_Reserve;
        gDBCommTable.close_func = EXI2_Unreserve;
    }

    return isStub;
}

DSError TRKInitializeIntDrivenUART(u32 param_0, u32 param_1, u32 param_2, volatile u8** param_3) {
    gDBCommTable.initialize_func(param_3, TRKEXICallBack);
    return kNoError;
}

void EnableEXI2Interrupts(void) {
    gDBCommTable.init_interrupts_func();
}

// Sends the write buffer as one 0x800-byte block (zero padded).
int WriteUARTFlush(void) {
    int ret;

    ret = 0;
    while (gWritePos < 0x800) {
        gWriteBuf[gWritePos] = 0;
        gWritePos++;
    }

    if (gWritePos != 0) {
        ret = (gDBCommTable.write_func(gWriteBuf, gWritePos) == 0) ? 0 : -1;
        gWritePos = 0;
    }

    return ret;
}

int WriteUART1(s8 c) {
    gWriteBuf[gWritePos++] = c;
    return 0;
}

int TRKReadUARTPoll(u8* c) {
    int ret;

    ret = kUARTNoData;
    if (gReadPos >= gReadCount) {
        gReadPos = 0;
        gReadCount = gDBCommTable.peek_func();
        if (gReadCount > 0) {
            if (gReadCount > 0x110A) {
                gReadCount = 0x110A;
            }
            ret = (gDBCommTable.read_func(gReadBuf, gReadCount) == 0) ? 0 : -1;
            if (ret != 0) {
                gReadCount = 0;
            }
        }
    }

    if (gReadPos < gReadCount) {
        *c = gReadBuf[gReadPos++];
        ret = 0;
    }

    return ret;
}

void ReserveEXI2Port(void) {
    gDBCommTable.open_func();
}

void UnreserveEXI2Port(void) {
    gDBCommTable.close_func();
}

void TRK_board_display(char* str) {
    OSReport(str);
}

void TRKUARTInterruptHandler(void) {}

// Restores the interrupted context's registers (r2 and r3/r4 saved in SPRGs
// for the handler) and enters TRKInterruptHandler with the exception ID in r3.
asm void TRKLoadContext(register OSContext* ctx, register u32 exceptionID) {
    nofralloc
    lwz r0, 0x0(r3)
    lwz r1, 0x4(r3)
    lwz r2, 0x8(r3)
    lhz r5, 0x1a2(r3)
    rlwinm. r6, r5, 0, 30, 30
    beq L_801375A4
    rlwinm r5, r5, 0, 31, 29
    sth r5, 0x1a2(r3)
    lmw r5, 0x14(r3)
    b L_801375A8
L_801375A4:
    lmw r13, 0x34(r3)
L_801375A8:
    mr r31, r3
    mr r3, r4
    lwz r4, 0x80(r31)
    mtcrf 255, r4
    lwz r4, 0x84(r31)
    mtlr r4
    lwz r4, 0x88(r31)
    mtctr r4
    lwz r4, 0x8c(r31)
    mtxer r4
    mfmsr r4
    rlwinm r4, r4, 0, 17, 15
    rlwinm r4, r4, 0, 31, 29
    mtmsr r4
    mtspr 273, r2
    lwz r4, 0xc(r31)
    mtspr 274, r4
    lwz r4, 0x10(r31)
    mtspr 275, r4
    lwz r2, 0x198(r31)
    lwz r4, 0x19c(r31)
    lwz r31, 0x7c(r31)
    b TRKInterruptHandler
}
