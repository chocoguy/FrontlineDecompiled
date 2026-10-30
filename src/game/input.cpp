// Input events: each frame, every port's buttons and axes are mapped to event
// values through the active KEYMAP.
#include "game/input.h"
#include "game/MathFun.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "ea/sys/pad.h"
#ifdef FRONTLINE_NATIVE
#include "game/GameMessage.h"
#include "game/Moh2.h"
#include "game/pumhandler.h"
#include "game/uistypes.h"
#include "native/hooks.h"
#endif

CInputManager g_inputMgr;

#ifdef FRONTLINE_NATIVE
// Keyboard and mouse (native/hooks.h): each frame the platform's game actions
// become input events on top of what the controller's keymap produced.
namespace {

struct NativeBinding {
    u32 action;
    u16 event;
};

// Always: the configurable actions (s_actionEvents in ConfigGC.cpp) and the
// fixed in-game events.
const NativeBinding kPlayBindings[] = {
    {NATIVE_ACTION_JUMP, 0xB},         {NATIVE_ACTION_CROUCH, 0xC},      {NATIVE_ACTION_USE, 0xD},
    {NATIVE_ACTION_FIRE, 0xE},         {NATIVE_ACTION_RELOAD, 0xF},      {NATIVE_ACTION_AIM, 0x10},
    {NATIVE_ACTION_NEXT_WEAPON, 0x13}, {NATIVE_ACTION_PREV_WEAPON, 0x14}, {NATIVE_ACTION_MELEE, 0x15},
    {NATIVE_ACTION_CENTER_VIEW, 0x16}, {NATIVE_ACTION_SCORES, 0x17},
};
// Only while playing: Start pauses, and D-pad right (the menus' right) asks
// HQ for a hint.
const NativeBinding kPlayingBindings[] = {
    {NATIVE_ACTION_PAUSE, 0x79},
    {NATIVE_ACTION_HINT, 0x7E},
};
// Only in menus: the UI's buttons (IStudio reads events 0x79.. as buttons 0..15).
const NativeBinding kMenuBindings[] = {
    {NATIVE_ACTION_MENU_START, 0x79}, {NATIVE_ACTION_MENU_UP, 0x7B}, {NATIVE_ACTION_MENU_DOWN, 0x7C},
    {NATIVE_ACTION_MENU_LEFT, 0x7D},  {NATIVE_ACTION_MENU_RIGHT, 0x7E}, {NATIVE_ACTION_MENU_B, 0x7F},
    {NATIVE_ACTION_MENU_A, 0x80},     {NATIVE_ACTION_MENU_Y, 0x81},     {NATIVE_ACTION_MENU_X, 0x82},
    {NATIVE_ACTION_MENU_Z, 0x84},     {NATIVE_ACTION_MENU_L, 0x85},     {NATIVE_ACTION_MENU_R, 0x86},
};

// Frames of scope zoom per wheel notch (the D-pad zooms 0.6 degrees a frame).
const int kScopeZoomFrames = 5;

NativeInput s_native[NUM_INPUT_DEVICES];
u32 s_nativeHeld[NUM_INPUT_DEVICES];  // last frame's, to find presses
int s_wheelPresses[NUM_INPUT_DEVICES];  // weapon switches still to send
int s_scopeZoom[NUM_INPUT_DEVICES];     // frames of scope zoom still to send

// In a level, not paused and past the intro screens: the mouse looks around
// and Start pauses. Otherwise the keys drive the menus.
bool Playing() {
    int state = g_Pause.field_0x88;
    return (g_uiGameState & GAMESTATE_IN_LEVEL) && !g_Pause.bIsGamePaused() && !(state >= 1 && state < 6);
}

void SetNativeEvents(CInputEvent* events, const NativeBinding* bindings, int count, u32 held, u32 pressed) {
    for (int i = 0; i < count; i++) {
        CInputEvent* e = &events[bindings[i].event];
        u32 on = (e->flags & INPUTEVENT_PRESS) ? pressed : held | pressed;
        if (on & bindings[i].action) {
            e->value = 1;
        }
    }
}

// Signed analog event `ev` driven by the keys too (-128..127, as from a stick).
void SetNativeAxis(CInputEvent* events, int ev, int value) {
    int pad = events[ev].value;
    if (value != 0 && (pad < 0 ? -pad : pad) < (value < 0 ? -value : value)) {
        events[ev].value = value;
    }
}

}  // namespace

const NativeInput& NativeFrameInput(int dev) {
    return s_native[dev];
}

