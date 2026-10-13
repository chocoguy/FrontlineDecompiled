#include <dolphin/dsp.h>

#define __DSPRegs ((volatile u16*)0xCC005000)

#define DSP_TASK_FLAG_CANCEL 2
#define DSP_TASK_STATE_RUN 1
#define DSP_TASK_STATE_YIELD 2
#define DSP_TASK_STATE_DONE 3

void __DSP_debug_printf(const char* fmt, ...);

BOOL __DSP_rude_task_pending;
DSPTaskInfo* __DSP_rude_task;
DSPTaskInfo* __DSP_tmp_task;
DSPTaskInfo* __DSP_last_task;
DSPTaskInfo* __DSP_first_task;
DSPTaskInfo* __DSP_curr_task;

void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next);
void __DSP_remove_task(DSPTaskInfo* task);

static inline void SendMail(u32 mail) {
    DSPSendMailToDSP(mail);
    while (DSPCheckMailToDSP()) {}
}

// DSP mail interrupt: the task manager microcode reports init/resume/yield/done/request.
void __DSPHandler(__OSInterrupt interrupt, OSContext* context) {
    DSPTaskInfo* tmp_task;
    OSContext exceptionContext;
    u16 tmp;
    u32 mail;

    tmp = __DSPRegs[5];
    tmp = (u16)((tmp & ~0x28) | 0x80);
    __DSPRegs[5] = tmp;

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);

    while (!DSPCheckMailFromDSP()) {}
    mail = DSPReadMailFromDSP();

    if ((__DSP_curr_task->flags & DSP_TASK_FLAG_CANCEL) && (mail == 0xDCD10002)) {
        mail = 0xDCD10003;
    }

    switch (mail) {
    case 0xDCD10000:  // init done
        __DSP_curr_task->state = DSP_TASK_STATE_RUN;
        if (__DSP_curr_task->init_cb) {
            (*(__DSP_curr_task->init_cb))((void*)__DSP_curr_task);
        }
        break;
    case 0xDCD10001:  // resumed
        __DSP_curr_task->state = DSP_TASK_STATE_RUN;
        if (__DSP_curr_task->res_cb) {
            (*(__DSP_curr_task->res_cb))((void*)__DSP_curr_task);
        }
        break;
    case 0xDCD10002:  // yield
        if (__DSP_rude_task_pending) {
            if (__DSP_curr_task == __DSP_rude_task) {
                SendMail(0xCDD10003);
                __DSP_rude_task = NULL;
                __DSP_rude_task_pending = FALSE;
                if (__DSP_curr_task->res_cb) {
                    (*(__DSP_curr_task->res_cb))((void*)__DSP_curr_task);
                }
            } else {
                SendMail(0xCDD10001);
                __DSP_exec_task(__DSP_curr_task, __DSP_rude_task);
                __DSP_curr_task->state = DSP_TASK_STATE_YIELD;
                __DSP_rude_task_pending = FALSE;
                __DSP_curr_task = __DSP_rude_task;
                __DSP_rude_task = NULL;
            }
        } else if (__DSP_curr_task->next == NULL) {
            if (__DSP_curr_task == __DSP_first_task) {
                SendMail(0xCDD10003);
                if (__DSP_curr_task->res_cb) {
                    (*(__DSP_curr_task->res_cb))((void*)__DSP_curr_task);
                }
            } else {
                SendMail(0xCDD10001);
                __DSP_exec_task(__DSP_curr_task, __DSP_first_task);
                __DSP_curr_task->state = DSP_TASK_STATE_YIELD;
                __DSP_curr_task = __DSP_first_task;
            }
        } else {
            SendMail(0xCDD10001);
            __DSP_exec_task(__DSP_curr_task, __DSP_curr_task->next);
            __DSP_curr_task->state = DSP_TASK_STATE_YIELD;
            __DSP_curr_task = __DSP_curr_task->next;
        }
        break;
    case 0xDCD10003:  // done
        if (__DSP_rude_task_pending) {
            if (__DSP_curr_task->done_cb) {
                (*(__DSP_curr_task->done_cb))((void*)__DSP_curr_task);
            }
            SendMail(0xCDD10001);
            __DSP_exec_task(NULL, __DSP_rude_task);
            __DSP_remove_task(__DSP_curr_task);
            __DSP_rude_task_pending = FALSE;
            __DSP_curr_task = __DSP_rude_task;
            __DSP_rude_task = NULL;
        } else if (__DSP_curr_task->next == NULL) {
            if (__DSP_curr_task == __DSP_first_task) {
                if (__DSP_curr_task->done_cb) {
                    (*(__DSP_curr_task->done_cb))((void*)__DSP_curr_task);
                }
                SendMail(0xCDD10002);
                __DSP_curr_task->state = DSP_TASK_STATE_DONE;
                __DSP_remove_task(__DSP_curr_task);
            } else {
                if (__DSP_curr_task->done_cb) {
                    (*(__DSP_curr_task->done_cb))((void*)__DSP_curr_task);
                }
                SendMail(0xCDD10001);
                __DSP_curr_task->state = DSP_TASK_STATE_DONE;
                __DSP_exec_task(NULL, __DSP_first_task);
                __DSP_curr_task = __DSP_first_task;
                __DSP_remove_task(__DSP_last_task);
            }
        } else {
            if (__DSP_curr_task->done_cb) {
                (*(__DSP_curr_task->done_cb))((void*)__DSP_curr_task);
            }
            SendMail(0xCDD10001);
            __DSP_curr_task->state = DSP_TASK_STATE_DONE;
            __DSP_exec_task(NULL, __DSP_curr_task->next);
            __DSP_curr_task = __DSP_curr_task->next;
            __DSP_remove_task(__DSP_curr_task->prev);
        }
        break;
    case 0xDCD10004:  // request
        if (__DSP_curr_task->req_cb) {
            (*(__DSP_curr_task->req_cb))((void*)__DSP_curr_task);
        }
        break;
    }

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

