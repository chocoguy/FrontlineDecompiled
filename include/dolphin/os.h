// Minimal Dolphin SDK OS declarations, added as decompiled code needs them.
// Signatures follow the public Dolphin SDK.
#ifndef _DOLPHIN_OS_H_
#define _DOLPHIN_OS_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void OSInit(void);
void OSReport(const char* msg, ...);
void OSPanic(const char* file, int line, const char* msg, ...);
BOOL OSDisableInterrupts(void);
BOOL OSEnableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
void DCFlushRange(void* addr, u32 nBytes);
void DCStoreRange(void* addr, u32 nBytes);
void DCStoreRangeNoSync(void* addr, u32 nBytes);
void DCFlushRangeNoSync(void* addr, u32 nBytes);
void PPCSync(void);
void ICInvalidateRange(void* addr, u32 nBytes);
u32 OSGetResetCode(void);

void* OSGetArenaLo(void);
void* OSGetArenaHi(void);
void OSSetArenaLo(void* addr);
void OSSetArenaHi(void* addr);

// Heaps
typedef int OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps);
OSHeapHandle OSCreateHeap(void* start, void* end);
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);
void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);

u32 OSGetTick(void);
typedef s64 OSTime;
OSTime OSGetTime(void);
OSTime __OSGetSystemTime(void);
#ifdef FRONTLINE_NATIVE
// On the console these are words in low memory the IPL fills in.
#define OS_BUS_CLOCK 162000000u
#define OS_CORE_CLOCK 486000000u
#else
#define OS_BUS_CLOCK (*(u32*)0x800000F8)
#define OS_CORE_CLOCK (*(u32*)0x800000FC)
#endif
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))
#define OSSecondsToTicks(sec) ((sec) * OS_TIMER_CLOCK)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSMicrosecondsToTicks(usec) (((usec) * (OS_TIMER_CLOCK / 125000)) / 8)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))

// The IPL ROM font. size 0x30
typedef struct OSFontHeader {
    /* 0x00 */ u16 fontType;
    /* 0x02 */ u16 firstChar;
    /* 0x04 */ u16 lastChar;
    /* 0x06 */ u16 invalChar;
    /* 0x08 */ u16 ascent;
    /* 0x0A */ u16 descent;
    /* 0x0C */ u16 width;
    /* 0x0E */ u16 leading;
    /* 0x10 */ u16 cellWidth;
    /* 0x12 */ u16 cellHeight;
    /* 0x14 */ u32 sheetSize;
    /* 0x18 */ u16 sheetFormat;
    /* 0x1A */ u16 sheetColumn;
    /* 0x1C */ u16 sheetRow;
    /* 0x1E */ u16 sheetWidth;
    /* 0x20 */ u16 sheetHeight;
    /* 0x22 */ u16 widthTable;
    /* 0x24 */ u32 sheetImage;
    /* 0x28 */ u32 sheetFullSize;
    /* 0x2C */ u8 c0;
    /* 0x2D */ u8 c1;
    /* 0x2E */ u8 c2;
    /* 0x2F */ u8 c3;
} OSFontHeader;
BOOL OSInitFont(OSFontHeader* fontData);
char* OSGetFontTexture(const char* string, void** image, s32* x, s32* y, s32* width);
char* OSGetFontWidth(const char* string, s32* width);

void DCInvalidateRange(void* addr, u32 nBytes);
void DCEnable(void);
void ICEnable(void);
void ICFlashInvalidate(void);
void LCDisable(void);
void L2GlobalInvalidate(void);


// CPU context. size 0x2C8
typedef struct OSContext {
    /* 0x000 */ u32 gpr[32];
    /* 0x080 */ u32 cr;
    /* 0x084 */ u32 lr;
    /* 0x088 */ u32 ctr;
    /* 0x08C */ u32 xer;
    /* 0x090 */ f64 fpr[32];
    /* 0x190 */ u32 fpscr_pad;
    /* 0x194 */ u32 fpscr;
    /* 0x198 */ u32 srr0;
    /* 0x19C */ u32 srr1;
    /* 0x1A0 */ u16 mode;
    /* 0x1A2 */ u16 state;
    /* 0x1A4 */ u32 gqr[8];
    /* 0x1C4 */ u32 psf_pad;
    /* 0x1C8 */ f64 psf[32];
} OSContext;
void OSClearContext(OSContext* context);
void OSSetCurrentContext(OSContext* context);
OSContext* OSGetCurrentContext(void);
void OSDumpContext(OSContext* context);

