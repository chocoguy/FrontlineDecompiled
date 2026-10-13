// Dolphin SDK OS internals shared between the os/ units.
#ifndef _DOLPHIN_OS_PRIV_H_
#define _DOLPHIN_OS_PRIV_H_

#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

// OSThread.c
void __OSThreadInit(void);
s32 __OSGetEffectivePriority(OSThread* thread);
void __OSPromoteThread(OSThread* thread, s32 priority);
void __OSReschedule(void);

#define __OSActiveThreadQueue (*(OSThreadQueue*)0x800000DC)

// OSMutex.c
void __OSUnlockAllMutex(OSThread* thread);

// OSContext.c
void __OSContextInit(void);
void OSSaveFPUContext(OSContext* context);
u32 OSSaveContext(OSContext* context);
void OSLoadContext(OSContext* context);
void OSInitContext(OSContext* context, u32 pc, u32 newsp);
u32 OSGetStackPointer(void);

// OSInterrupt.c
void __OSInterruptInit(void);
void __OSDispatchInterrupt(u8 exception, OSContext* context);

extern volatile __OSInterrupt __OSLastInterrupt;
extern volatile u32 __OSLastInterruptSrr0;
extern volatile OSTime __OSLastInterruptTime;

// OSAlarm.c
BOOL OSCheckAlarmQueue(void);

// OSCache.c
void __OSCacheInit(void);

// OSMemory.c
void __OSInitMemoryProtection(void);

// OSRtc.c
void __OSInitSram(void);
BOOL __OSSyncSram(void);
BOOL __OSReadROM(void* buffer, s32 length, s32 offset);

// OSAudioSystem.c
void __OSInitAudioSystem(void);
void __OSStopAudioSystem(void);

// OSSync.c
void __OSInitSystemCall(void);

// OSTime.c
OSTime __OSTimeToSystemTime(OSTime time);

// OSLink.c
void __OSModuleInit(void);

// OSReset.c / OSReboot.c / OSResetSW.c
void __OSDoHotReset(u32 resetCode);
void __OSReboot(u32 resetCode, BOOL forceMenu);
void __OSResetSWInterruptHandler(__OSInterrupt interrupt, OSContext* context);

// OS.c
typedef u8 __OSException;
#define __OS_EXCEPTION_SYSTEM_RESET 0
#define __OS_EXCEPTION_MACHINE_CHECK 1
#define __OS_EXCEPTION_DSI 2
#define __OS_EXCEPTION_ISI 3
#define __OS_EXCEPTION_EXTERNAL_INTERRUPT 4
#define __OS_EXCEPTION_ALIGNMENT 5
#define __OS_EXCEPTION_PROGRAM 6
#define __OS_EXCEPTION_FLOATING_POINT 7
#define __OS_EXCEPTION_DECREMENTER 8
#define __OS_EXCEPTION_SYSTEM_CALL 9
#define __OS_EXCEPTION_TRACE 10
#define __OS_EXCEPTION_PERFORMACE_MONITOR 11
#define __OS_EXCEPTION_BREAKPOINT 12
#define __OS_EXCEPTION_SYSTEM_INTERRUPT 13
#define __OS_EXCEPTION_THERMAL_INTERRUPT 14
#define __OS_EXCEPTION_MAX 15

typedef void (*__OSExceptionHandler)(__OSException exception, OSContext* context);
__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler);
__OSExceptionHandler __OSGetExceptionHandler(__OSException exception);
void __OSPSInit(void);
extern BOOL __OSInIPL;
extern OSTime __OSStartTime;

// OSError.c
extern OSErrorHandler __OSErrorTable[OS_ERROR_MAX];
void __OSUnhandledException(__OSException exception, OSContext* context, u32 dsisr, u32 dar);

#ifdef __cplusplus
}
#endif

#endif
