#include <dolphin/ax.h>
#include <dolphin/os.h>

// Voices by priority; stack 0 holds the free voices.
static AXVPB* __AXStackHead[AX_PRIORITY_STACKS];
static AXVPB* __AXStackTail[AX_PRIORITY_STACKS];
static AXVPB* __AXCallbackStack;

AXVPB* __AXGetStackHead(u32 priority) {
    return __AXStackHead[priority];
}

void __AXServiceCallbackStack(void) {
    AXVPB* p;
    for (p = __AXPopCallbackStack(); p != NULL; p = __AXPopCallbackStack()) {
        BOOL old;
        if (p->callback != NULL) {
            p->callback(p);
        }
        old = OSDisableInterrupts();
        __AXRemoveFromStack(p);
        __AXPushFreeStack(p);
        OSRestoreInterrupts(old);
    }
}

static void __AXInitVoiceStacks(void) {
    u32 i;
    __AXCallbackStack = NULL;
    for (i = 0; i < AX_PRIORITY_STACKS; i++) {
        __AXStackTail[i] = NULL;
        __AXStackHead[i] = NULL;
    }
}

void __AXAllocInit(void) {
    __AXInitVoiceStacks();
}

void __AXAllocQuit(void) {
    __AXInitVoiceStacks();
}

void __AXPushFreeStack(AXVPB* p) {
    p->next = __AXStackHead[AX_PRIORITY_FREE];
    __AXStackHead[AX_PRIORITY_FREE] = p;
    p->priority = AX_PRIORITY_FREE;
}

void __AXPushCallbackStack(AXVPB* p) {
    p->next1 = __AXCallbackStack;
    __AXCallbackStack = p;
}

AXVPB* __AXPopCallbackStack(void) {
    AXVPB* p = __AXCallbackStack;
    if (p != NULL) {
        __AXCallbackStack = (AXVPB*)p->next1;
    }
    return p;
}

void __AXRemoveFromStack(AXVPB* p) {
    u32 priority = p->priority;
    AXVPB* head = __AXStackHead[priority];
    AXVPB* tail = __AXStackTail[priority];
    if (head == tail) {
        __AXStackTail[priority] = NULL;
        __AXStackHead[priority] = NULL;
        return;
    }
    if (p == head) {
        __AXStackHead[priority] = (AXVPB*)p->next;
        ((AXVPB*)__AXStackHead[priority])->prev = NULL;
        return;
    }
    if (p == tail) {
        __AXStackTail[priority] = (AXVPB*)p->prev;
        ((AXVPB*)__AXStackTail[priority])->next = NULL;
        return;
    }
    ((AXVPB*)p->prev)->next = p->next;
    ((AXVPB*)p->next)->prev = p->prev;
}

static inline AXVPB* __AXPopFreeStack(void) {
    AXVPB* p = __AXStackHead[AX_PRIORITY_FREE];
    if (p != NULL) {
        __AXStackHead[AX_PRIORITY_FREE] = (AXVPB*)p->next;
    }
    return p;
}

static inline AXVPB* __AXPopStackFromBottom(u32 priority) {
    AXVPB* p = NULL;
    if (__AXStackHead[priority] != NULL) {
        if (__AXStackHead[priority] == __AXStackTail[priority]) {
            p = __AXStackHead[priority];
            __AXStackTail[priority] = NULL;
            __AXStackHead[priority] = NULL;
        } else if (__AXStackTail[priority] != NULL) {
            p = __AXStackTail[priority];
            __AXStackTail[priority] = (AXVPB*)p->prev;
            ((AXVPB*)__AXStackTail[priority])->next = NULL;
        }
    }
    return p;
}

static inline void __AXPushStackHead(AXVPB* p, u32 priority) {
    p->next = __AXStackHead[priority];
    p->prev = NULL;
    if (p->next != NULL) {
        __AXStackHead[priority]->prev = p;
        __AXStackHead[priority] = p;
    } else {
        __AXStackTail[priority] = p;
        __AXStackHead[priority] = p;
    }
    p->priority = priority;
}

void AXFreeVoice(AXVPB* p) {
    BOOL old = OSDisableInterrupts();
    __AXRemoveFromStack(p);
    if (p->pb.state == AX_PB_STATE_RUN) {
        p->depop = TRUE;
    }
    __AXSetPBDefault(p);
    __AXPushFreeStack(p);
    OSRestoreInterrupts(old);
}

// Takes a free voice, or steals the oldest voice of a lower priority.
AXVPB* AXAcquireVoice(u32 priority, AXVoiceCallback callback, u32 userContext) {
    BOOL old = OSDisableInterrupts();
    AXVPB* p = __AXPopFreeStack();
    if (p == NULL) {
        u32 i;
        for (i = 1; i < priority; i++) {
            p = __AXPopStackFromBottom(i);
            if (p != NULL) {
                if (p->pb.state == AX_PB_STATE_RUN) {
                    p->depop = TRUE;
                }
                if (p->callback != NULL) {
                    p->callback(p);
                }
                break;
            }
        }
    }
    if (p != NULL) {
        __AXPushStackHead(p, priority);
        p->callback = callback;
        p->userContext = userContext;
        __AXSetPBDefault(p);
    }
    OSRestoreInterrupts(old);
    return p;
}
