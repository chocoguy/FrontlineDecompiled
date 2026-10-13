#include "game/system.h"

bool _Sys_ModuleActive;
static s32 _Sys_LastError;
static u8 _SysCS[0x24];  // critical section, managed by Sys*CriticalSectionFunc

void SysSetLastErrorFunc(s32 error) {
    _Sys_LastError = error;
}

s32 SysShutdown(void) {
    s32 result;

    if (_Sys_ModuleActive) {
        result = SysShutdownDependent();
        SysShutdownCriticalSectionFunc(_SysCS);
        _Sys_ModuleActive = false;
    } else {
        result = 0x20002;  // not initialized
    }
    _Sys_LastError = result;
    return result;
}

s32 SysInit(SysInitParms_t* parms) {
    s32 result;

    if (!_Sys_ModuleActive) {
        result = SysInitDependent(parms);
        _Sys_ModuleActive = true;
        SysInitCriticalSectionFunc(_SysCS);
    } else {
        result = 0x20001;  // already initialized
    }
    _Sys_LastError = result;
    return result;
}
