// Skeleton for game/system (from tools/gen_headers.py, now hand-maintained).
#ifndef _GAME_SYSTEM_H_
#define _GAME_SYSTEM_H_

#include "types.h"
#include "game_types.h"

// Parameters for SysInit (size 0x8).
struct SysInitParms_t {
    /* 0x0 */ u8 field_0x0;
    /* 0x1 */ u8 field_0x1;
    /* 0x2 */ u8 field_0x2;
    /* 0x3 */ u8 field_0x3;
    /* 0x4 */ u32 field_0x4;
};

extern bool _Sys_ModuleActive;

// Platform-dependent parts (defined elsewhere, C++ linkage).
s32 SysInitDependent(const SysInitParms_t* parms);  // 0x800F0B58
s32 SysShutdownDependent();                          // 0x800F0AE8

#ifdef __cplusplus
extern "C" {
#endif

void SysSetLastErrorFunc(s32 error);   // 0x8001CBD8
s32 SysShutdown(void);                 // 0x8001CBE0
s32 SysInit(SysInitParms_t* parms);    // 0x8001CC44

void SysInitCriticalSectionFunc(void* cs);      // 0x800F0AF4
void SysShutdownCriticalSectionFunc(void* cs);  // 0x800F0A98

#ifdef __cplusplus
}
#endif

#endif
