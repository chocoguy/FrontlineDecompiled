// MetroTRK (CodeWarrior debug monitor, GameCube "MINNOW_DOLPHIN" build) internals.
#ifndef _TRK_MINNOW_DOLPHIN_TRK_H_
#define _TRK_MINNOW_DOLPHIN_TRK_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int DSError;
typedef int MessageBufferID;
typedef u32 NubEventID;
typedef u8 MessageCommandID;

#define kNoError 0
#define kEventQueueFull 0x100
#define kNoMessageBufferAvailable 0x300
#define kMessageBufferOverflow 0x301
#define kMessageBufferReadError 0x302
#define kDispatchError 0x500

#define TRK_MSG_BUFFER_SIZE 0x880
#define TRK_MSG_BUFFER_COUNT 3

// NubEventType
#define kNullEvent 0
#define kShutdownEvent 1
#define kRequestEvent 2
#define kBreakpointEvent 3
#define kExceptionEvent 4
#define kSupportEvent 5

// Reply/notify command IDs
#define kDSReplyACK 0x80
#define kDSNotifyStopped 0x90

// One framed message. size 0x890
typedef struct TRKBuffer {
    /* 0x00 */ u32 mutex;
    /* 0x04 */ BOOL isInUse;
    /* 0x08 */ u32 length;
    /* 0x0C */ u32 position;
    /* 0x10 */ u8 data[TRK_MSG_BUFFER_SIZE];
} TRKBuffer;

// size 0xC
typedef struct TRKEvent {
    /* 0x0 */ u8 eventType;
    /* 0x4 */ NubEventID eventID;
    /* 0x8 */ MessageBufferID msgBufID;
} TRKEvent;

// size 0x28
typedef struct TRKEventQueue {
    /* 0x00 */ u32 mutex;
    /* 0x04 */ int count;
    /* 0x08 */ int next;
    /* 0x0C */ TRKEvent events[2];
    /* 0x24 */ NubEventID eventID;
} TRKEventQueue;

// Serial receive state. size 0x14
typedef struct TRKFramingState {
    /* 0x00 */ MessageBufferID msgBufID;
    /* 0x04 */ TRKBuffer* buffer;
    /* 0x08 */ u8 receiveState;
    /* 0x0C */ BOOL isEscape;
    /* 0x10 */ u8 fcs;
} TRKFramingState;

// Receive states
#define kTRKWaitFlag 0
#define kTRKFoundFlag 1
#define kTRKInFrame 2
#define kTRKFrameOverflow 3

// Framing bytes
#define TRK_FRAME_FLAG 0x7E
#define TRK_FRAME_ESCAPE 0x7D

// Debugger-side copy of the target CPU state. size 0x430
typedef struct Default_PPC {
    /* 0x00 */ u32 GPR[32];
    /* 0x80 */ u32 PC;
    /* 0x84 */ u32 LR;
    /* 0x88 */ u32 CR;
    /* 0x8C */ u32 CTR;
    /* 0x90 */ u32 XER;
} Default_PPC;

typedef struct Float_PPC {
    /* 0x000 */ u64 FPR[32];
    /* 0x100 */ u64 FPSCR;
    /* 0x108 */ u64 FPECR;
} Float_PPC;