// Saves `curr`'s DRAM (if any) and switches the DSP to `next`.
void __DSP_exec_task(DSPTaskInfo* curr, DSPTaskInfo* next) {
    if (curr) {
        SendMail((u32)curr->dram_mmem_addr);
        SendMail(curr->dram_length);
        SendMail(curr->dram_addr);
    } else {
        SendMail(0);
        SendMail(0);
        SendMail(0);
    }

    SendMail((u32)next->iram_mmem_addr);
    SendMail(next->iram_length);
    SendMail(next->iram_addr);

    if (next->state == 0) {  // DSP_TASK_STATE_INIT
        SendMail(next->dsp_init_vector);
        SendMail(0);
        SendMail(0);
        SendMail(0);
    } else {
        SendMail(next->dsp_resume_vector);
        SendMail((u32)next->dram_mmem_addr);
        SendMail(next->dram_length);
        SendMail(next->dram_addr);
    }
}

void __DSP_boot_task(DSPTaskInfo* task) {
    volatile u32 mail;

    while (!DSPCheckMailFromDSP()) {}
    mail = DSPReadMailFromDSP();

    SendMail(0x80F3A001);
    SendMail((u32)task->iram_mmem_addr);
    SendMail(0x80F3C002);
    SendMail(task->iram_addr & 0xFFFF);
    SendMail(0x80F3A002);
    SendMail(task->iram_length);
    SendMail(0x80F3B002);
    SendMail(0);
    SendMail(0x80F3D001);
    SendMail(task->dsp_init_vector);

    __DSP_debug_printf("DSP is booting task: 0x%08X\n", task);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM MMEM ADDR: 0x%08X\n", task->iram_mmem_addr);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM DSP ADDR : 0x%08X\n", task->iram_addr);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM LENGTH   : 0x%08X\n", task->iram_length);
    __DSP_debug_printf("__DSP_boot_task()  : DRAM MMEM ADDR: 0x%08X\n", task->dram_length);
    __DSP_debug_printf("__DSP_boot_task()  : Start Vector  : 0x%08X\n", task->dsp_init_vector);
}

// Inserts by priority (lower value runs first).
void __DSP_insert_task(DSPTaskInfo* task) {
    DSPTaskInfo* temp;

    if (__DSP_first_task == NULL) {
        __DSP_first_task = __DSP_last_task = __DSP_curr_task = task;
        task->next = task->prev = NULL;
    } else {
        temp = __DSP_first_task;
        while (temp) {
            if (task->priority < temp->priority) {
                task->prev = temp->prev;
                temp->prev = task;
                task->next = temp;
                if (task->prev == NULL) {
                    __DSP_first_task = task;
                } else {
                    (task->prev)->next = task;
                }
                break;
            }
            temp = temp->next;
        }

        if (temp == NULL) {
            __DSP_last_task->next = task;
            task->next = NULL;
            task->prev = __DSP_last_task;
            __DSP_last_task = task;
        }
    }
}

void __DSP_remove_task(DSPTaskInfo* task) {
    task->flags = 0;  // DSP_TASK_FLAG_CLEARALL
    task->state = DSP_TASK_STATE_DONE;

    if (__DSP_first_task == task) {
        if (task->next) {
            __DSP_first_task = task->next;
            task->next->prev = NULL;
        } else {
            __DSP_first_task = __DSP_last_task = __DSP_curr_task = NULL;
        }
    } else if (__DSP_last_task == task) {
        __DSP_last_task = task->prev;
        task->prev->next = NULL;
        __DSP_curr_task = __DSP_first_task;
    } else {
        __DSP_curr_task = task->next;
        task->prev->next = task->next;
        task->next->prev = task->prev;
    }
}
