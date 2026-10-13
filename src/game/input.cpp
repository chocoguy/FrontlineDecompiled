// Input events: each frame, every port's buttons and axes are mapped to event
// values through the active KEYMAP.
#include "game/input.h"
#include "game/MathFun.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "ea/sys/pad.h"

CInputManager g_inputMgr;

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