typedef struct Extended1_PPC_6xx_7xx {
    /* 0x000 */ u32 SR[16];
    /* 0x040 */ u32 TBL;
    /* 0x044 */ u32 TBU;
    /* 0x048 */ u32 HID0;
    /* 0x04C */ u32 HID1;
    /* 0x050 */ u32 MSR;
    /* 0x054 */ u32 PVR;
    /* 0x058 */ u32 IBAT0U;
    /* 0x05C */ u32 IBAT0L;
    /* 0x060 */ u32 IBAT1U;
    /* 0x064 */ u32 IBAT1L;
    /* 0x068 */ u32 IBAT2U;
    /* 0x06C */ u32 IBAT2L;
    /* 0x070 */ u32 IBAT3U;
    /* 0x074 */ u32 IBAT3L;
    /* 0x078 */ u32 DBAT0U;
    /* 0x07C */ u32 DBAT0L;
    /* 0x080 */ u32 DBAT1U;
    /* 0x084 */ u32 DBAT1L;
    /* 0x088 */ u32 DBAT2U;
    /* 0x08C */ u32 DBAT2L;
    /* 0x090 */ u32 DBAT3U;
    /* 0x094 */ u32 DBAT3L;
    /* 0x098 */ u32 DMISS;
    /* 0x09C */ u32 DCMP;
    /* 0x0A0 */ u32 HASH1;
    /* 0x0A4 */ u32 HASH2;
    /* 0x0A8 */ u32 IMISS;
    /* 0x0AC */ u32 ICMP;
    /* 0x0B0 */ u32 RPA;
    /* 0x0B4 */ u32 SDR1;
    /* 0x0B8 */ u32 DAR;
    /* 0x0BC */ u32 DSISR;
    /* 0x0C0 */ u32 SPRG0;
    /* 0x0C4 */ u32 SPRG1;
    /* 0x0C8 */ u32 SPRG2;
    /* 0x0CC */ u32 SPRG3;
    /* 0x0D0 */ u32 DEC;
    /* 0x0D4 */ u32 IABR;
    /* 0x0D8 */ u32 EAR;
    /* 0x0DC */ u32 DABR;
    /* 0x0E0 */ u32 PMC1;
    /* 0x0E4 */ u32 PMC2;
    /* 0x0E8 */ u32 PMC3;
    /* 0x0EC */ u32 PMC4;
    /* 0x0F0 */ u32 SIA;
    /* 0x0F4 */ u32 MMCR0;
    /* 0x0F8 */ u32 MMCR1;
    /* 0x0FC */ u32 THRM1;
    /* 0x100 */ u32 THRM2;
    /* 0x104 */ u32 THRM3;
    /* 0x108 */ u32 ICTC;
    /* 0x10C */ u32 L2CR;
    /* 0x110 */ u32 UMMCR2;
    /* 0x114 */ u32 UBAMR;
    /* 0x118 */ u32 UMMCR0;
    /* 0x11C */ u32 UPMC1;
    /* 0x120 */ u32 UPMC2;
    /* 0x124 */ u32 USIA;
    /* 0x128 */ u32 UMMCR1;
    /* 0x12C */ u32 UPMC3;
    /* 0x130 */ u32 UPMC4;
    /* 0x134 */ u32 USDA;
    /* 0x138 */ u32 MMCR2;
    /* 0x13C */ u32 BAMR;
    /* 0x140 */ u32 SDA;
    /* 0x144 */ u32 MSSCR0;
    /* 0x148 */ u32 MSSCR1;
    /* 0x14C */ u32 PIR;
    /* 0x150 */ u32 exceptionID;
    /* 0x154 */ u32 GQR[8];
    /* 0x174 */ u32 HID2;
    /* 0x178 */ u32 WPAR;
    /* 0x17C */ u32 DMA_U;
    /* 0x180 */ u32 DMA_L;
} Extended1_PPC_6xx_7xx;

typedef struct Extended2_PPC_6xx_7xx {
    /* 0x00 */ u32 PSR[32][2];
} Extended2_PPC_6xx_7xx;

typedef struct ProcessorState_PPC {
    /* 0x000 */ Default_PPC Default;
    /* 0x098 */ Float_PPC Float;
    /* 0x1A8 */ Extended1_PPC_6xx_7xx Extended1;
    /* 0x32C */ Extended2_PPC_6xx_7xx Extended2;
    /* 0x42C */ u32 transport_handler_saved_ra;
} ProcessorState_PPC;

// Monitor state. size 0xA4
typedef struct TRKState_PPC {
    /* 0x00 */ u32 GPR[32];
    /* 0x80 */ u32 LR;
    /* 0x84 */ u32 CTR;
    /* 0x88 */ u32 XER;
    /* 0x8C */ u32 MSR;
    /* 0x90 */ u32 DAR;
    /* 0x94 */ u32 DSISR;
    /* 0x98 */ BOOL isStopped;
    /* 0x9C */ BOOL inputActivated;
    /* 0xA0 */ void* inputPendingPtr;
} TRKState_PPC;

extern ProcessorState_PPC gTRKCPUState;
extern TRKState_PPC gTRKState;

// A header inline in MetroTRK (no out-of-line copy exists in this build).
static inline DSError TRKAppendBuffer1_ui8(TRKBuffer* buffer, const u8 data) {
    if (buffer->position >= TRK_MSG_BUFFER_SIZE) {
        return kMessageBufferOverflow;
    }
    buffer->data[buffer->position++] = data;
    buffer->length++;
    return kNoError;
}

// mainloop.c
void TRKNubMainLoop(void);

