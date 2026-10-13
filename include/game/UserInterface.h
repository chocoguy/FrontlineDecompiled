// UserInterface: a player's HUD (compass and health meter, ammo count and
// icon, weapon name, hit direction markers; in multiplayer the radar,
// score, scoreboard and viewport edges), plus the level intro's typed
// landmark and time stamps. Hand-maintained.
#ifndef _GAME_USERINTERFACE_H_
#define _GAME_USERINTERFACE_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"

class CPlayerObject;
class CSprite;
class CFont;

// Element display modes (m_healthMode, m_ammoMode).
enum {
    UI_SHOW_FADE = 0,  // shows for a while after a change, then fades
    UI_SHOW_PULSE = 1,  // pulses (low health)
};

// size 0x824
class UserInterface {
public:
    /* 0x0 */ u8 unk_0x0[0x8];
    /* 0x8 */ f32 m_northX;  // the compass's reference direction
    /* 0xC */ f32 m_northY;
    /* 0x10 */ f32 m_northZ;
    /* 0x14 */ u8 unk_0x14[0x790];
    /* 0x7A4 */ u8 f7A4_7 : 1;
    /* 0x7A4 */ u8 f7A4_6 : 1;
    /* 0x7A4 */ u8 f7A4_5 : 1;
    /* 0x7A4 */ u8 f7A4_4 : 1;
    /* 0x7A4 */ u8 f7A4_low : 4;
    /* 0x7A5 */ u8 unk_0x7A5[0x3];
    /* 0x7A8 */ f32 m_hitTime[4];  // per direction (AddHitDirection 1..4)
    /* 0x7B8 */ u8 unk_0x7B8[0x4];
    /* 0x7BC */ u32 m_lastHealth;  // for the health meter's mode
    /* 0x7C0 */ f32 m_alpha;  // the whole HUD (1 - m_fadeIn)
    /* 0x7C4 */ f32 m_fadeIn;  // > 0 while the HUD fades in
    /* 0x7C8 */ f32 m_healthAlpha;
    /* 0x7CC */ f32 m_ammoAlpha;
    /* 0x7D0 */ f32 m_nameAlpha;  // the weapon name
    /* 0x7D4 */ s32 m_healthMode;
    /* 0x7D8 */ s32 m_ammoMode;
    /* 0x7DC */ f32 m_healthTime;  // seconds the health meter stays up
    /* 0x7E0 */ f32 m_ammoTime;
    /* 0x7E4 */ f32 m_nameTime;
    /* 0x7E8 */ f32 m_pulse;  // phase (0..2pi)
    /* 0x7EC */ f32 field_0x7EC;
    /* 0x7F0 */ u8 m_alwaysShow;
    /* 0x7F1 */ u8 unk_0x7F1[0x3];
    /* 0x7F4 */ f32 m_heartbeat;  // seconds to the next low-health rumble
    /* 0x7F8 */ f32 m_compassSpeed;
    /* 0x7FC */ f32 m_compassAngle;
    /* 0x800 */ u8 m_letterbox;  // cinematic bars instead of the HUD (cleared when a level (re)starts)
    /* 0x801 */ u8 unk_0x801[0x3];
    /* 0x804 */ s32 m_lastReserve;
    /* 0x808 */ s32 m_lastClip;
    /* 0x80C */ u32 m_lastWeapon;
    /* 0x810 */ s32 m_lastHealthShown;
    /* 0x814 */ u8 m_compassFlip;  // the level's map faces the other way
    /* 0x815 */ u8 unk_0x815[0x3];
    /* 0x818 */ u32 m_playerIndex;  // g_scene player
    /* 0x81C */ f32 m_mpTimeRemaining;  // minutes
    /* 0x820 */ f32 m_mpTimeWarning;  // set to 5 at the 5 and 1 minute marks
    int AddHitDirection(unsigned int);  // 0x800BA8C0
    void DrawRadar();  // 0x800BB1AC
    void Update(float);  // 0x800BB6F4
    void DrawHitStatus();  // 0x800BBBC0
    void DrawCompass();  // 0x800BBEA0
    void UpdateCompassRotation(CPlayerObject*, float);  // 0x800BCBCC
    void DrawBulletStats();  // 0x800BCE58
    void DrawWeaponName(const char*);  // 0x800BD590
    void DrawFragCount(bool);  // 0x800BD864
    void DrawViewportEdge();  // 0x800BE108
    void DrawScore();  // 0x800BE590
    void Draw(float);  // 0x800BEB24
    void Init();  // 0x800BEE4C
    ~UserInterface();  // 0x800BF2D4
    UserInterface();  // 0x800BF3EC

    static CSprite* headshotImages[20];  // 0x80188CC0 (multiplayer characters)
    static CSprite* healthSprite;  // 0x8034EBC0
    static CSprite* compassSprite;  // 0x8034EBC4
    static CSprite* ammoSprite;  // 0x8034EBC8
    static CSprite* compassEdgeSprite;  // 0x8034EBCC
    static CSprite* hitMeterSprite;  // 0x8034EBD0
    static CSprite* blackSprite;  // 0x8034EBD4
    static CSprite* dotSprite;  // 0x8034EBD8
    static CSprite* greySprite;  // 0x8034EBDC
    static CSprite* starSprite;  // 0x8034EBE0
    static CSprite* circleSprite;  // 0x8034EBE4
    static CFont* bulletText;  // 0x8034EBE8
    static CFont* smallMPFont;  // 0x8034EBEC
};
OFFSET_ASSERT(UserInterface, m_lastHealth, 0x7BC);
OFFSET_ASSERT(UserInterface, m_letterbox, 0x800);
SIZE_ASSERT(UserInterface, 0x824);

// Free functions
void DisplayTimeStamp(float);  // 0x800BA8E4
void DisplayLandmarkStamp(float);  // 0x800BAB5C
void UpdateXFilesText(float);  // 0x800BADE0
void SetXFilesTextOnscreenTime(float);  // 0x800BAEC4
void SetTimeStampString(char*);  // 0x800BAECC
void SetLandmarkStampString(char*);  // 0x800BAF58
void ResetTimeStampStuff();  // 0x800BAFDC
void InitTimeStampStuff();  // 0x800BB098

#endif
