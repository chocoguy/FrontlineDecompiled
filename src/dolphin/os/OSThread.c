#include <dolphin/os.h>
#include <dolphin/os_priv.h>

#define __OSCurrentThread (*(OSThread**)0x800000E4)
#define __OSFPUContext (*(OSContext**)0x800000D8)

#define OS_CONTEXT_STATE_EXC 0x02u

// Set by the linker: the default (main) thread's stack.
extern u8 _stack_addr[];
extern u8 _stack_end[];

#define AddTail(queue, thread, link)          \
    do {                                      \
        OSThread* __prev;                     \
                                              \
        __prev = (queue)->tail;               \
        if (__prev == NULL) {                 \
            (queue)->head = (thread);         \
        } else {                              \
            __prev->link.next = (thread);     \
        }                                     \
        (thread)->link.prev = __prev;         \
        (thread)->link.next = NULL;           \
        (queue)->tail = (thread);             \
    } while (0)

// Inserts thread after every entry of equal or higher priority.
#define AddPrio(queue, thread, link)                                                          \
    do {                                                                                      \
        OSThread* __prev;                                                                     \
        OSThread* __next;                                                                     \
                                                                                              \
        for (__next = (queue)->head; __next && __next->priority <= (thread)->priority;        \
             __next = __next->link.next) {                                                    \
        }                                                                                     \
        if (__next == NULL) {                                                                 \
            AddTail(queue, thread, link);                                                     \
        } else {                                                                              \
            (thread)->link.next = __next;                                                     \
            __prev = __next->link.prev;                                                       \
            __next->link.prev = (thread);                                                     \
            (thread)->link.prev = __prev;                                                     \
            if (__prev == NULL) {                                                             \
                (queue)->head = (thread);                                                     \
            } else {                                                                          \
                __prev->link.next = (thread);                                                 \
            }                                                                                 \
        }                                                                                     \
    } while (0)

#define RemoveItem(queue, thread, link)       \
    do {                                      \
        OSThread* __next;                     \
        OSThread* __prev;                     \
                                              \
        __next = (thread)->link.next;         \
        __prev = (thread)->link.prev;         \
        if (__next == NULL) {                 \
            (queue)->tail = __prev;           \
        } else {                              \
            __next->link.prev = __prev;       \
        }                                     \
        if (__prev == NULL) {                 \
            (queue)->head = __next;           \
        } else {                              \
            __prev->link.next = __next;       \
        }                                     \
    } while (0)

#define RemoveHead(queue, thread, link)       \
    do {                                      \
        OSThread* __next;                     \
                                              \
        (thread) = (queue)->head;             \
        __next = (thread)->link.next;         \
        if (__next == NULL) {                 \
            (queue)->tail = NULL;             \
        } else {                              \
            __next->link.prev = NULL;         \
        }                                     \
        (queue)->head = __next;               \
    } while (0)

static OSThreadQueue RunQueue[OS_PRIORITY_MAX + 1];
static OSThread IdleThread;
static OSThread DefaultThread;
static OSContext IdleContext;

// Bit (31 - priority) is set while RunQueue[priority] is non-empty.
static volatile u32 RunQueueBits;
static volatile BOOL RunQueueHint;
static volatile s32 Reschedule;

static inline void OSInitMutexQueue(OSMutexQueue* queue) {
    queue->head = queue->tail = NULL;
}

void __OSThreadInit(void) {
    OSThread* thread = &DefaultThread;
    int prio;

    thread->state = OS_THREAD_STATE_RUNNING;
    thread->attr = OS_THREAD_ATTR_DETACH;
    thread->priority = thread->base = 16;
    thread->suspend = 0;
    thread->val = (void*)-1;
    thread->mutex = NULL;
    OSInitThreadQueue(&thread->queueJoin);
    OSInitMutexQueue(&thread->queueMutex);

    __OSFPUContext = &thread->context;

    OSClearContext(&thread->context);
    OSSetCurrentContext(&thread->context);
    thread->stackBase = _stack_addr;
    thread->stackEnd = (u32*)_stack_end;
    *(thread->stackEnd) = OS_THREAD_STACK_MAGIC;

    RunQueueBits = 0;
    __OSCurrentThread = thread;
    RunQueueHint = FALSE;
    for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; ++prio) {
        OSInitThreadQueue(&RunQueue[prio]);
    }

    OSInitThreadQueue(&__OSActiveThreadQueue);
    AddTail(&__OSActiveThreadQueue, thread, linkActive);
    OSClearContext(&IdleContext);
    Reschedule = 0;
}

