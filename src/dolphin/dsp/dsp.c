#include <dolphin/dsp.h>

#define __DSPRegs ((volatile u16*)0xCC005000)

void __DSPHandler(__OSInterrupt interrupt, OSContext* context);
void __DSP_debug_printf(const char* fmt, ...);
void __DSP_insert_task(DSPTaskInfo* task);
void __DSP_boot_task(DSPTaskInfo* task);

extern DSPTaskInfo* __DSP_tmp_task;
extern DSPTaskInfo* __DSP_last_task;
extern DSPTaskInfo* __DSP_first_task;
extern DSPTaskInfo* __DSP_curr_task;
extern DSPTaskInfo* __DSP_rude_task;
extern BOOL __DSP_rude_task_pending;

static BOOL __DSP_init_flag;

u32 DSPCheckMailToDSP(void) {
    return (__DSPRegs[0] >> 15) & 1;
}

u32 DSPCheckMailFromDSP(void) {
    return (__DSPRegs[2] >> 15) & 1;
}

u32 DSPReadMailFromDSP(void) {
    return (__DSPRegs[2] << 16) | __DSPRegs[3];
}

void DSPSendMailToDSP(u32 mail) {
    __DSPRegs[0] = (u16)(mail >> 16);
    __DSPRegs[1] = (u16)mail;
}

void DSPInit(void) {
    BOOL old;

    __DSP_debug_printf("DSPInit(): Build Date: %s %s\n", "Dec 17 2001", "18:25:00");

    if (__DSP_init_flag == TRUE) {
        return;
    }

    old = OSDisableInterrupts();
    __OSSetInterruptHandler(7, __DSPHandler);
    __OSUnmaskInterrupts(0x01000000);

    __DSPRegs[5] = (u16)((__DSPRegs[5] & ~0xA8) | 0x800);
    __DSPRegs[5] = (u16)(__DSPRegs[5] & ~0xAC);

    __DSP_tmp_task = NULL;
    __DSP_curr_task = NULL;
    __DSP_last_task = NULL;
    __DSP_first_task = NULL;
    __DSP_init_flag = TRUE;
    OSRestoreInterrupts(old);
}

BOOL DSPCheckInit(void) {
    return __DSP_init_flag;
}

void DSPReset(void) {
    BOOL old = OSDisableInterrupts();
    __DSPRegs[5] = (u16)((__DSPRegs[5] & ~0xA8) | 0x801);
    __DSP_init_flag = FALSE;
    OSRestoreInterrupts(old);
}

void DSPHalt(void) {
    BOOL old = OSDisableInterrupts();
    __DSPRegs[5] = (u16)((__DSPRegs[5] & ~0xA8) | 4);
    OSRestoreInterrupts(old);
}

u32 DSPGetDMAStatus(void) {
    return __DSPRegs[5] & 0x200;
}

DSPTaskInfo* DSPAddTask(DSPTaskInfo* task) {
    BOOL old;

    old = OSDisableInterrupts();
    __DSP_insert_task(task);
    task->state = 0;  // DSP_TASK_STATE_INIT
    task->flags = 1;  // DSP_TASK_FLAG_ATTACHED
    OSRestoreInterrupts(old);
    if (task == __DSP_first_task) {
        __DSP_boot_task(task);
    }
    return task;
}

// Makes `task` run next (if it outranks the current task, the current one is yielded).
DSPTaskInfo* DSPAssertTask(DSPTaskInfo* task) {
    BOOL old;

    old = OSDisableInterrupts();
    if (__DSP_curr_task == task) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = TRUE;
        OSRestoreInterrupts(old);
        return task;
    }

    if (task->priority < __DSP_curr_task->priority) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = TRUE;
        if (__DSP_curr_task->state == 1) {  // DSP_TASK_STATE_RUN
            BOOL old2 = OSDisableInterrupts();
            __DSPRegs[5] = (u16)((__DSPRegs[5] & ~0xA8) | 2);
            OSRestoreInterrupts(old2);
        }
        OSRestoreInterrupts(old);
        return task;
    }

    OSRestoreInterrupts(old);
    return NULL;
}
