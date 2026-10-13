#include <dolphin/ar.h>
#include <dolphin/os.h>

static ARQRequest* __ARQRequestQueueHi;
static ARQRequest* __ARQRequestTailHi;
static ARQRequest* __ARQRequestQueueLo;
static ARQRequest* __ARQRequestTailLo;
static ARQRequest* __ARQRequestQueueTemp;
static ARQRequest* __ARQRequestTailTemp;
static ARQRequest* __ARQRequestPendingHi;
static ARQRequest* __ARQRequestPendingLo;
static ARQCallback __ARQCallbackHi;
static ARQCallback __ARQCallbackLo;
static u32 __ARQChunkSize;
static volatile BOOL __ARQ_init_flag;

// Low priority requests are moved a chunk at a time between high priority ones.
void __ARQServiceQueueLo(void) {
    if (__ARQRequestPendingLo == NULL && __ARQRequestQueueLo != NULL) {
        __ARQRequestPendingLo = __ARQRequestQueueLo;
        __ARQRequestQueueLo = __ARQRequestQueueLo->next;
    }
    if (__ARQRequestPendingLo != NULL) {
        if (__ARQRequestPendingLo->length <= __ARQChunkSize) {
            if (__ARQRequestPendingLo->type == ARQ_TYPE_MRAM_TO_ARAM) {
                ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest,
                           __ARQRequestPendingLo->length);
            } else {
                ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source,
                           __ARQRequestPendingLo->length);
            }
            __ARQCallbackLo = __ARQRequestPendingLo->callback;
        } else if (__ARQRequestPendingLo->type == ARQ_TYPE_MRAM_TO_ARAM) {
            ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->source, __ARQRequestPendingLo->dest,
                       __ARQChunkSize);
        } else {
            ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest, __ARQRequestPendingLo->source,
                       __ARQChunkSize);
        }
        __ARQRequestPendingLo->length -= __ARQChunkSize;
        __ARQRequestPendingLo->source += __ARQChunkSize;
        __ARQRequestPendingLo->dest += __ARQChunkSize;
    }
}

void __ARQCallbackHack(u32 pointerToARQRequest) {
}

static inline void __ARQStartHi(void) {
    if (__ARQRequestQueueHi != NULL) {
        if (__ARQRequestQueueHi->type == ARQ_TYPE_MRAM_TO_ARAM) {
            ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->source, __ARQRequestQueueHi->dest,
                       __ARQRequestQueueHi->length);
        } else {
            ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->dest, __ARQRequestQueueHi->source,
                       __ARQRequestQueueHi->length);
        }
        __ARQCallbackHi = __ARQRequestQueueHi->callback;
        __ARQRequestPendingHi = __ARQRequestQueueHi;
        __ARQRequestQueueHi = __ARQRequestQueueHi->next;
    }
}

void __ARQInterruptServiceRoutine(void) {
    if (__ARQCallbackHi != NULL) {
        __ARQCallbackHi((u32)__ARQRequestPendingHi);
        __ARQRequestPendingHi = NULL;
        __ARQCallbackHi = NULL;
    } else if (__ARQCallbackLo != NULL) {
        __ARQCallbackLo((u32)__ARQRequestPendingLo);
        __ARQRequestPendingLo = NULL;
        __ARQCallbackLo = NULL;
    }
    __ARQStartHi();
    if (__ARQRequestPendingHi == NULL) {
        __ARQServiceQueueLo();
    }
}

void ARQInit(void) {
    if (__ARQ_init_flag == TRUE) {
        return;
    }
    __ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
    __ARQChunkSize = 0x1000;
    ARRegisterDMACallback(__ARQInterruptServiceRoutine);
    __ARQRequestPendingHi = NULL;
    __ARQRequestPendingLo = NULL;
    __ARQCallbackHi = NULL;
    __ARQCallbackLo = NULL;
    __ARQ_init_flag = TRUE;
}

void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback) {
    BOOL enabled;
    request->next = NULL;
    request->owner = owner;
    request->type = type;
    request->source = source;
    request->dest = dest;
    request->length = length;
    if (callback != NULL) {
        request->callback = callback;
    } else {
        request->callback = __ARQCallbackHack;
    }
    enabled = OSDisableInterrupts();
    switch (priority) {
    case ARQ_PRIORITY_LOW:
        if (__ARQRequestQueueLo != NULL) {
            __ARQRequestTailLo->next = request;
        } else {
            __ARQRequestQueueLo = request;
        }
        __ARQRequestTailLo = request;
        break;
    case ARQ_PRIORITY_HIGH:
        if (__ARQRequestQueueHi != NULL) {
            __ARQRequestTailHi->next = request;
        } else {
            __ARQRequestQueueHi = request;
        }
        __ARQRequestTailHi = request;
        break;
    }
    if (__ARQRequestPendingHi == NULL && __ARQRequestPendingLo == NULL) {
        __ARQStartHi();
        if (__ARQRequestPendingHi == NULL) {
            __ARQServiceQueueLo();
        }
    }
    OSRestoreInterrupts(enabled);
}

// Drops the request from both queues (not from a transfer in progress unless it is a low
// priority one whose last chunk has not started).
void ARQRemoveRequest(ARQRequest* task) {
    ARQRequest* r;
    BOOL enabled = OSDisableInterrupts();

    __ARQRequestQueueTemp = __ARQRequestTailTemp = NULL;
    for (r = __ARQRequestQueueHi; r != NULL; r = r->next) {
        if (r != task) {
            if (__ARQRequestQueueTemp == NULL) {
                __ARQRequestQueueTemp = __ARQRequestTailTemp = r;
            } else {
                __ARQRequestTailTemp->next = r;
                __ARQRequestTailTemp = r;
            }
        }
    }
    __ARQRequestQueueHi = __ARQRequestQueueTemp;
    __ARQRequestTailHi = __ARQRequestTailTemp;

    __ARQRequestQueueTemp = __ARQRequestTailTemp = NULL;
    for (r = __ARQRequestQueueLo; r != NULL; r = r->next) {
        if (r != task) {
            if (__ARQRequestQueueTemp == NULL) {
                __ARQRequestQueueTemp = __ARQRequestTailTemp = r;
            } else {
                __ARQRequestTailTemp->next = r;
                __ARQRequestTailTemp = r;
            }
        }
    }
    __ARQRequestQueueLo = __ARQRequestQueueTemp;
    __ARQRequestTailLo = __ARQRequestTailTemp;

    if (__ARQRequestPendingLo != NULL && __ARQRequestPendingLo == task && __ARQCallbackLo == NULL) {
        __ARQRequestPendingLo = NULL;
    }
    OSRestoreInterrupts(enabled);
}
