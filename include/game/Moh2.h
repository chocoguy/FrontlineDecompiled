// Moh2: main(), the per-frame game loop, the disc-error / reset screen, and
// the game's core global objects (scene, camera, screen, render list, shell,
// pause menu). Hand-maintained.
#ifndef _GAME_MOH2_H_
#define _GAME_MOH2_H_

#include "types.h"
#include "game_types.h"

struct CColor;
class CCamera;
class CFadeIn;
class CFog;
class CRenderList;
class CSprite;
class CViewport;

// Cheat settings for the current game (size 0x14).
struct CCheats {
    CCheats() {
        m_invincible = 0;
        field_0x1 = 0;
        field_0x2 = 0;
        field_0x3 = 0;
        field_0x4 = 0.0f;
        field_0x8 = 0;
        field_0x9 = 0;
        field_0xA = 0;
        field_0xB = 0;
        field_0xD = 0;
        field_0xE = 0;
        field_0xC = 0;
        field_0xF = 0;
        field_0x10 = 0;
    }

    /* 0x0 */ u8 m_invincible;  // player takes no damage
    /* 0x1 */ u8 field_0x1;
    /* 0x2 */ u8 field_0x2;
    /* 0x3 */ u8 field_0x3;
    /* 0x4 */ f32 field_0x4;  // > 0: slow motion (frame time scaled by 0.1)
    /* 0x8 */ u8 field_0x8;  // bullets spawn sparks off hearing volumes instead of whizzing
    /* 0x9 */ u8 field_0x9;
    /* 0xA */ u8 field_0xA;
    /* 0xB */ u8 field_0xB;
    /* 0xC */ u8 field_0xC;
    /* 0xD */ u8 field_0xD;
    /* 0xE */ u8 field_0xE;
    /* 0xF */ u8 field_0xF;
    /* 0x10 */ u8 field_0x10;
    /* 0x11 */ u8 unk_0x11[0x3];
};

// g_uiGameState bits
#define GAMESTATE_LEVEL_DONE 1  // leave the in-level loop
#define GAMESTATE_IN_SHELL 2
#define GAMESTATE_IN_LEVEL 4
#define GAMESTATE_NO_RETRY 8  // don't restart the level when it ends

// Free functions
int MohDVDErrorTask(int, int);  // 0x80016E80
void SetFog(CColor&, float, float);  // 0x80017DF8
float GetGameTime();  // 0x80017E2C
int GetFrameCount();  // 0x80017E60
void DebugMsg(const char*, ...);  // 0x80018C88
void SendRenderList();  // 0x8001A108
void WaitScreen();  // 0x8001A178
void WaitRenderList();  // 0x8001A280

extern u8 InternalFont[0x62B0];  // 0x8017B7E0 (font file used for the disc-error screen)
extern CCamera g_camera;  // 0x801949A0 (the camera between levels)
extern CFog g_fog;  // 0x80194C50
extern CRenderList theRenderList;  // 0x80194FBC
extern CFadeIn g_FadeIn;  // 0x80196A6C
extern CCheats g_cheats;  // 0x80196A94
extern float g_DefaultMotionBlur;  // 0x8034D960
extern float g_SlowMOHMotionBlur;  // 0x8034D964
extern u8* pInternalFont;  // 0x8034D974
extern u32 g_uiGameState;  // 0x8034E6A0
extern int g_updateTime;  // 0x8034E6B8

// Reset-button handling (see MohDVDErrorTask).
extern int resethappend;  // 0x8034E6F0 (frames since the reset button was pressed)
extern int DisableReset;  // 0x8034E6F4 (queue resets until cleared)
extern int DisableResetEntirley;  // 0x8034E6F8

#endif
