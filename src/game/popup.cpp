#include "game/popup.h"
#include "game/Screen.h"

#include <string.h>

// The defaults: centered near the top, five seconds, grey.
static inline void Defaults(PopUpMessage* m) {
    m->m_onScreenTime = 5.0f;
    m->m_screenLocX = 0x140;
    m->m_screenLocY = 12.0f * g_screen.m_heightScale;
    m->m_top = 12.0f * g_screen.m_heightScale;
    m->m_bottom = 88.0f * g_screen.m_heightScale;
    m->field_0x16 = 4;
    m->m_a = 0x23;
    m->m_b = 0x23;
    m->m_g = 0x23;
    m->m_r = 0x23;
    m->m_textSize = 0x16;
    m->m_property = 1;
    m->m_placement = 4;
    m->m_available = 1;
    memset(m->m_string, 0, 0x32);
}

void PopUpMessage::UpdateDisplayTime(float dt) {
    m_onScreenTime -= dt;
}

// The scrolling property that a settled message takes.
static inline void Settle(PopUpMessage* m) {
    switch (m->m_property) {
    case 4:
        m->m_property = 1;
        break;
    case 5:
        m->m_property = 2;
        break;
    case 6:
        m->m_property = 3;
        break;
    case 8:
        m->m_property = 7;
        break;
    }
}

// Moves the message to its row (settling its property) while it shows;
// resets it once its time is up.
void PopUpMessage::Scroll() {
    if (m_onScreenTime > 0.0f) {
        switch (m_placement) {
        case 0:
        case 4:
            m_screenLocY = m_top;
            if (m_screenLocY >= m_top) {
                m_screenLocY = m_top;
                Settle(this);
            }
            break;
        case 1:
        case 2:
            m_screenLocY = m_top;
            if (m_screenLocY <= m_top) {
                m_screenLocY = m_top;
                Settle(this);
            }
            break;
        }
    } else {
        Defaults(this);
    }
}

// Moves the row by one line (down for placements 0/4, up for 1/2).
void PopUpMessage::MoveTop() {
    switch (m_placement) {
    case 0:
        m_top += m_textSize;
        break;
    case 1:
        m_top -= m_textSize;
        break;
    case 2:
        m_top -= m_textSize;
        break;
    case 4:
        m_top += m_textSize;
        break;
    }
}

char* PopUpMessage::GetString() {
    return m_string;
}

u16 PopUpMessage::Get_top() {
    return m_top;
}

u8 PopUpMessage::Get_a() {
    return m_a;
}

u8 PopUpMessage::Get_b() {
    return m_b;
}

u8 PopUpMessage::Get_g() {
    return m_g;
}

u8 PopUpMessage::Get_r() {
    return m_r;
}

float PopUpMessage::Get_onScreenTime() {
    return m_onScreenTime;
}

u8 PopUpMessage::Get_property() {
    return m_property;
}

u16 PopUpMessage::Get_screenLocY() {
    return m_screenLocY;
}

u16 PopUpMessage::Get_screenLocX() {
    return m_screenLocX;
}

u8 PopUpMessage::Get_placement() {
    return m_placement;
}

u8 PopUpMessage::Available() {
    return m_available;
}

void PopUpMessage::SetString(const char* text) {
    strncpy(m_string, text, 0x32);
}

void PopUpMessage::ChangeAvailability(unsigned char available) {
    m_available = available;
}

void PopUpMessage::Set_bottom(unsigned short bottom) {
    m_bottom = bottom;
}

void PopUpMessage::Set_top(unsigned short top) {
    m_top = top;
}

void PopUpMessage::Set_rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    m_r = r;
    m_g = g;
    m_b = b;
    m_a = a;
}

void PopUpMessage::Set_textSize(unsigned char size) {
    m_textSize = size;
}

void PopUpMessage::Set_onScreenTime(float time) {
    m_onScreenTime = time;
}

void PopUpMessage::Set_property(unsigned char property) {
    m_property = property;
}

void PopUpMessage::Set_screenLocs(unsigned short x, unsigned short y) {
    m_screenLocX = x;
    m_screenLocY = y;
}

void PopUpMessage::Set_placement(unsigned char placement) {
    m_placement = placement;
}

void PopUpMessage::Reset() {
    Defaults(this);
}

PopUpMessage::~PopUpMessage() {}

PopUpMessage::PopUpMessage() {
    Defaults(this);
}
