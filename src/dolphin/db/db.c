#include <dolphin/os.h>

void OSDumpContext(OSContext* context);
void PPCHalt(void);

// Debugger interface block at 0x80000040 (the debug monitor's exception mask is at +4).
typedef struct DBInterface {
    /* 0x00 */ u32 bPresent;
    /* 0x04 */ u32 exceptionMask;
    /* 0x08 */ void (*ExceptionDestination)(void);
    /* 0x0C */ void* exceptionReturn;
} DBInterface;

DBInterface* __DBInterface;
BOOL DBVerbose;

void __DBExceptionDestination(void);

void DBInit(void) {
    __DBInterface = (DBInterface*)0x80000040;
    __DBInterface->ExceptionDestination = (void (*)(void))((u32)__DBExceptionDestination + 0x80000000);
    DBVerbose = TRUE;
}

void __DBExceptionDestinationAux(void) {
    u32* contextAddr = (u32*)0xC0;
    OSContext* context = (OSContext*)(0x80000000 + *contextAddr);

    OSReport("DBExceptionDestination\n");
    OSDumpContext(context);
    PPCHalt();
}

// Entered with address translation off: turn it back on, then report.
asm void __DBExceptionDestination(void) {
    nofralloc
    mfmsr r3
    ori r3, r3, 0x30
    mtmsr r3
    b __DBExceptionDestinationAux
}

BOOL __DBIsExceptionMarked(__OSInterrupt exception) {
    u32 mask = 1 << (u8)exception;
    return __DBInterface->exceptionMask & mask;
}

void DBPrintf(char* format, ...) {}