void OSInitThreadQueue(OSThreadQueue* queue) {
    queue->head = queue->tail = NULL;
}

OSThread* OSGetCurrentThread(void) {
    return __OSCurrentThread;
}

s32 OSDisableScheduler(void) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    count = Reschedule++;
    OSRestoreInterrupts(enabled);
    return count;
}

s32 OSEnableScheduler(void) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    count = Reschedule--;
    OSRestoreInterrupts(enabled);
    return count;
}

static inline void SetRun(OSThread* thread) {
    thread->queue = &RunQueue[thread->priority];
    AddTail(thread->queue, thread, link);
    RunQueueBits |= 1u << (OS_PRIORITY_MAX - thread->priority);
    RunQueueHint = TRUE;
}

#pragma auto_inline off
static void UnsetRun(OSThread* thread) {
    OSThreadQueue* queue;

    queue = thread->queue;
    RemoveItem(queue, thread, link);
    if (queue->head == NULL) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - thread->priority));
    }
    thread->queue = NULL;
}
#pragma auto_inline reset

// A thread's priority is its base priority raised to that of the most urgent
// thread waiting on a mutex it holds.
s32 __OSGetEffectivePriority(OSThread* thread) {
    s32 priority;
    OSMutex* mutex;
    OSThread* blocked;

    priority = thread->base;
    for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
        blocked = mutex->queue.head;
        if (blocked && blocked->priority < priority) {
            priority = blocked->priority;
        }
    }
    return priority;
}

// Returns the owner of the mutex thread waits on, whose priority may now need
// updating too.
static OSThread* SetEffectivePriority(OSThread* thread, s32 priority) {
    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        UnsetRun(thread);
        thread->priority = priority;
        SetRun(thread);
        break;
    case OS_THREAD_STATE_WAITING:
        RemoveItem(thread->queue, thread, link);
        thread->priority = priority;
        AddPrio(thread->queue, thread, link);
        if (thread->mutex) {
            return thread->mutex->thread;
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        thread->priority = priority;
        break;
    }
    return NULL;
}

static inline void UpdatePriority(OSThread* thread) {
    s32 priority;

    do {
        if (0 < thread->suspend) {
            break;
        }
        priority = __OSGetEffectivePriority(thread);
        if (thread->priority == priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
    } while (thread);
}

void __OSPromoteThread(OSThread* thread, s32 priority) {
    do {
        if (0 < thread->suspend || thread->priority <= priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
    } while (thread);
}

static OSThread* SelectThread(BOOL yield) {
    OSContext* currentContext;
    OSThread* currentThread;
    OSThread* nextThread;
    s32 priority;
    OSThreadQueue* queue;

    if (0 < Reschedule) {
        return NULL;
    }

    currentContext = OSGetCurrentContext();
    currentThread = OSGetCurrentThread();
    if (currentContext != &currentThread->context) {
        return NULL;
    }

    if (currentThread) {
        if (currentThread->state == OS_THREAD_STATE_RUNNING) {
            if (!yield) {
                priority = __cntlzw(RunQueueBits);
                if (currentThread->priority <= priority) {
                    return NULL;
                }
            }
            currentThread->state = OS_THREAD_STATE_READY;
            SetRun(currentThread);
        }

        // OSSaveContext returns 1 when this thread is later resumed.
        if (!(currentThread->context.state & OS_CONTEXT_STATE_EXC) && OSSaveContext(&currentThread->context)) {
            return NULL;
        }
    }

    __OSCurrentThread = NULL;
    if (RunQueueBits == 0) {
        // Idle with interrupts on until something becomes runnable.
        OSSetCurrentContext(&IdleContext);
        do {
            OSEnableInterrupts();
            while (RunQueueBits == 0) {
            }
            OSDisableInterrupts();
        } while (RunQueueBits == 0);
        OSClearContext(&IdleContext);
    }

    RunQueueHint = FALSE;

    priority = __cntlzw(RunQueueBits);
    queue = &RunQueue[priority];
    RemoveHead(queue, nextThread, link);
    if (queue->head == NULL) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - priority));
    }
    nextThread->queue = NULL;
    nextThread->state = OS_THREAD_STATE_RUNNING;
    __OSCurrentThread = nextThread;
    OSSetCurrentContext(&nextThread->context);
    OSLoadContext(&nextThread->context);
    return nextThread;
}