static void ApplyNativeInput(CInputManager* mgr, u32 dev, CInputEvent* events) {
    const NativeInput& in = s_native[dev];
    u32 held = in.held;
    u32 pressed = in.pressed | (held & ~s_nativeHeld[dev]);
    s_nativeHeld[dev] = held;

    // The wheel switches weapons, one a frame, or zooms a scope in or out.
    if (mgr->m_dpadLocked[dev]) {
        s_wheelPresses[dev] = 0;
        s_scopeZoom[dev] += in.wheel * kScopeZoomFrames;
        s_scopeZoom[dev] = MathFunClamp<long>(s_scopeZoom[dev], -30, 30);
        if (s_scopeZoom[dev] != 0) {
            events[s_scopeZoom[dev] > 0 ? 0x8F : 0x8E].value = 1;
            s_scopeZoom[dev] += s_scopeZoom[dev] > 0 ? -1 : 1;
        }
    } else {
        s_scopeZoom[dev] = 0;
        s_wheelPresses[dev] = MathFunClamp<long>(s_wheelPresses[dev] + in.wheel, -3, 3);
        if (s_wheelPresses[dev] != 0) {
            pressed |= s_wheelPresses[dev] > 0 ? NATIVE_ACTION_PREV_WEAPON : NATIVE_ACTION_NEXT_WEAPON;
            s_wheelPresses[dev] += s_wheelPresses[dev] > 0 ? -1 : 1;
        }
    }

    // Pause again (Esc) continues, as the pause menu's Continue does. B would
    // move the menu's cursor to Quit.
    if ((pressed & NATIVE_ACTION_PAUSE) && (g_uiGameState & GAMESTATE_IN_LEVEL) && g_Pause.bIsGamePaused() &&
        (g_PauseScreenLockedController == -1 || g_PauseScreenLockedController == (int)dev)) {
        UISParam_t result;
        GameMessage(2, 0, 0, 0, NULL, &result);
        held &= ~NATIVE_ACTION_MENU_B;
        pressed &= ~(NATIVE_ACTION_PAUSE | NATIVE_ACTION_MENU_B);
    }

    SetNativeEvents(events, kPlayBindings, sizeof(kPlayBindings) / sizeof(kPlayBindings[0]), held, pressed);
    if (Playing()) {
        SetNativeEvents(events, kPlayingBindings, sizeof(kPlayingBindings) / sizeof(kPlayingBindings[0]), held,
                        pressed);
    } else {
        SetNativeEvents(events, kMenuBindings, sizeof(kMenuBindings) / sizeof(kMenuBindings[0]), held, pressed);
    }

    // Moving: forward is up on the stick, which reads negative.
    int full = (held & NATIVE_ACTION_WALK) ? 0x40 : 0x7F;
    int forward = ((held & NATIVE_ACTION_FORWARD) ? 1 : 0) - ((held & NATIVE_ACTION_BACK) ? 1 : 0);
    int strafe = ((held & NATIVE_ACTION_RIGHT) ? 1 : 0) - ((held & NATIVE_ACTION_LEFT) ? 1 : 0);
    SetNativeAxis(events, 7, -forward * full);
    SetNativeAxis(events, 8, strafe * full);
}
#endif

void CInputManager::Init() {
    CDeviceManager::Init();
    m_keymap = NULL;
    m_anyInput = 0;
    m_feedbackEnabled = 1;
    field_0x244A = 0;
    m_dpadLocked[0] = 0;
    m_dpadLocked[1] = 0;
    m_dpadLocked[2] = 0;
    m_dpadLocked[3] = 0;

    // Give the pads a chance to report in.
    for (int tries = 0; tries < 10000; tries++) {
        Update(0.0f);
        if (IsReady()) {
            break;
        }
    }

    m_playerKeymaps[0] = NULL;
    m_playerKeymaps[1] = NULL;
    m_playerKeymaps[2] = NULL;
    m_playerKeymaps[3] = NULL;
}

