#include "game/anim.h"

#include "game/animdb.h"
#include "game/system.h"

#define ANIM_ERR_ALREADY_ACTIVE 0x140001
#define ANIM_ERR_NOT_ACTIVE 0x140002

// Stacks (depth 3) of user data and state for nested animation callbacks.
static void* _Anim_pCurrentUserData[3];
static u32 _Anim_UserCallbackState[3];
CAnimDatabase g_AnimDB;

static bool _Anim_ModuleActive;
static AnimUserOpcodeCallback _Anim_pUserOpcodeCallback;
static AnimUserOpcodeCallback _Anim_pUserOpcodeSkipCallback;
static s8 _Anim_UserDataStackPosition;
static s8 _Anim_UserStateStackPosition;

int _AnimDoUserOpcodeCallback(unsigned short opcode) {
    return _Anim_pUserOpcodeCallback(_Anim_pCurrentUserData[_Anim_UserDataStackPosition], opcode,
                              _Anim_UserCallbackState[_Anim_UserDataStackPosition]);
}

int _AnimDoUserOpcodeSkipCallback(unsigned short opcode) {
    return _Anim_pUserOpcodeSkipCallback(NULL, opcode, 0);
}

void _AnimUserCallbackPushState(unsigned long state) {
    _Anim_UserCallbackState[++_Anim_UserStateStackPosition] = state;
}

void _AnimUserCallbackPushInfo(void* userData) {
    _Anim_pCurrentUserData[++_Anim_UserDataStackPosition] = userData;
}

void _AnimUserCallbackPopState() {
    _Anim_UserStateStackPosition--;
}

void _AnimUserCallbackPopInfo() {
    _Anim_UserDataStackPosition--;
}

extern "C" s32 AnimInit(void) {
    s32 err = 0;
    bool wasActive = _Anim_ModuleActive;
    if (!wasActive) {
        _Anim_ModuleActive = true;
    }
    if (wasActive) {
        err = ANIM_ERR_ALREADY_ACTIVE;
    }
    _Anim_UserDataStackPosition = -1;
    _Anim_UserStateStackPosition = -1;
    g_AnimDB.Init();
    SysSetLastErrorFunc(err);
    return err;
}

extern "C" s32 AnimShutdown(void) {
    s32 err = 0;
    if (_Anim_ModuleActive) {
        _Anim_pUserOpcodeCallback = NULL;
        _Anim_ModuleActive = false;
    } else {
        err = ANIM_ERR_NOT_ACTIVE;
    }
    SysSetLastErrorFunc(err);
    return err;
}

extern "C" void AnimInitHeader(u16* header, u16 a, u16 b, u16 c) {
    header[0] = a;
    header[1] = b;
    header[2] = c;
}

extern "C" void AnimSetUserOpcodeCallbacks(AnimUserOpcodeCallback callback,
                                           AnimUserOpcodeCallback skipCallback) {
    _Anim_pUserOpcodeCallback = callback;
    _Anim_pUserOpcodeSkipCallback = skipCallback;
}