void __OSReschedule(void) {
    if (!RunQueueHint) {
        return;
    }
    SelectThread(FALSE);
}

void OSYieldThread(void) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    SelectThread(TRUE);
    OSRestoreInterrupts(enabled);
}

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize, s32 priority,
                    u16 attr) {
    BOOL enabled;
    u32 sp;

    if (priority < OS_PRIORITY_MIN || OS_PRIORITY_MAX < priority) {
        return FALSE;
    }

    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr & OS_THREAD_ATTR_DETACH;
    thread->base = priority;
    thread->priority = priority;
    thread->suspend = 1;
    thread->val = (void*)-1;
    thread->mutex = NULL;
    OSInitThreadQueue(&thread->queueJoin);
    OSInitMutexQueue(&thread->queueMutex);

    // Terminate the back chain with a null frame.
    sp = (u32)stack;
    sp &= ~7;
    sp -= 8;
    ((u32*)sp)[0] = 0;
    ((u32*)sp)[1] = 0;
    OSInitContext(&thread->context, (u32)func, sp);
    thread->context.lr = (u32)OSExitThread;
    thread->context.gpr[3] = (u32)param;
    thread->stackBase = stack;
    thread->stackEnd = (u32*)((u32)stack - stackSize);
    *(thread->stackEnd) = OS_THREAD_STACK_MAGIC;

    enabled = OSDisableInterrupts();
    AddTail(&__OSActiveThreadQueue, thread, linkActive);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

void OSExitThread(void* val) {
    BOOL enabled;
    OSThread* currentThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();
    OSClearContext(&currentThread->context);
    if (currentThread->attr & OS_THREAD_ATTR_DETACH) {
        RemoveItem(&__OSActiveThreadQueue, currentThread, linkActive);
        currentThread->state = 0;
    } else {
        currentThread->state = OS_THREAD_STATE_MORIBUND;
        currentThread->val = val;
    }
    __OSUnlockAllMutex(currentThread);
    OSWakeupThread(&currentThread->queueJoin);
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

void OSCancelThread(OSThread* thread) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        if (!(0 < thread->suspend)) {
            UnsetRun(thread);
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        break;
    case OS_THREAD_STATE_WAITING:
        RemoveItem(thread->queue, thread, link);
        thread->queue = NULL;
        if (!(0 < thread->suspend) && thread->mutex) {
            UpdatePriority(thread->mutex->thread);
        }
        break;
    default:
        OSRestoreInterrupts(enabled);
        return;
    }

    OSClearContext(&thread->context);
    if (thread->attr & OS_THREAD_ATTR_DETACH) {
        RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    } else {
        thread->state = OS_THREAD_STATE_MORIBUND;
    }

    __OSUnlockAllMutex(thread);
    OSWakeupThread(&thread->queueJoin);
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

s32 OSResumeThread(OSThread* thread) {
    BOOL enabled;
    s32 suspendCount;

    enabled = OSDisableInterrupts();
    suspendCount = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_READY:
            thread->priority = __OSGetEffectivePriority(thread);
            SetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            RemoveItem(thread->queue, thread, link);
            thread->priority = __OSGetEffectivePriority(thread);
            AddPrio(thread->queue, thread, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

s32 OSSuspendThread(OSThread* thread) {
    BOOL enabled;
    s32 suspendCount;

    enabled = OSDisableInterrupts();
    suspendCount = thread->suspend++;
    if (suspendCount == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_RUNNING:
            RunQueueHint = TRUE;
            thread->state = OS_THREAD_STATE_READY;
            break;
        case OS_THREAD_STATE_READY:
            UnsetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            RemoveItem(thread->queue, thread, link);
            thread->priority = 32;
            AddTail(thread->queue, thread, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        __OSReschedule();
    }
    OSRestoreInterrupts(enabled);
    return suspendCount;
}

void OSSleepThread(OSThreadQueue* queue) {
    BOOL enabled;
    OSThread* currentThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();

    currentThread->state = OS_THREAD_STATE_WAITING;
    currentThread->queue = queue;
    AddPrio(queue, currentThread, link);
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}

void OSWakeupThread(OSThreadQueue* queue) {
    BOOL enabled;
    OSThread* thread;

    enabled = OSDisableInterrupts();
    while (queue->head) {
        RemoveHead(queue, thread, link);
        thread->state = OS_THREAD_STATE_READY;
        if (!(0 < thread->suspend)) {
            SetRun(thread);
        }
    }
    __OSReschedule();
    OSRestoreInterrupts(enabled);
}
