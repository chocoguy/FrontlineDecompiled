// The game's movie entry points (rcmp_main.cpp): plays an RCMP movie full
// screen, skippable with any button. Hand-maintained.
#ifndef _MOVIE_RCMP_MAIN_H_
#define _MOVIE_RCMP_MAIN_H_

#include "types.h"
#include "game_types.h"
#include "movie/rcmp.h"
#include "movie/framework.h"

// size 0x48
class OPTION_PARSING_PLAYER {
public:
    /* 0x00 */ f32 m_speed;
    /* 0x04 */ f32 m_scaleX;
    /* 0x08 */ f32 m_scaleY;
    /* 0x0C */ f32 m_x;
    /* 0x10 */ f32 m_y;
    /* 0x14 */ f32 m_frameHeight;
    /* 0x18 */ f32 m_frameWidth;
    /* 0x1C */ f32 m_screenHeight;
    /* 0x20 */ f32 m_screenWidth;
    /* 0x24 */ u8 m_quit;  // a button was pressed
    /* 0x25 */ u8 m_playing;
    /* 0x26 */ u8 m_skippable;
    /* 0x28 */ int m_delay;
    /* 0x2C */ u8 m_noSkip;  // show every frame
    /* 0x2D */ u8 m_software;  // GCSW_VD into the frame buffer
    /* 0x2E */ u8 m_preload;
    /* 0x2F */ u8 m_sound;
    /* 0x30 */ u8 m_waitRightTime;  // wait for each frame's time
    /* 0x31 */ u8 m_stats;
    /* 0x32 */ u8 unk_0x32;
    /* 0x33 */ u8 unk_0x33;
    /* 0x34 */ u8 unk_0x34;
    /* 0x38 */ const char* m_videoName;
    /* 0x3C */ const char* m_audioName;
    /* 0x40 */ RCMP::AV_PLAYER* m_player;
    /* 0x44 */ RCMP::VIDEO_DRIVER* m_driver;

    OPTION_PARSING_PLAYER() {
        m_noSkip = 0;
        m_waitRightTime = 0;
        m_stats = 0;
        unk_0x32 = 0;
        m_preload = 0;
        m_sound = 0;
        m_software = 0;
        unk_0x33 = 0;
        unk_0x34 = 0;
        m_delay = 0;
        m_videoName = NULL;
        m_audioName = NULL;
        m_player = NULL;
        m_speed = 1.0f;
        m_scaleX = 1.0f;
        m_scaleY = 1.0f;
        m_x = 0.0f;
        m_y = 0.0f;
        m_frameHeight = 0.0f;
        m_frameWidth = 0.0f;
        m_screenWidth = 0.0f;
        m_screenHeight = 0.0f;
        m_quit = 0;
        m_driver = NULL;
    }
    ~OPTION_PARSING_PLAYER();
    void Play(bool skippable);  // 0x8010370C
    void ReadInput();  // 0x80103E94
};

void RCMP_PlayMovie(char* name, bool skippable);  // 0x8010408C
void RCMP_Shutdown();  // 0x8010422C
void RCMP_Initialize();  // 0x80104250

extern FRAMEWORK g_fw;  // 0x8034EDC8
extern OPTION_PARSING_PLAYER* CurentOPPlayer;  // 0x8034EDCC

#endif