// nubevent.c
extern TRKEventQueue gTRKEventQueue;
void TRKDestructEvent(TRKEvent* event);
void TRKConstructEvent(TRKEvent* event, int eventType);
DSError TRKPostEvent(TRKEvent* event);
BOOL TRKGetNextEvent(TRKEvent* event);
DSError TRKInitializeEventQueue(void);

// nubinit.c
extern BOOL gTRKBigEndian;
void TRKNubWelcome(void);
DSError TRKTerminateNub(void);
DSError TRKInitializeNub(void);

// msg.c
DSError TRKMessageSend(TRKBuffer* msg);

// msgbuf.c
DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count);
DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count);
DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data);
DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data);
DSError TRKReadBuffer1_ui16(TRKBuffer* buffer, u16* data);
DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data);
DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count);
DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count);
DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data);
DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data);
DSError TRKAppendBuffer1_ui16(TRKBuffer* buffer, const u16 data);
DSError TRKReadBuffer(TRKBuffer* buffer, void* data, u32 length);
DSError TRKAppendBuffer(TRKBuffer* buffer, const void* data, u32 length);
DSError TRKSetBufferPosition(TRKBuffer* buffer, u32 position);
void TRKResetBuffer(TRKBuffer* buffer, u8 keepData);
void TRKReleaseBuffer(MessageBufferID id);
TRKBuffer* TRKGetBuffer(MessageBufferID id);
DSError TRKGetFreeBuffer(MessageBufferID* id, TRKBuffer** buffer);
DSError TRKInitializeMessageBuffers(void);
extern TRKBuffer gTRKMsgBufs[TRK_MSG_BUFFER_COUNT];

// serpoll.c
extern void* gTRKInputPendingPtr;
DSError TRKTerminateSerialHandler(void);
DSError TRKInitializeSerialHandler(void);
void TRKProcessInput(MessageBufferID bufferIdx);
void TRKGetInput(void);
MessageBufferID TRKTestForPacket(void);

// usr_put.c
void usr_put_initialize(void);
BOOL usr_puts_serial(const char* msg);

// dispatch.c
DSError TRKDispatchMessage(TRKBuffer* buffer);
DSError TRKInitializeDispatcher(void);

// msghndlr.c
DSError TRKDoUnsupported(TRKBuffer* buffer);
DSError TRKDoConnect(TRKBuffer* buffer);
DSError TRKDoDisconnect(TRKBuffer* buffer);
DSError TRKDoReset(TRKBuffer* buffer);
DSError TRKDoVersions(TRKBuffer* buffer);
DSError TRKDoSupportMask(TRKBuffer* buffer);
DSError TRKDoCPUType(TRKBuffer* buffer);
DSError TRKDoReadMemory(TRKBuffer* buffer);
DSError TRKDoWriteMemory(TRKBuffer* buffer);
DSError TRKDoReadRegisters(TRKBuffer* buffer);
DSError TRKDoWriteRegisters(TRKBuffer* buffer);
DSError TRKDoFlushCache(TRKBuffer* buffer);
DSError TRKDoSetOption(TRKBuffer* buffer);
DSError TRKDoContinue(TRKBuffer* buffer);
DSError TRKDoStep(TRKBuffer* buffer);
DSError TRKDoStop(TRKBuffer* buffer);
DSError TRKStandardACK(TRKBuffer* buffer, MessageCommandID commandId, int replyError);
void SetTRKConnected(BOOL connected);
BOOL GetTRKConnected(void);

// support.c
DSError TRKSuppAccessFile(u32 file_handle, u8* data, u32* count, u8* io_result, BOOL need_reply, BOOL read);
DSError TRKRequestSend(TRKBuffer* msgBuf, MessageBufferID* bufferId, u32 p1, u32 p2, int p3);
DSError HandleOpenFileSupportRequest(const char* path, u8 replyError, u32* param_3, u8* ioResult);
DSError HandleCloseFileSupportRequest(u32 file_handle, u8* ioResult);
DSError HandlePositionFileSupportRequest(u32 file_handle, u32* position, u8 mode, u8* ioResult);

// targsupp.s: trap into the debugger with the request in registers.
u8 TRKAccessFile(u8 command, u32 file_handle, u32* count, u8* buffer);
u8 TRKOpenFile(u8 command, const char* name, u8 mode, u32* handle);
u8 TRKCloseFile(u8 command, u32 file_handle);
u8 TRKPositionFile(u8 command, u32 file_handle, s32* position, u8 mode);

