// Animation module setup and the user-opcode callback hooks used while
// animation scripts run. Hand-maintained.
#ifndef _GAME_ANIM_H_
#define _GAME_ANIM_H_

#include "types.h"

class CAnimDatabase;

// Called for user opcodes in animation scripts with the innermost pushed user
// data and state.
// Returns whether the opcode was handled.
typedef int (*AnimUserOpcodeCallback)(void* userData, unsigned short opcode, unsigned long state);

extern "C" {
// Returns 0, or 0x140001 if the module was already active.
s32 AnimInit(void);  // 0x8006923C
// Returns 0, or 0x140002 if the module was not active.
s32 AnimShutdown(void);  // 0x800691E4
void AnimInitHeader(u16* header, u16 a, u16 b, u16 c);  // 0x800691D4
void AnimSetUserOpcodeCallbacks(AnimUserOpcodeCallback callback,
                                AnimUserOpcodeCallback skipCallback);  // 0x800691C8
}

// Return non-zero when the callback consumed the opcode's parameter word.
int _AnimDoUserOpcodeCallback(unsigned short opcode);  // 0x80069350
int _AnimDoUserOpcodeSkipCallback(unsigned short opcode);  // 0x8006931C
void _AnimUserCallbackPushState(unsigned long state);  // 0x800692F4
void _AnimUserCallbackPushInfo(void* userData);  // 0x800692CC
void _AnimUserCallbackPopState();  // 0x800692BC
void _AnimUserCallbackPopInfo();  // 0x800692AC

extern CAnimDatabase g_AnimDB;  // 0x801D4A84

#endif
