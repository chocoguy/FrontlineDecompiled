// Controller ports: reading the pads each frame and driving rumble.
#include "game/devicemgr.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "game/mainloop.h"
#include "ea/sys/pad.h"
#include "math.h"
#include "string.h"

// Analog slots combined into ANALOG_AB and ANALOG_TRIGGERS.
static u8 s_minus_buttons[2] = { ANALOG_B, ANALOG_TRIGGER_L };
static u8 s_plus_buttons[2] = { ANALOG_A, ANALOG_TRIGGER_R };

static u32 g_connectedBits;

void CGCDevice::Update() {
    PADStatus* pad = PAD_getdataptr(m_port);
    u32 mask = 0x80000000 >> m_port;

    switch (pad->err) {
    case PAD_ERR_NONE:
    case PAD_ERR_TRANSFER:
        g_connectedBits |= mask;
        m_ready = 1;
        break;
    case PAD_ERR_NO_CONTROLLER:
        PADReset(mask);
        if (mask & g_connectedBits) {
            m_ready = 0;
        }
        break;
    default:
        m_ready = 0;
        break;
    }

    if (m_ready) {
        m_state.buttons = pad->button;

        // Scale the sticks to roughly +-128, boosting diagonals (the pad's
        // range is octagonal) by up to 1.8x.
        f32 x = 1.76 * pad->stickX;
        f32 y = 1.76 * -pad->stickY;
        f32 cx = 2.15 * pad->substickX;
        f32 cy = 2.15 * -pad->substickY;
        f32 ratio;
        if (fabs(x) > fabs(y)) {
            ratio = fabs(y) / fabs(x);
        } else if (y != 0.0f) {
            ratio = fabs(x) / fabs(y);
        } else {
            ratio = 1.0f;
        }
        x = x * (0.8 * ratio + 1.0);
        y = y * (0.8 * ratio + 1.0);
        if (fabs(cx) > fabs(cy)) {
            ratio = fabs(cy) / fabs(cx);
        } else if (cy != 0.0f) {
            ratio = fabs(cx) / fabs(cy);
        } else {
            ratio = 1.0f;
        }
        cx = cx * (0.8 * ratio + 1.0);
        cy = cy * (0.8 * ratio + 1.0);

        m_state.analog[ANALOG_STICK_X] = 128.0f + x;
        m_state.analog[ANALOG_STICK_Y] = 128.0f + y;
        m_state.analog[ANALOG_SUBSTICK_X] = 128.0f + cx;
        m_state.analog[ANALOG_SUBSTICK_Y] = 128.0f + cy;
        m_state.analog[ANALOG_TRIGGER_L] = 1.7 * pad->triggerLeft;
        m_state.analog[ANALOG_TRIGGER_R] = 1.7 * pad->triggerRight;
        if (m_state.analog[ANALOG_TRIGGER_L] > 0x20) {
            m_state.buttons |= 0x40;
        }
        if (m_state.analog[ANALOG_TRIGGER_R] > 0x20) {
            m_state.buttons |= 0x20;
        }
        m_state.analog[ANALOG_A] = pad->analogA * 0x11;
        m_state.analog[ANALOG_B] = pad->analogB * 0x11;
        m_state.analog[ANALOG_AB] =
            ((m_state.analog[s_plus_buttons[0]] - m_state.analog[s_minus_buttons[0]]) >> 1) + 0x80;
        m_state.analog[ANALOG_TRIGGERS] =
            ((m_state.analog[s_plus_buttons[1]] - m_state.analog[s_minus_buttons[1]]) >> 1) + 0x80;
    }
}

