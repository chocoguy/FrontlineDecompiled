// Dolphin SDK AR/ARQ (auxiliary RAM) declarations, added as decompiled code needs them.
#ifndef _DOLPHIN_AR_H_
#define _DOLPHIN_AR_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARAM_DIR_MRAM_TO_ARAM 0
#define ARAM_DIR_ARAM_TO_MRAM 1

#define ARQ_TYPE_MRAM_TO_ARAM ARAM_DIR_MRAM_TO_ARAM
#define ARQ_TYPE_ARAM_TO_MRAM ARAM_DIR_ARAM_TO_MRAM
#define ARQ_PRIORITY_LOW 0
#define ARQ_PRIORITY_HIGH 1

typedef void (*ARCallback)(void);
typedef void (*ARQCallback)(u32 pointerToARQRequest);

typedef struct ARQRequest {
    /* 0x00 */ struct ARQRequest* next;
    /* 0x04 */ u32 owner;
    /* 0x08 */ u32 type;
    /* 0x0C */ u32 priority;
    /* 0x10 */ u32 source;
    /* 0x14 */ u32 dest;
    /* 0x18 */ u32 length;
    /* 0x1C */ ARQCallback callback;
} ARQRequest;

ARCallback ARRegisterDMACallback(ARCallback callback);
u32 ARGetDMAStatus(void);
void ARStartDMA(u32 type, u32 mainmem_addr, u32 aram_addr, u32 length);
u32 ARAlloc(u32 length);
u32 ARFree(u32* length);
BOOL ARCheckInit(void);
u32 ARInit(u32* stackIndexAddr, u32 numEntries);
void ARReset(void);
void ARSetSize(void);
u32 ARGetBaseAddress(void);
u32 ARGetSize(void);
u32 ARGetInternalSize(void);
void ARClear(u32 flag);

void ARQInit(void);
void ARQReset(void);
void ARQPostRequest(ARQRequest* task, u32 owner, u32 type, u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback);
void ARQRemoveRequest(ARQRequest* task);
void ARQRemoveOwnerRequest(u32 owner);
void ARQFlushQueue(void);
void ARQSetChunkSize(u32 size);
u32 ARQGetChunkSize(void);
BOOL ARQCheckInit(void);

#ifdef __cplusplus
}
#endif

#endif
