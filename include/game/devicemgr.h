// CDeviceManager: the four GameCube controller ports, their processed state,
// rumble, and each port's input event table (filled in by CInputManager).
// Hand-maintained (layout checked against the retail code).
#ifndef _GAME_DEVICEMGR_H_
#define _GAME_DEVICEMGR_H_

#include "types.h"

#define NUM_INPUT_DEVICES 4
#define NUM_INPUT_EVENTS 189

// Indices into CDeviceState::analog (also the "source" of analog keymap entries).
enum {
    ANALOG_STICK_X,
    ANALOG_STICK_Y,
    ANALOG_SUBSTICK_X,
    ANALOG_SUBSTICK_Y,
    ANALOG_TRIGGER_L,
    ANALOG_TRIGGER_R,
    ANALOG_A,
    ANALOG_B,
    ANALOG_AB,        // (A - B) / 2 + 128
    ANALOG_TRIGGERS,  // (R - L) / 2 + 128
};

// One controller's processed state, as returned by GetDeviceState.
// Sticks are 0..255 centred on 128 (Y up), triggers and A/B 0..255.
struct CDeviceState {
    /* 0x0 */ u16 buttons;  // PAD_BUTTON_* bits; analog L/R past 0x20 also set 0x40/0x20
    /* 0x2 */ u8 analog[10];
};

// Rumble actuator state.
struct CActuator {
    /* 0x00 */ s32 type;       // 0 = off, 1 = fade in, 2 = fade out, 9..13 = fixed levels
    /* 0x04 */ s32 duration;   // whole seconds, for the fades
    /* 0x08 */ f32 strength;   // 0..255
    /* 0x0C */ f32 timeLeft;
    /* 0x10 */ f32 onTime;     // current pulse: motor on for onTime, off for offTime
    /* 0x14 */ f32 offTime;
    /* 0x18 */ u8 active;
    /* 0x19 */ u8 motorOn;
};

// One input event's current value (see CInputManager).
struct CInputEvent {
    /* 0x0 */ u16 flags;     // INPUTEVENT_*
    /* 0x2 */ u8 deadzone;
    /* 0x4 */ s32 prev;
    /* 0x8 */ s32 value;
};

// size 0x90C
class CGCDevice {
public:
    /* 0x00 */ u8 m_ready;
    /* 0x04 */ s32 m_port;
    /* 0x08 */ CDeviceState m_state;
    /* 0x14 */ CActuator m_actuators[1];
    /* 0x30 */ CInputEvent m_events[NUM_INPUT_EVENTS];  // initialized from masterEventTable

    int UpdateActuators(float);  // 0x800842DC
    void Update();  // 0x800845EC
};

// size 0x2430
class CDeviceManager {
public:
    /* 0x0 */ CGCDevice m_devices[NUM_INPUT_DEVICES];

    CInputEvent* GetEvents(int dev) { return m_devices[dev].m_events; }

    void UpdateActuators(float);  // 0x80083F7C
    int SetActuatorState(int, long, long, float, float);  // 0x80083FD8
    CDeviceState* GetDeviceState(unsigned long);  // 0x80084158
    u8 IsReady(int);  // 0x800841B4
    void Update();  // 0x800841C0
    void Shutdown();  // 0x80084210
    void Init();  // 0x80084214
    ~CDeviceManager();  // 0x8008429C
    CDeviceManager();  // 0x800842D8
};

extern u32 masterEventTable[];  // keymap.cpp

#endif
