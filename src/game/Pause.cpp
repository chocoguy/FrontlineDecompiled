// Pause: the pause screen, the mission objectives listed on it, and the
// pause-screen cheat codes.
#include "game/Pause.h"
#include "game/font.h"
#include "game/sprite.h"
#include "game/Screen.h"
#include "game/scene.h"
#include "game/player.h"
#include "game/input.h"
#include "game/sound.h"
#include "game/toilet.h"
#include "game/pumhandler.h"
#include "game/GameMessage.h"
#include "game/uistypes.h"
#include "game/ShellMenu.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "string.h"

SObjectiveList theObjectiveList;
int g_PauseScreenLockedController = -1;
int g_FinalSinglePlayerController = -1;
bool g_unplugged;

// Buttons pressed on the pause screen (one bit each), for the cheat codes.
static char g_cheatSequence[9];
static u32 g_cheatSequenceIndex;

enum {
    MAX_CHEAT_SEQUENCE = 8,
};

// The pause-screen sound event (the volume goes in the last word).
static inline void PauseSound(int volume) {
    int sound[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, volume};
    AEMS_SendEvent(sound);
}

static inline void ClearObjectives() {
    theObjectiveList.m_count = 0;
    for (int i = 0; i < 100; i++) {
        theObjectiveList.m_entries[i].m_done = 0;
        theObjectiveList.m_entries[i].m_text = NULL;
        theObjectiveList.m_entries[i].m_length = 0;
    }
}

void Pause::SetState(PauseState state) {
    if (state != field_0x88) {
        switch (state) {
        case 3:
            m_studio.iSirenActivateScreen(0, 0);
            MUSIC_Pause(true);
            AEMS_Pause(true);
            break;
        case 4:
            SoundSetMasterVolume(0);
            m_volumeRate = 25.4f;
            MUSIC_Pause(false);
            AEMS_Pause(false);
            break;
        case 6:
            if (m_volume < 127.0f) {
                SoundSetMasterVolume(127);
            }
            break;
        }
    }
    field_0x88 = state;
}

bool GetObjectiveStatus(unsigned int number) {
    return theObjectiveList.m_entries[number - 1].m_done;
}

void SetObjectiveStatus(unsigned int number, bool done) {
    theObjectiveList.m_entries[number - 1].m_done = done;
    if (done) {
        g_Shell.CompletedAnotherObjective();
    }
}

void AddObjective(unsigned int number, unsigned int message) {
    u8 length = (int)message < umTable.m_count ? umTable.m_entries[message].m_length : 0;
    SObjective& objective = theObjectiveList.m_entries[number - 1];
    objective.m_text = GetUMString(message);
    objective.m_done = 0;
    objective.m_length = length;
    theObjectiveList.m_count++;
}

void Pause::PauseShutdown() {
    g_PauseScreenLockedController = -1;
    m_studio.iSirenShutDown();
}

void Pause::UnpauseGame() {
    g_PauseScreenLockedController = -1;
    MUSIC_Pause(false);
    AEMS_Pause(false);
    m_paused = 0;
    g_cheatSequenceIndex = 0;
}

bool Pause::bIsGamePaused() {
    return m_paused;
}

// Paused: the menu, then the objectives (a checkbox and the text per row).
void Pause::PauseDraw(unsigned long frames) {
    if (m_paused) {
        m_studio.iSirenDrawProcessFnc(frames);
        if (!field_0x94) {
            int y = 100;
            for (u32 i = 0; i < 100; i++) {
                int row = (u16)y;
                if (row >= 0x1A8) {
                    continue;
                }
                SObjective& objective = theObjectiveList.m_entries[i];
                if (m_pFont != NULL && objective.m_text != NULL) {
                    m_box[0].y = row + 1;
                    m_box[1].y = row + 1;
                    m_box[2].y = row + 0x18;
                    m_box[3].y = row + 0x18;
                    if (objective.m_done) {
                        m_pChecked->DrawPoly(m_box, 4, CSprite::POLY_TRIANGLE_FAN);
                    } else {
                        m_pUnchecked->DrawPoly(m_box, 4, CSprite::POLY_TRIANGLE_FAN);
                    }
                    if (objective.m_length < 50) {
                        m_pFont->SetColor(CColor(0xFF, 0xFF, 0xFF, 0xFF));
                        m_pFont->DrawText(56.0f, row + 6, objective.m_text);
                    }
                    y += 0x18;
                }
            }
        }
    } else if (field_0x88 >= 3 && field_0x88 < 6) {
        m_studio.iSirenDrawProcessFnc(frames);
    }
}

static inline bool ControllerReady(int player) {
    return g_inputMgr.CDeviceManager::IsReady(g_scene.GetPlayer(player)->m_controllerPort);
}