// Errors (exception numbers).
typedef u16 OSError;
typedef void (*OSErrorHandler)(OSError error, OSContext* context, ...);
#define OS_ERROR_SYSTEM_RESET 0
#define OS_ERROR_MACHINE_CHECK 1
#define OS_ERROR_DSI 2
#define OS_ERROR_ISI 3
#define OS_ERROR_EXTERNAL_INTERRUPT 4
#define OS_ERROR_ALIGNMENT 5
#define OS_ERROR_PROGRAM 6
#define OS_ERROR_FLOATING_POINT 7
#define OS_ERROR_DECREMENTER 8
#define OS_ERROR_SYSTEM_CALL 9
#define OS_ERROR_TRACE 10
#define OS_ERROR_PERFORMACE_MONITOR 11
#define OS_ERROR_BREAKPOINT 12
#define OS_ERROR_SYSTEM_INTERRUPT 13
#define OS_ERROR_THERMAL_INTERRUPT 14
#define OS_ERROR_PROTECTION 15
#define OS_ERROR_MAX (OS_ERROR_PROTECTION + 1)
OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler);

// Threads, mutexes and message queues.
typedef struct OSThread OSThread;
typedef struct OSMutex OSMutex;
typedef void* OSMessage;

// The game embeds these in buffers of their console size (EA's mutexes are
// u8[0x1C]), so natively their pointers are 32-bit too (PTR32): threads,
// mutexes and queues all live in the 32-bit address map.
typedef struct OSThreadQueue {
    PTR32(OSThread) head;
    PTR32(OSThread) tail;
} OSThreadQueue;

typedef struct OSThreadLink {
    PTR32(OSThread) next;
    PTR32(OSThread) prev;
} OSThreadLink;

typedef struct OSMutexQueue {
    PTR32(OSMutex) head;
    PTR32(OSMutex) tail;
} OSMutexQueue;

typedef struct OSMutexLink {
    PTR32(OSMutex) next;
    PTR32(OSMutex) prev;
} OSMutexLink;

// size 0x310
struct OSThread {
    /* 0x000 */ OSContext context;
    /* 0x2C8 */ u16 state;
    /* 0x2CA */ u16 attr;
    /* 0x2CC */ s32 suspend;
    /* 0x2D0 */ s32 priority;
    /* 0x2D4 */ s32 base;
    /* 0x2D8 */ PTR32(void) val;
    /* 0x2DC */ PTR32(OSThreadQueue) queue;
    /* 0x2E0 */ OSThreadLink link;
    /* 0x2E8 */ OSThreadQueue queueJoin;
    /* 0x2F0 */ PTR32(OSMutex) mutex;
    /* 0x2F4 */ OSMutexQueue queueMutex;
    /* 0x2FC */ OSThreadLink linkActive;
    /* 0x304 */ PTR32(u8) stackBase;
    /* 0x308 */ PTR32(u32) stackEnd;
};

#define OS_THREAD_STATE_READY 1
#define OS_THREAD_STATE_RUNNING 2
#define OS_THREAD_STATE_WAITING 4
#define OS_THREAD_STATE_MORIBUND 8
#define OS_THREAD_ATTR_DETACH 0x0001u
#define OS_PRIORITY_MIN 0
#define OS_PRIORITY_MAX 31
#define OS_PRIORITY_IDLE OS_PRIORITY_MAX
#define OS_THREAD_STACK_MAGIC 0xDEADBABE

// size 0x18
struct OSMutex {
    /* 0x00 */ OSThreadQueue queue;
    /* 0x08 */ PTR32(OSThread) thread;
    /* 0x0C */ s32 count;
    /* 0x10 */ OSMutexLink link;
};

