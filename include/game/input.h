// CInputManager: maps controller buttons and axes to game input events
// through a KEYMAP, on top of CDeviceManager.
// Hand-maintained (layout checked against the retail code).
#ifndef _GAME_INPUT_H_
#define _GAME_INPUT_H_

#include "types.h"
#include "game/devicemgr.h"

// CInputEvent::flags
#define INPUTEVENT_ANALOG 0x1   // source is an analog slot, not a button bit
#define INPUTEVENT_PRESS 0x2    // value is 1 only on the frame it changes to non-zero
#define INPUTEVENT_SIGNED 0x4   // analog centred on 128 -> -128..127
#define INPUTEVENT_CUBIC 0x8    // response curve value^3
#define INPUTEVENT_CHORD 0x10   // source is two button bits (low byte, high byte), both held

// One binding: input event index <- button bit / analog slot (0xFFFF = unbound).
struct KEYMAPENTRY {
    /* 0x0 */ u16 event;
    /* 0x2 */ u16 source;
};

// Ends at an entry whose event is 0xFFFF.
struct KEYMAP {
    /* 0x0 */ u32 id;
    /* 0x4 */ KEYMAPENTRY entries[1];
};

// size 0x2450
class CInputManager : public CDeviceManager {
public:
    /* 0x2430 */ KEYMAP* m_keymap;
    /* 0x2434 */ KEYMAP* m_playerKeymaps[NUM_INPUT_DEVICES];  // multiplayer
    /* 0x2444 */ u8 m_anyInput;          // some event was non-zero this frame
    /* 0x2445 */ u8 m_feedbackEnabled;   // single player rumble option
    /* 0x2446 */ u8 m_feedbackBlocked[NUM_INPUT_DEVICES];
    /* 0x244A */ u8 field_0x244A;
    /* 0x244B */ u8 m_dpadLocked[NUM_INPUT_DEVICES];  // D-pad up/down only drive events 0x8E/0x8F

    void Reset();  // 0x80084958
    int VerifyAnalog();  // 0x80084A04
    bool IsReady();  // 0x80084A0C
    void SetFeedback(unsigned long, long, long, float, float);  // 0x80084A6C
    void ResetActuatorState(long);  // 0x80084ADC
    bool GetValueAnyPlayer(unsigned long);  // 0x80084B58
    int GetValue(unsigned long, unsigned long);  // 0x80084C14
    void SetKeymap(unsigned long, KEYMAP*);  // 0x80084C3C
    void Shutdown();  // 0x80084C60
    void Update(float);  // 0x80084C80
    void Init();  // 0x80085144
};

extern CInputManager g_inputMgr;  // 0x801EC80C

extern KEYMAP g_config_keymap[];  // per-port keymaps, 0x104 bytes apart (KEYMAP is variable-sized)

#ifdef FRONTLINE_NATIVE
#include "native/hooks.h"

// This frame's keyboard and mouse input on controller `dev` (mouse look, for
// the player; the actions are already in the input events).
const NativeInput& NativeFrameInput(int dev);
#endif
#endif