// mutex_TRK.c
DSError TRKReleaseMutex(void* mutex);
DSError TRKAcquireMutex(void* mutex);
DSError TRKInitializeMutex(void* mutex);

// notify.c
DSError TRKDoNotifyStopped(MessageCommandID cmd);

// flush_cache.c
void TRK_flush_cache(void* address, u32 size);

// mem_TRK.c
void* TRK_memset(void* dst, int val, u32 n);
void* TRK_memcpy(void* dst, const void* src, u32 n);

// targimpl.c
typedef struct DSVersions {
    /* 0x0 */ u8 kernelMajor;
    /* 0x1 */ u8 kernelMinor;
    /* 0x2 */ u8 protocolMajor;
    /* 0x3 */ u8 protocolMinor;
} DSVersions;

typedef struct DSCPUType {
    /* 0x0 */ u8 cpuMajor;
    /* 0x1 */ u8 cpuMinor;
    /* 0x2 */ u8 bigEndian;
    /* 0x3 */ u8 defaultTypeSize;
    /* 0x4 */ u8 fpTypeSize;
    /* 0x5 */ u8 extended1TypeSize;
    /* 0x6 */ u8 extended2TypeSize;
} DSCPUType;

// TRK internal error codes returned by the target layer.
#define kInvalidMemory 0x700
#define kInvalidRegister 0x701
#define kCWDSException 0x702
#define kUnsupportedError 0x703
#define kInvalidProcessId 0x704
#define kInvalidThreadId 0x705
#define kOsError 0x706

#define kUserMemory 0
#define kDebuggerMemory 1

DSError TRKTargetVersions(DSVersions* versions);
DSError TRKTargetSupportMask(u8 mask[32]);
DSError TRKTargetCPUType(DSCPUType* cpuType);
DSError TRKTargetAccessMemory(void* data, u32 start, u32* length, int memorySpace, BOOL read);
DSError TRKTargetAccessDefault(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                               BOOL read);
DSError TRKTargetAccessFP(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize, BOOL read);
DSError TRKTargetAccessExtended1(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                                 BOOL read);
DSError TRKTargetAccessExtended2(u32 firstRegister, u32 lastRegister, TRKBuffer* b, u32* registerStorageSize,
                                 BOOL read);
DSError TRKTargetFlushCache(u8 options, void* start, u32 end);
DSError TRKTargetSingleStep(u32 count, BOOL stepOver);
DSError TRKTargetStepOutOfRange(u32 rangeStart, u32 rangeEnd, BOOL stepOver);
u32 TRKTargetGetPC(void);
DSError TRKTargetStop(void);
void TRKTargetSetInputPendingPtr(void* ptr);
DSError TRKTargetSupportRequest(void);
DSError TRKTargetInterrupt(TRKEvent* event);
BOOL TRKTargetStopped(void);
void TRKTargetSetStopped(unsigned int stopped);
void TRKTargetAddStopInfo(TRKBuffer* buffer);
void TRKTargetAddExceptionInfo(TRKBuffer* buffer);
void TRKSwapAndGo(void);

// dolphin_trk.c
void __TRK_reset(void);
DSError TRKInitializeTarget(void);
u32 TRKTargetTranslate(u32 addr);
void EnableMetroTRKInterrupts(void);

// mpc_7xx_603e.c
void TRKSaveExtended1Block(void);
void TRKRestoreExtended1Block(void);
u8 TRKTargetCPUMinorType(void);

// targimpl.c
u32 __TRK_get_MSR(void);
void __TRK_set_MSR(u32 msr);
extern u8 gTRKInterruptVectorTable[];
extern u8 gTRKRestoreFlags[];

// main_TRK.c
DSError TRK_main(void);

// dolphin_trk_glue.c
int InitMetroTRKCommTable(int hwId);
void EnableEXI2Interrupts(void);
DSError TRKInitializeIntDrivenUART(u32 param_0, u32 param_1, u32 param_2, volatile u8** param_3);
int TRKReadUARTPoll(u8* c);
int WriteUART1(s8 c);
int WriteUARTFlush(void);
void TRK_board_display(char* str);
void UnreserveEXI2Port(void);
void ReserveEXI2Port(void);

// targcont.c
DSError TRKTargetContinue(void);

// target_options.c
void SetUseSerialIO(u8 sio);
u8 GetUseSerialIO(void);

void OSReport(const char* msg, ...);

#ifdef __cplusplus
}
#endif

#endif
