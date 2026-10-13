// gcSystem: the GameCube side of the system layer (OS/DVD init and the
// system critical section). Hand-maintained.
#ifndef _GAME_GCSYSTEM_H_
#define _GAME_GCSYSTEM_H_

#include "types.h"
#include "game_types.h"
#include "game/system.h"  // SysInitDependent, SysShutdownDependent

struct SysInitParms_t;

// A recursive critical section. size 0x24
struct SysCriticalSection {
    /* 0x0 */ u8 m_mutex[0x1C];
    /* 0x1C */ void* m_owner;  // OSThread
    /* 0x20 */ s32 m_count;
};

// Nothing to load on GameCube.
void SysLoadAllModules(bool, int);  // 0x800F0AF0

#endif
