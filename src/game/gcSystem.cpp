// gcSystem: GameCube system initialization and the system critical section.
#include "game/gcSystem.h"
#include "game/system.h"
#include "ea/sys/mutex2.h"
#include "dolphin/dvd.h"

extern "C" {
void OSInit(void);
void* OSGetCurrentThread(void);
}

// Shared by every critical section.
static u8 _Sys_CritSectSema[0x1C] __attribute__((aligned(32)));
static int _Sys_CritSectCtr;

void SysShutdownCriticalSectionFunc(void* cs) {
    SysCriticalSection* section = (SysCriticalSection*)cs;
    MUTEX_destroy(section->m_mutex);
    section->m_owner = NULL;
    if (--_Sys_CritSectCtr == 0) {
        MUTEX_destroy(_Sys_CritSectSema);
    }
}

s32 SysShutdownDependent() {
    return 0;
}

void SysLoadAllModules(bool, int) {}

void SysInitCriticalSectionFunc(void* cs) {
    SysCriticalSection* section = (SysCriticalSection*)cs;
    if (_Sys_CritSectCtr == 0) {
        MUTEX_create(_Sys_CritSectSema);
    }
    _Sys_CritSectCtr++;
    MUTEX_create(section->m_mutex);
    section->m_owner = OSGetCurrentThread();
    section->m_count = 0;
}

s32 SysInitDependent(const SysInitParms_t*) {
    OSInit();
    DVDInit();
    return 0;
}