// Rumble is pulsed: the motor runs for onTime out of every 0.1s, onTime
// scaling with the wanted strength. Returns 0 when the pad or rumble is idle.
int CGCDevice::UpdateActuators(float frames) {
    u8 changed = 0;
    float dt = frames / 60.0f;
    CActuator& act = m_actuators[0];

    if (PAD_getdataptr(m_port)->err != PAD_ERR_NONE) {
        return 0;
    }
    if (g_Pause.bIsGamePaused() || (g_bInMultiplayerMode && g_bMPEndLevel)) {
        if (act.motorOn) {
            act.motorOn = 0;
            PADControlMotor(m_port, PAD_MOTOR_STOP_HARD);
        }
        return 0;
    }
    if (!act.active) {
        return 0;
    }

    act.timeLeft -= dt;
    if (act.timeLeft <= 0.0f || act.type == 0) {
        changed = 1;
        act.active = 0;
        act.motorOn = 0;
    } else if (act.onTime > 0.0f) {
        act.onTime -= dt;
        if (act.onTime <= 0.0f) {
            act.onTime = 0.0f;
            if (act.offTime > 0.0f) {
                changed = 1;
                act.motorOn = 0;
            }
        }
    } else {
        act.offTime -= dt;
        if (act.offTime <= 0.0f) {
            float level;
            switch (act.type) {
            case 1: {
                float range = 180.0f * act.strength;
                level = (u32)range + 75 - (u32)(range * (act.timeLeft / act.duration));
                break;
            }
            case 2:
                level = (u32)(180.0f * act.strength * (act.timeLeft / act.duration)) + 75;
                break;
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
                level = act.strength;
                break;
            default:
                level = act.strength;
                break;
            }
            if (level > 230.0f) {
                level = 255.0f;
            }
            if (level < 25.0f) {
                level = 0.0f;
            }
            changed = 1;
            act.onTime = 0.0003921568627450981 * level;  // 1/2550
            act.offTime = 0.1 - act.onTime;
            act.motorOn = 1;
        }
    }

    if (changed) {
        if (act.motorOn == 1) {
            PADControlMotor(m_port, PAD_MOTOR_RUMBLE);
        } else if (act.type == 0) {
            PADControlMotor(m_port, PAD_MOTOR_STOP_HARD);
        } else {
            PADControlMotor(m_port, PAD_MOTOR_STOP);
        }
    }
    return 1;
}

CDeviceManager::CDeviceManager() {}

CDeviceManager::~CDeviceManager() {}

void CDeviceManager::Init() {
    for (int i = 0; i < NUM_INPUT_DEVICES; i++) {
        CGCDevice& dev = m_devices[i];
        dev.m_port = i;
        memset(dev.m_state.analog, 0, 0xC);
        dev.m_ready = 0;
        memcpy(dev.m_events, masterEventTable, sizeof(dev.m_events));
    }
}

void CDeviceManager::Shutdown() {}

void CDeviceManager::Update() {
    PAD_update();
    for (int i = 0; i < NUM_INPUT_DEVICES; i++) {
        m_devices[i].Update();
    }
}

u8 CDeviceManager::IsReady(int dev) {
    return m_devices[dev].m_ready;
}

CDeviceState* CDeviceManager::GetDeviceState(unsigned long dev) {
    if (PAD_getdataptr(m_devices[dev].m_port)->err == PAD_ERR_NONE) {
        return &m_devices[dev].m_state;
    }
    return NULL;
}

// type: see CActuator; strength is 0..1 for type 13, 0..255 for 1 and 2.
int CDeviceManager::SetActuatorState(int dev, long type, long motor, float strength,
                                     float duration) {
    switch (type) {
    case 0:
        if (motor >= 1) {
            break;
        }
        m_devices[dev].m_actuators[motor].strength = 0.0f;
        goto set;
    case 1:
    case 2:
        motor = 0;
        m_devices[dev].m_actuators[0].strength = strength;
        goto set;
    case 9:
        motor = 0;
        m_devices[dev].m_actuators[0].strength = 100.0f;
        goto set;
    case 10:
        motor = 0;
        m_devices[dev].m_actuators[0].strength = 177.0f;
        goto set;
    case 11:
    case 12:
        motor = 0;
        m_devices[dev].m_actuators[0].strength = 255.0f;
        goto set;
    case 13:
        if (motor >= 1) {
            break;
        }
        m_devices[dev].m_actuators[motor].strength = (u32)(255.0f * strength);
    set:
        m_devices[dev].m_actuators[motor].type = type;
        m_devices[dev].m_actuators[motor].duration = duration;
        m_devices[dev].m_actuators[motor].timeLeft = duration;
        m_devices[dev].m_actuators[motor].onTime = 0.0f;
        m_devices[dev].m_actuators[motor].offTime = 0.0f;
        m_devices[dev].m_actuators[motor].active = 1;
        break;
    }
    return 1;
}

void CDeviceManager::UpdateActuators(float frames) {
    for (int i = 0; i < NUM_INPUT_DEVICES; i++) {
        m_devices[i].UpdateActuators(frames);
    }
}