// size 0x20
typedef struct OSMessageQueue {
    /* 0x00 */ OSThreadQueue queueSend;
    /* 0x08 */ OSThreadQueue queueReceive;
    /* 0x10 */ PTR32(OSMessage) msgArray;
    /* 0x14 */ s32 msgCount;
    /* 0x18 */ s32 firstIndex;
    /* 0x1C */ s32 usedCount;
} OSMessageQueue;
FILE_SIZE_ASSERT(OSThreadQueue, 0x8);
FILE_SIZE_ASSERT(OSThread, 0x310);
FILE_SIZE_ASSERT(OSMutex, 0x18);
FILE_SIZE_ASSERT(OSMessageQueue, 0x20);
#define OS_MESSAGE_NOBLOCK 0
#define OS_MESSAGE_BLOCK 1

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize,
                    s32 priority, u16 attr);
s32 OSResumeThread(OSThread* thread);
s32 OSSuspendThread(OSThread* thread);
void OSInitMutex(OSMutex* mutex);
void OSLockMutex(OSMutex* mutex);
void OSUnlockMutex(OSMutex* mutex);
void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount);
BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags);
BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags);

void OSInitThreadQueue(OSThreadQueue* queue);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);
void OSCancelThread(OSThread* thread);
void OSExitThread(void* val);
OSThread* OSGetCurrentThread(void);
void OSYieldThread(void);
s32 OSDisableScheduler(void);
s32 OSEnableScheduler(void);

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
u32 __OSMaskInterrupts(u32 mask);
u32 OSGetConsoleType(void);
u8 __OSGetDIConfig(void);
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt);
u32 __OSUnmaskInterrupts(u32 mask);

// Alarms
typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, OSContext* context);
// size 0x28
struct OSAlarm {
    /* 0x00 */ OSAlarmHandler handler;
    /* 0x04 */ u32 tag;
    /* 0x08 */ OSTime fire;
    /* 0x10 */ OSAlarm* prev;
    /* 0x14 */ OSAlarm* next;
    /* 0x18 */ OSTime period;
    /* 0x20 */ OSTime start;
};
void OSInitAlarm(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);

// Reset callbacks. size 0x10
typedef BOOL (*OSResetFunction)(BOOL final);
typedef struct OSResetFunctionInfo OSResetFunctionInfo;
struct OSResetFunctionInfo {
    /* 0x0 */ OSResetFunction func;
    /* 0x4 */ u32 priority;
    /* 0x8 */ OSResetFunctionInfo* next;
    /* 0xC */ OSResetFunctionInfo* prev;
};
void OSRegisterResetFunction(OSResetFunctionInfo* info);

// Battery-backed SRAM. size 0x14
typedef struct OSSram {
    /* 0x00 */ u16 checkSum;
    /* 0x02 */ u16 checkSumInv;
    /* 0x04 */ u32 ead0;
    /* 0x08 */ u32 ead1;
    /* 0x0C */ u32 counterBias;
    /* 0x10 */ s8 displayOffsetH;
    /* 0x11 */ u8 ntd;
    /* 0x12 */ u8 language;
    /* 0x13 */ u8 flags;
} OSSram;

// Extended SRAM. size 0x2C
typedef struct OSSramEx {
    /* 0x00 */ u8 flashID[2][12];
    /* 0x18 */ u32 wirelessKeyboardID;
    /* 0x1C */ u16 wirelessPadID[4];
    /* 0x24 */ u8 dvdErrorCode;
    /* 0x25 */ u8 _padding0;
    /* 0x26 */ u8 flashIDCheckSum[2];
    /* 0x28 */ u16 gbs;
    /* 0x2A */ u8 _padding1[2];
} OSSramEx;

OSSram* __OSLockSram(void);
OSSramEx* __OSLockSramEx(void);
BOOL __OSUnlockSram(BOOL commit);
BOOL __OSUnlockSramEx(BOOL commit);

#define OS_FONT_ENCODE_ANSI 0u
#define OS_FONT_ENCODE_SJIS 1u
u16 OSGetFontEncode(void);
u16 OSGetWirelessID(s32 chan);
void OSSetWirelessID(s32 chan, u16 id);

u32 OSGetResetButtonState(void);
#define OS_RESET_RESTART 0
#define OS_RESET_HOTRESET 1
#define OS_RESET_SHUTDOWN 2
void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu);
typedef void (*OSResetCallback)(void);
OSResetCallback OSSetResetCallback(OSResetCallback callback);
// 0 mono, 1 stereo.
u32 OSGetSoundMode(void);
void OSSetSoundMode(u32 mode);

#ifdef __cplusplus
}
#endif

#endif