void CInputManager::Update(float frames) {
    m_anyInput = 0;
    CDeviceManager::Update();
#ifdef FRONTLINE_NATIVE
    for (int dev = 0; dev < NUM_INPUT_DEVICES; dev++) {
        NativeTakeInput(dev, dev == 0 && Playing(), &s_native[dev]);
    }
    NativePromptsUpdate();
#endif

    for (u32 dev = 0; (int)dev < NUM_INPUT_DEVICES; dev++) {
        CInputEvent* events = GetEvents(dev);
        CDeviceState* state = GetDeviceState(dev);

        if (events != NULL && state == NULL) {
            // No controller: drop everything.
            for (u32 i = 0; i < NUM_INPUT_EVENTS; i++) {
                events[i].prev = 0;
                events[i].value = 0;
            }
            continue;
        }

        for (u32 i = 0; i < NUM_INPUT_EVENTS; i++) {
            events[i].value = 0;
        }
        if (g_unplugged) {
            for (u32 i = 0; i < NUM_INPUT_EVENTS; i++) {
                events[i].prev = 0;
                events[i].value = 0;
            }
            continue;
        }

        if (g_bInMultiplayerMode) {
            m_keymap = m_playerKeymaps[dev];
        }
        if (m_keymap == NULL) {
            continue;
        }

        for (int k = 0;; k++) {
            u16 ev = m_keymap->entries[k].event;
            if (ev == 0xFFFF) {
                break;
            }
            u16 src = m_keymap->entries[k].source;
            if (src == 0xFFFF) {
                events[ev].value = 0;
                continue;
            }

            CInputEvent* e = &events[ev];
            u16 flags = e->flags;
            u32 analog = flags & INPUTEVENT_ANALOG;
            u8 raw;
            if (analog) {
                raw = state->analog[src];
            } else if (flags & INPUTEVENT_CHORD) {
                raw = ((state->buttons & (1 << (u8)src)) != 0) &
                      ((state->buttons & (1 << (u8)(src >> 8))) != 0);
            } else if (m_dpadLocked[dev] &&
                       ((src == 3 && ev != 0x8E) || (src == 2 && ev != 0x8F))) {
                e->value = 0;
                continue;
            } else {
                raw = (state->buttons & (1 << src)) != 0;
            }

            int value = raw;
            u32 isSigned = flags & INPUTEVENT_SIGNED;
            if (isSigned) {
                value -= 0x80;
            }

            if (analog) {
                // Dead zone, rescaled to the full range.
                u8 deadzone = e->deadzone;
                int max = 0xFF;
                if (isSigned) {
                    max = 0x80;
                }
                if (src >= 8) {
                    value *= 2;
                }
                if (value > 0) {
                    value = max * (value - deadzone) / (max - deadzone);
                    if (value < 0) {
                        value = 0;
                    }
                } else {
                    value = max * (value + deadzone) / (max - deadzone);
                    if (value > 0) {
                        value = 0;
                    }
                }
                if (isSigned) {
                    value = MathFunClamp<long>(value, -0x80, 0x7F);
                } else {
                    value = MathFunClamp<long>(value, 0, 0xFF);
                }
            }

            if (flags & INPUTEVENT_CUBIC) {
                int sign = 1;
                if (value < 0) {
                    sign = -1;
                    value *= -1;
                }
                if (analog) {
                    value = value * (value * (sign * value)) / (127 * 127);
                } else {
                    value = value * (value * (sign * value)) / (255 * 255);
                }
            }

            if (flags & INPUTEVENT_PRESS) {
                int prev = e->prev;
                u8 pressed = 0;
                e->prev = value;
                if (value != 0 && value != prev) {
                    pressed = 1;
                }
                value = pressed != 0;
            }

            if (value != 0) {
                m_anyInput = 1;
            }
            e->value = value;
        }
#ifdef FRONTLINE_NATIVE
        ApplyNativeInput(this, dev, events);
        if (s_native[dev].held != 0 || s_native[dev].pressed != 0) {
            m_anyInput = 1;
        }
#endif
    }

    CDeviceManager::UpdateActuators(frames);
}

void CInputManager::Shutdown() {
    CDeviceManager::Shutdown();
}

void CInputManager::SetKeymap(unsigned long player, KEYMAP* keymap) {
    if (g_bInMultiplayerMode) {
        m_playerKeymaps[player] = keymap;
        return;
    }
    m_keymap = keymap;
}

int CInputManager::GetValue(unsigned long dev, unsigned long event) {
    CInputEvent* events = GetEvents(dev);
    if (events != NULL) {
        return events[event].value;
    }
    return 0;
}

bool CInputManager::GetValueAnyPlayer(unsigned long event) {
    return GetValue(0, event) != 0 || GetValue(1, event) != 0 || GetValue(2, event) != 0 ||
           GetValue(3, event) != 0;
}

void CInputManager::ResetActuatorState(long type) {
    for (int dev = 0; dev < NUM_INPUT_DEVICES; dev++) {
        CDeviceManager::SetActuatorState(dev, type, 0, 0.0f, 1.0f);
        if (type == 0) {
            PADControlMotor(dev, PAD_MOTOR_STOP_HARD);
        }
    }
}

void CInputManager::SetFeedback(unsigned long player, long type, long motor, float strength,
                                float duration) {
    u8 enabled = 0;
    if (g_bInMultiplayerMode) {
        if (g_Shell.m_players[player].m_rumble) {
            enabled = 1;
        }
    } else {
        enabled = m_feedbackEnabled;
    }
    if (enabled && !m_feedbackBlocked[player]) {
        CDeviceManager::SetActuatorState(player, type, motor, strength, duration);
    }
}

bool CInputManager::IsReady() {
    for (int dev = 0; dev < NUM_INPUT_DEVICES; dev++) {
        if (CDeviceManager::IsReady(dev)) {
            return true;
        }
    }
    return false;
}

int CInputManager::VerifyAnalog() {
    return 1;
}

void CInputManager::Reset() {
    for (int dev = 0; dev < NUM_INPUT_DEVICES; dev++) {
        CInputEvent* events = GetEvents(dev);
        if (events != NULL) {
            for (int i = 0; i < NUM_INPUT_EVENTS; i++) {
                events[i].value = 0;
                events[i].prev = 0;
            }
        }
    }
}
