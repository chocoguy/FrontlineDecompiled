// Pause: the in-game pause screen (an iSiren UI with the mission objectives
// drawn over it, and the pause-screen cheat codes), plus the scripted intro
// states that fade the sound in. Hand-maintained.
#ifndef _GAME_PAUSE_H_
#define _GAME_PAUSE_H_

#include "types.h"
#include "game_types.h"
#include "game/IStudio.h"
#include "game/sprite.h"

class CFont;
class CSprite;

// size 0x98
class Pause {
public:
    /* 0x0 */ CFont* m_pFont;  // objective text
    /* 0x4 */ CSprite* m_pChecked;  // completed objective's box
    /* 0x8 */ CSprite* m_pUnchecked;
    /* 0xC */ _spritepolyvert m_box[4];  // checkbox quad (y set per row)
    /* 0x6C */ u8 m_paused;
    /* 0x6D */ u8 unk_0x6D[0x3];
    /* 0x70 */ IStudio m_studio;
    // 0 normal play, 3 intro screen, 4 intro (sound fades in), 5 intro fade
    // out, 6 intro done, 7 no pausing.
    /* 0x88 */ s32 field_0x88;
    /* 0x8C */ f32 m_volume;  // master volume while fading in (0..127)
    /* 0x90 */ f32 m_volumeRate;  // per second
    /* 0x94 */ u8 field_0x94;  // set: don't draw the objectives
    /* 0x95 */ u8 field_0x95;  // the player quit from the pause screen
    /* 0x96 */ s8 m_pausedBy;  // player index that paused, -1 none
    /* 0x97 */ u8 unk_0x97[0x1];

    void SetState(PauseState);  // 0x800DD5A8
    void PauseShutdown();  // 0x800DD758
    void UnpauseGame();  // 0x800DD784
    bool bIsGamePaused();  // 0x800DD7D0
    void PauseDraw(unsigned long);  // 0x800DD7D8
    void PauseUpdate(unsigned long);  // 0x800DD98C
    void Reset();  // 0x800DDEBC
    void PauseInit();  // 0x800DDF78
    ~Pause();  // 0x800DE22C
    Pause();  // 0x800DE2E4
};
OFFSET_ASSERT(Pause, m_studio, 0x70);
OFFSET_ASSERT(Pause, field_0x88, 0x88);
OFFSET_ASSERT(Pause, m_pausedBy, 0x96);
SIZE_ASSERT(Pause, 0x98);

// A mission objective (numbered from 1 by the scripts). size 0x8
struct SObjective {
    /* 0x0 */ const char* m_text;  // user-message string
    /* 0x4 */ u8 m_length;  // text drawn only when shorter than 50
    /* 0x5 */ u8 m_done;
    /* 0x6 */ u8 unk_0x6[0x2];
};

// size 0x324
struct SObjectiveList {
    /* 0x0 */ int m_count;
    /* 0x4 */ SObjective m_entries[100];
};

// Free functions
bool GetObjectiveStatus(unsigned int);  // 0x800DD674
void SetObjectiveStatus(unsigned int, bool);  // 0x800DD68C
// Objective `number` shows user message `message`.
void AddObjective(unsigned int number, unsigned int message);  // 0x800DD6D4

extern SObjectiveList theObjectiveList;  // 0x801F70EC
extern Pause g_Pause;  // 0x80194FEC (Moh2.cpp)
extern bool g_unplugged;  // 0x8034EC9C
// Controller that opened the pause screen, or -1 for any.
extern int g_PauseScreenLockedController;  // 0x8034E2C8

extern int g_FinalSinglePlayerController;  // 0x8034E2CC
#endif
