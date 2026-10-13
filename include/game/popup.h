// PopUpMessage: one on-screen text message (position, color, how long it
// shows and how it scrolls into place). Hand-maintained.
#ifndef _GAME_POPUP_H_
#define _GAME_POPUP_H_

#include "types.h"
#include "game_types.h"

// Where a message sits (m_placement): 0/4 scroll down to m_top, 1/2 up.
// m_property: 1-3/7 settled, 4-6/8 still scrolling to the matching one.

// size 0x4C (bsbifunc's g_popUpMessage)
class PopUpMessage {
public:
    /* 0x0 */ u8 m_placement;
    /* 0x1 */ u8 unk_0x1[0x1];
    /* 0x2 */ u16 m_screenLocX;
    /* 0x4 */ u16 m_screenLocY;
    /* 0x6 */ u8 m_property;
    /* 0x7 */ u8 unk_0x7[0x1];
    /* 0x8 */ f32 m_onScreenTime;  // seconds left
    /* 0xC */ u16 m_textSize;  // line height
    /* 0xE */ u8 m_r;
    /* 0xF */ u8 m_g;
    /* 0x10 */ u8 m_b;
    /* 0x11 */ u8 m_a;
    /* 0x12 */ u16 m_top;
    /* 0x14 */ u16 m_bottom;
    /* 0x16 */ u16 field_0x16;
    /* 0x18 */ u8 m_available;
    /* 0x19 */ char m_string[0x33];  // SetString copies up to 0x32 chars
    void UpdateDisplayTime(float);  // 0x800BF4E8
    void Scroll();  // 0x800BF4F8
    void MoveTop();  // 0x800BF6EC
    char* GetString();  // 0x800BF768
    u16 Get_top();  // 0x800BF770
    u8 Get_a();  // 0x800BF778
    u8 Get_b();  // 0x800BF780
    u8 Get_g();  // 0x800BF788
    u8 Get_r();  // 0x800BF790
    float Get_onScreenTime();  // 0x800BF798
    u8 Get_property();  // 0x800BF7A0
    u16 Get_screenLocY();  // 0x800BF7A8
    u16 Get_screenLocX();  // 0x800BF7B0
    u8 Get_placement();  // 0x800BF7B8
    u8 Available();  // 0x800BF7C0
    void SetString(const char*);  // 0x800BF7C8
    void ChangeAvailability(unsigned char);  // 0x800BF7F0
    void Set_bottom(unsigned short);  // 0x800BF7F8
    void Set_top(unsigned short);  // 0x800BF800
    void Set_rgba(unsigned char, unsigned char, unsigned char, unsigned char);  // 0x800BF808
    void Set_textSize(unsigned char);  // 0x800BF81C
    void Set_onScreenTime(float);  // 0x800BF828
    void Set_property(unsigned char);  // 0x800BF830
    void Set_screenLocs(unsigned short, unsigned short);  // 0x800BF838
    void Set_placement(unsigned char);  // 0x800BF844
    void Reset();  // 0x800BF84C
    ~PopUpMessage();  // 0x800BF918
    PopUpMessage();  // 0x800BF954
};
OFFSET_ASSERT(PopUpMessage, m_available, 0x18);
SIZE_ASSERT(PopUpMessage, 0x4C);

#endif
