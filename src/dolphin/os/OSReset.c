#include <dolphin/os.h>
#include <dolphin/os_priv.h>
#include <dolphin/pad.h>
#include <string.h>

#define __VIRegs ((volatile u16*)0xCC002000)
#define __PIRegs ((volatile u32*)0xCC003000)

typedef struct OSResetQueue {
    OSResetFunctionInfo* head;
    OSResetFunctionInfo* tail;
} OSResetQueue;

static OSResetQueue ResetFunctionQueue;

void OSRegisterResetFunction(OSResetFunctionInfo* info) {
    OSResetFunctionInfo* iter;
    OSResetFunctionInfo* prev;

    // Sorted by priority, ascending; equal priorities keep registration order.
    for (iter = ResetFunctionQueue.head; iter && iter->priority <= info->priority; iter = iter->next) {
    }

    if (iter == NULL) {
        prev = ResetFunctionQueue.tail;
        if (prev == NULL) {
            ResetFunctionQueue.head = info;
        } else {
            prev->next = info;
        }
        info->prev = prev;
        info->next = NULL;
        ResetFunctionQueue.tail = info;
    } else {
        info->next = iter;
        prev = iter->prev;
        iter->prev = info;
        info->prev = prev;
        if (prev == NULL) {
            ResetFunctionQueue.head = info;
        } else {
            prev->next = info;
        }
    }
}

static inline BOOL CallResetFunctions(BOOL final) {
    OSResetFunctionInfo* info;
    s32 err = 0;

    for (info = ResetFunctionQueue.head; info; info = info->next) {
        err |= !info->func(final);
    }
    err |= !__OSSyncSram();
    if (err) {
        return FALSE;
    }
    return TRUE;
}

// Pokes the PI reset register. The code is laid out so that each piece is
// fetched into the instruction cache before it runs (the branches jump ahead
// to prefetch, then back), since memory goes away during the reset.
static asm void Reset(register s32 resetCode) {
    nofralloc
    b L_prefetch0
L_flush:
    mfspr r8, HID0
    ori r8, r8, 0x0008
    mtspr HID0, r8
    isync
    sync
    nop
    b L_wait
L_prefetch0:
    b L_prefetch1
L_wait:
    mftb r5, 268
L_waitloop:
    mftb r6, 268
    subf r7, r5, r6
    cmplwi r7, 0x1124
    blt L_waitloop
    nop
    b L_reset
L_prefetch1:
    b L_prefetch2
L_reset:
    lis r8, 0xCC00
    ori r8, r8, 0x3000
    li r4, 3
    stw r4, 0x24(r8)
    stw resetCode, 0x24(r8)
    nop
    b L_hang
L_prefetch2:
    b L_prefetch3
L_hang:
    nop
    b L_hang
L_prefetch3:
    b L_flush
}

void __OSDoHotReset(u32 resetCode) {
    OSDisableInterrupts();
    __VIRegs[1] = 0;
    ICFlashInvalidate();
    Reset(resetCode * 8);
}

static inline void KillThreads(void) {
    OSThread* thread;
    OSThread* next;

    for (thread = __OSActiveThreadQueue.head; thread; thread = next) {
        next = thread->linkActive.next;
        switch (thread->state) {
        case OS_THREAD_STATE_READY:
        case OS_THREAD_STATE_WAITING:
            OSCancelThread(thread);
            break;
        default:
            break;
        }
    }
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
    BOOL disableRecalibration;

    OSDisableScheduler();
    __OSStopAudioSystem();

    if (reset == OS_RESET_SHUTDOWN) {
        disableRecalibration = __PADDisableRecalibration(TRUE);
    }

    while (!CallResetFunctions(FALSE)) {
    }

    if (reset == OS_RESET_HOTRESET && forceMenu) {
        OSSram* sram;

        sram = __OSLockSram();
        sram->flags |= 0x40;
        __OSUnlockSram(TRUE);
        while (!__OSSyncSram()) {
        }
    }

    OSDisableInterrupts();
    CallResetFunctions(TRUE);
    LCDisable();

    if (reset == OS_RESET_HOTRESET) {
        __OSDoHotReset(resetCode);
    } else if (reset == OS_RESET_RESTART) {
        KillThreads();
        OSEnableScheduler();
        __OSReboot(resetCode, forceMenu);
    }

    // Shutdown: clear the low-memory globals for the next program.
    KillThreads();
    memset((void*)0x80000040, 0, 0xCC - 0x40);
    memset((void*)0x800000D4, 0, 0xE8 - 0xD4);
    memset((void*)0x800000F4, 0, 0xF8 - 0xF4);
    memset((void*)0x80003000, 0, 0xC0);
    memset((void*)0x800030C8, 0, 0xD4 - 0xC8);

    __PADDisableRecalibration(disableRecalibration);
}

u32 OSGetResetCode(void) {
    if (*(volatile u8*)0x800030E2 != 0) {
        return 0x80000000;
    }
    return (__PIRegs[9] & ~7) >> 3;
}
