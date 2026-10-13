// Jaw motion for talking soldiers.
#include "game/jaw_controller.h"

#include "game/MathFun.h"
#include "math.h"

void CJawController::SetMotionType(EJawType type) {
    m_type = type;
    switch (m_type) {
    case 5:
        m_openTime = 0.4f;
        m_closeTime = 1.0f;
        m_pauseTime = 5.0f;
        m_minOpen = 0.0f;
        m_maxOpen = 1.0f;
        m_numCycles = 10;
        m_prevMode = m_mode;
        m_mode = 6;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 2.0f;
        m_timer = 2.0f;
        m_waiting = 1;
        return;
    case 6:
        m_openTime = 6.0f;
        m_closeTime = 8.0f;
        m_pauseTime = 4.0f;
        m_minOpen = 0.0f;
        m_maxOpen = 0.4f;
        m_numCycles = MathFunRandomUI32Raw() % 5 + 5;
        m_prevMode = m_mode;
        m_mode = 1;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 2.0f;
        m_timer = 2.0f;
        m_waiting = 1;
        return;
    case 2:
        m_openTime = 0.6f;
        m_closeTime = 0.4f;
        m_pauseTime = 0.2f;
        m_minOpen = 0.1f;
        m_maxOpen = 0.5f;
        m_numCycles = MathFunRandomUI32Raw() % 10 + 5;
        m_prevMode = m_mode;
        m_mode = 1;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 2.0f;
        m_timer = 2.0f;
        m_waiting = 1;
        return;
    case 3:
        m_openTime = 0.4f;
        m_closeTime = 0.4f;
        m_pauseTime = 0.1f;
        m_minOpen = 0.15f;
        m_maxOpen = 0.5f;
        m_numCycles = MathFunRandomUI32Raw() % 10 + 5;
        m_prevMode = m_mode;
        m_mode = 9;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 1.0f;
        m_timer = 1.0f;
        m_waiting = 1;
        return;
    case 7:
        m_openTime = 0.4f;
        m_closeTime = 0.4f;
        m_pauseTime = 0.1f;
        m_minOpen = 0.15f;
        m_maxOpen = 0.5f;
        m_numCycles = 10;
        m_prevMode = m_mode;
        m_mode = 9;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 0.1f;
        m_timer = 0.1f;
        m_waiting = 1;
        return;
    case 4:
        m_openTime = 0.2f;
        m_closeTime = 0.5f;
        m_pauseTime = 0.0f;
        m_minOpen = 0.2f;
        m_maxOpen = 0.8f;
        m_numCycles = 6;
        m_prevMode = m_mode;
        m_mode = 9;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 1.5f;
        m_timer = 1.5f;
        m_waiting = 1;
        return;
    case 1:
        m_openTime = 0.15f;
        m_closeTime = 5.0f;
        m_pauseTime = 0.0f;
        m_minOpen = 0.0f;
        m_maxOpen = 0.7f;
        m_numCycles = 1;
        m_prevMode = m_mode;
        m_mode = 9;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 0.0f;
        m_timer = 0.0f;
        m_waiting = 1;
        return;
    default:
        m_openTime = 0.0f;
        m_closeTime = 86400.0f;
        m_pauseTime = 0.0f;
        m_minOpen = 0.0f;
        m_maxOpen = 0.0f;
        m_numCycles = 1;
        m_prevMode = m_mode;
        m_mode = 9;
        m_cyclesLeft = m_numCycles;
        m_waitTime = 0.0f;
        m_timer = 0.0f;
        m_waiting = 0;
        return;
    }
}

void CJawController::Update(int frames) {
    m_open = 0.0f;
    if (m_timer <= 0.0f && m_numCycles == 0) {
        return;
    }
    m_timer -= frames / 59.94f;
    if (m_waiting) {
        if (m_timer <= 0.0f) {
            m_waiting = 0;
        }
    } else if (m_timer <= 0.0f) {
        if (m_cyclesLeft != 0) {
            m_closeStart = m_closeTime;
            m_openStart = m_closeStart + m_openTime;
            m_timer = m_openStart + m_pauseTime;
            m_cyclesLeft--;
        } else if (m_type == 4) {
            m_closeStart = 86400.0f;
            m_openStart = 0.5f + m_closeStart;
            m_timer = 0.5f + m_openStart;
            m_numCycles = 1;
            m_cyclesLeft = 1;
            m_maxOpen = 0.5f;
        } else if (m_type == 1) {
            m_timer = 86400.0f;
            m_waiting = 1;
        } else {
            m_cyclesLeft = m_numCycles;
            m_timer = m_waitTime;
            m_waiting = 1;
        }
    } else if (m_timer > m_closeStart && m_timer < m_openStart) {
        m_open = 1.0f - (m_timer - m_closeStart) / (m_openStart - m_closeStart);
    } else if (m_timer <= m_closeStart) {
        m_open = m_timer / m_closeStart;
    }
    m_open *= (float)sin(0.017453292f * (90.0f * m_open));
    m_open = m_open * (m_maxOpen - m_minOpen) + m_minOpen;
}

CJawController::CJawController() {
    m_closeStart = 0.0f;
    m_openStart = 0.0f;
    m_openTime = 0.0f;
    m_closeTime = 0.0f;
    m_pauseTime = 0.0f;
    m_cyclesLeft = 0;
    m_numCycles = 0;
    m_minOpen = 0.0f;
    m_maxOpen = 0.0f;
    m_prevMode = -1;
    m_mode = 0;
    m_timer = 0.0f;
    m_open = 0.0f;
    m_waitTime = 0.0f;
    m_waiting = 1;
    m_type = (EJawType)0;
}