void Pause::PauseUpdate(unsigned long frames) {
    // The intro fades the sound in.
    if (field_0x88 >= 4 && field_0x88 < 6) {
        float seconds = 0.016683351f * frames;
        if (m_volume < 127.0f) {
            m_volume = MathFunClamp(m_volume + m_volumeRate * seconds, 0.0f, 127.0f);
        }
        SoundSetMasterVolume((int)m_volume);
    }

    // Start pauses; a controller coming unplugged pauses too.
    u8 pause = m_paused;
    u8 ready;
    if (!g_bInMultiplayerMode) {
        if (!g_scene.GetPlayer(0)->m_attachedKeepWeapon) {
            ready = 1;
        } else {
            ready = ControllerReady(0);
        }
    } else {
        ready = 1;
        for (int i = 0; i < g_NumPlayers; i++) {
            ready &= ControllerReady(i);
        }
    }
    if (ready) {
        if (field_0x88 == 0 || field_0x88 == 6) {
            for (int i = 0; i < g_NumPlayers; i++) {
                if (g_inputMgr.GetValue(g_scene.GetPlayer(i)->m_controllerPort, 0x79) != 0 && m_paused != 1) {
                    m_pausedBy = i;
                    pause = 1;
                    g_PauseScreenLockedController = g_scene.GetPlayer(i)->m_controllerPort;
                    break;
                }
            }
        }
    } else if (field_0x88 != 7 && (field_0x88 == 0 || field_0x88 == 6)) {
        pause = 1;
        for (int i = 0; i < g_NumPlayers; i++) {
            if (!ControllerReady(i)) {
                m_pausedBy = i;
                g_PauseScreenLockedController = g_scene.GetPlayer(i)->m_controllerPort;
            }
        }
    }

    if (pause && !m_paused) {
        m_studio.iSirenActivateScreen(0, 1);
        MUSIC_Pause(true);
        float sfx, music;
        SoundGetVolumes(sfx, music);
        PauseSound((int)(32767.0f * sfx));
        AEMS_Pause(true);
        m_paused = pause;
        g_cheatSequenceIndex = 0;
    } else if (m_paused || (field_0x88 >= 1 && field_0x88 < 6)) {
        m_studio.iSirenUpdate(frames);
    }

    // Pause-screen cheat codes, as sequences of button bits.
    static const char BLTSHIELD_CHEAT[6] = {0x08, 0x08, 0x04, 0x01, 0x20, 0x10};
    static const char IMMORTAILITY_CHEAT[8] = {0x08, 0x01, 0x08, 0x04, 0x20, 0x01, 0x10, (char)0x80};
    if (m_paused && !g_bInMultiplayerMode) {
        s8 buttons = (s8)(((g_inputMgr.GetValue(0, 0x84) & 1) << 1) | (g_inputMgr.GetValue(0, 0x85) & 1) |
                          ((g_inputMgr.GetValue(0, 0x86) & 1) << 2) | ((g_inputMgr.GetValue(0, 0x7F) & 1) << 3) |
                          ((g_inputMgr.GetValue(0, 0x82) & 1) << 4) | ((g_inputMgr.GetValue(0, 0x81) & 1) << 5) |
                          ((g_inputMgr.GetValue(0, 0x7B) & 1) << 6) | ((g_inputMgr.GetValue(0, 0x7C) & 1) << 7));
        if (buttons != 0 && g_cheatSequenceIndex < MAX_CHEAT_SEQUENCE) {
            g_cheatSequence[g_cheatSequenceIndex++] = buttons;
            g_cheatSequence[g_cheatSequenceIndex] = 0;
        }
        bool entered = false;
        if (strncmp(g_cheatSequence, BLTSHIELD_CHEAT, sizeof(BLTSHIELD_CHEAT)) == 0) {
            entered = true;
            g_cheats.field_0x8 = !g_cheats.field_0x8;
        }
        if (strncmp(g_cheatSequence, IMMORTAILITY_CHEAT, sizeof(IMMORTAILITY_CHEAT)) == 0) {
            entered = true;
            g_cheats.m_invincible = !g_cheats.m_invincible;
        }
        if (entered) {
            // Back to the game.
            g_cheatSequenceIndex = 0;
            g_cheatSequence[0] = 0;
            UISParam_t result;
            GameMessage(2, 0, 0, 0, NULL, &result);
        }
    }
}

void Pause::Reset() {
    m_paused = 0;
    ClearObjectives();
}

void Pause::PauseInit() {
    m_pausedBy = -1;
    m_paused = 0;
    field_0x95 = 0;
    m_studio.ISirenStartup(2, 8, 0x1E, 0xC8, 0x10);
    m_studio.iSirenLoadScreen(0, 1);
    // The D-Day intro (mission 6, stage 1) has its own screen.
    if (!g_bInMultiplayerMode && g_Shell.Get_currentMission() == 6 && g_Shell.Get_currentStage() == 1) {
        m_studio.iSirenLoadScreen(0, 0);
    }
    _spritepolyvert box[4] = {
        {0.0f, 0.0f, 28.0f, 96.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {1.0f, 0.0f, 52.0f, 96.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {1.0f, 1.0f, 52.0f, 120.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {0.0f, 1.0f, 28.0f, 120.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    };
    memcpy(m_box, box, sizeof(m_box));
    CScreen* screen = CScreen::GetCurrent();
    if (screen != NULL) {
        LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
        void* fontData = TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(const char, level->fields[44].value), NULL);
        m_pFont = new CFont((const char*)fontData, (ERenderPriority)0x6F);
        m_pFont->SetColor(CColor(0x3F, 0x3F, 0x3F, 0x80));
        m_pFont->SetScale(1.0f, 1.0f);
        m_pFont->SetDepth(screen->GetDepthBufferMax());
    }
    ClearObjectives();
    m_volume = 0.0f;
    m_volumeRate = 0.0f;
    field_0x88 = 0;
    m_pChecked = new CSprite("data\\bitmaps\\checkedbox.gsh", (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6E);
    m_pUnchecked = new CSprite("data\\bitmaps\\clearcheckbox.gsh", (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6E);
    g_PauseScreenLockedController = -1;
}

Pause::~Pause() {
    if (m_pUnchecked != NULL) {
        delete m_pUnchecked;
    }
    if (m_pChecked != NULL) {
        delete m_pChecked;
    }
    if (m_pFont != NULL) {
        delete m_pFont;
    }
}

Pause::Pause() : m_paused(0) {
    m_pFont = NULL;
    field_0x94 = 1;
    field_0x95 = 0;
}
