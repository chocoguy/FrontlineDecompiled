#include "game/UserInterface.h"
#include "game/player.h"
#include "game/player_weapon_object.h"
#include "game/weapon.h"
#include "game/scene.h"
#include "game/sprite.h"
#include "game/font.h"
#include "game/color.h"
#include "game/Screen.h"
#include "game/viewport.h"
#include "game/ShellMenu.h"
#include "game/Pause.h"
#include "game/input.h"
#include "game/sound.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/mainloop.h"
#include "game/pumhandler.h"
#include "game/bstypes.h"
#include "game/BSGO_Basic.h"
#include "game/MathFun.h"
#include "ea/sys/font.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

CSprite* UserInterface::headshotImages[20];
CSprite* UserInterface::healthSprite;
CSprite* UserInterface::compassSprite;
CSprite* UserInterface::ammoSprite;
CSprite* UserInterface::compassEdgeSprite;
CSprite* UserInterface::hitMeterSprite;
CSprite* UserInterface::blackSprite;
CSprite* UserInterface::dotSprite;
CSprite* UserInterface::greySprite;
CSprite* UserInterface::starSprite;
CSprite* UserInterface::circleSprite;
CFont* UserInterface::bulletText;
CFont* UserInterface::smallMPFont;

// The HUD's quads, in 640x448 screen units (the compass and the health
// meter around it are centered on (92, 384)).
_spritepolyvert LetterBoxVerts[8] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 6.4e+02f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 6.4e+02f, 85.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 0.0f, 85.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 0.0f, 0.0f, 362.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 6.4e+02f, 362.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 6.4e+02f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 0.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
};
_spritepolyvert CompassVerts[24] = {
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.0f, 92.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 156.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 156.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.5f, 156.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.5f, 156.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 156.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 156.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 1.0f, 92.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 1.0f, 92.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 28.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 28.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 0.5f, 28.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 0.5f, 28.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 0.0f, 28.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 0.0f, 28.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.5f, 0.0f, 92.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
};
_spritepolyvert StarSpriteVerts[4] = {
    {0.0f, 0.0f, -16.0f, -16.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 0.0f, 16.0f, -16.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 1.0f, 16.0f, 16.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {0.0f, 1.0f, -16.0f, 16.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
};
_spritepolyvert headshotImageVerts[4] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 0.0f, 32.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 1.0f, 32.0f, 32.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {0.0f, 1.0f, 0.0f, 32.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
};
_spritepolyvert DotSpriteVerts[4] = {
    {0.3f, 0.25f, -3.0f, -3.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.75f, 0.25f, 3.0f, -3.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.75f, 0.75f, 3.0f, 3.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.3f, 0.75f, -3.0f, 3.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
};
_spritepolyvert AmmoSpriteVerts[4] = {
    {0.0f, 0.0f, 506.0f, 3.8e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 554.0f, 3.8e+02f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 554.0f, 428.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 506.0f, 428.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
};
_spritepolyvert HitSpriteVerts[12] = {
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.0f, 0.0f, 28.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {1.0f, 0.0f, 156.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {1.0f, 0.0f, 156.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {1.0f, 1.0f, 156.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {1.0f, 1.0f, 156.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.0f, 1.0f, 28.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.5f, 0.5f, 92.0f, 384.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.0f, 1.0f, 28.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
    {0.0f, 0.0f, 28.0f, 3.2e+02f, 0.0f, {0x80, 0x80, 0x80, 0x60}},
};

// Team colors (red, yellow, green, blue).
CColor teamColors[4] = {
    CColor(0x60, 0x20, 0x20, 0x80),
    CColor(0x60, 0x60, 0x00, 0x80),
    CColor(0x00, 0x60, 0x00, 0x80),
    CColor(0x20, 0x20, 0x80, 0x80),
};
// Scratch vertices for whichever quad is being drawn.
_spritepolyvert TempVerts[24];

// The level intro's landmark and time stamps, typed out a character at a
// time, then faded.
char landmark[0x30];
char timestamp[0x30];
char landmarkDisplayText[0x30];
char timestampDisplayText[0x30];
float alpha = 128.0f;
float onscreenTime;
CFont* timestampFont;
int charMarkerForLandmarkString;
int charMarkerForTimeStampString;
bool bDisplayLandmarkStamp;
bool bDisplayTimeStamp;
bool bDoneParsingLandmark;
bool bDoneParsingTimeStamp;
bool bDoneFading;
bool bDoneShowingXFilesText;

// Hits from direction `dir` (1..4) show for three seconds.
int UserInterface::AddHitDirection(unsigned int dir) {
    if (dir < 1 || dir > 4) {
        return 0;
    }
    m_hitTime[dir - 1] = 3.0f;
    return 1;
}

// A typewriter key.
static inline void TypeSound() {
    int sample = (int)MathFunRandomReal(0.0f, 2.9999f);
    float left, right;
    SoundGetVolumes(left, right);
    int event[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, 0};
    event[2] = sample;
    event[9] = (int)(32767.0f * (((float)(g_Shell.field_0x28 - 1) / 6.0f) * 0.25f));
    AEMS_SendEvent(event);
}

// The time stamp line, a character every 0.028 seconds.
void DisplayTimeStamp(float dt) {
    static float frame = 0.0f;
    static float xxxTimeStampX = 40.0f;
    static float xxxTimeStampY = 316.0f * g_screen.m_heightScale;
    if (timestamp[0] == 0) {
        bDoneParsingTimeStamp = 1;
        return;
    }
    timestampFont->SetColor(CColor(0x80, 0x80, 0x80, (int)alpha));
    timestampFont->DrawText(xxxTimeStampX, xxxTimeStampY, timestampDisplayText);
    if (bDoneParsingTimeStamp) {
        return;
    }
    if (frame > 0.028) {
        int length = strlen(timestamp);
        if (charMarkerForTimeStampString > length) {
            charMarkerForTimeStampString = length;
            bDoneParsingTimeStamp = 1;
        }
        memcpy(timestampDisplayText, timestamp, charMarkerForTimeStampString);
        frame = 0.0f;
        charMarkerForTimeStampString++;
        if (charMarkerForTimeStampString % 2 == 0) {
            TypeSound();
        }
    } else {
        frame = 0.016683351f * dt + frame;
    }
}

// The landmark line; the time stamp starts once it is typed.
void DisplayLandmarkStamp(float dt) {
    static float frame = 0.0f;
    static float xxxLandmarkX = 40.0f;
    static float xxxLandmarkY = 296.0f * g_screen.m_heightScale;
    if (landmark[0] == 0) {
        bDisplayTimeStamp = 1;
        bDoneParsingLandmark = 1;
        return;
    }
    timestampFont->SetColor(CColor(0x80, 0x80, 0x80, (int)alpha));
    timestampFont->DrawText(xxxLandmarkX, xxxLandmarkY, landmarkDisplayText);
    if (bDoneParsingLandmark) {
        return;
    }
    if (frame > 0.028) {
        int length = strlen(landmark);
        if (charMarkerForLandmarkString > length) {
            charMarkerForLandmarkString = length;
            bDisplayTimeStamp = 1;
            bDoneParsingLandmark = 1;
        }
        memcpy(landmarkDisplayText, landmark, charMarkerForLandmarkString);
        frame = 0.0f;
        charMarkerForLandmarkString++;
        // (The original tests the time stamp's marker here.)
        if (charMarkerForTimeStampString % 2 == 0) {
            TypeSound();
        }
    } else {
        frame = 0.016683351f * dt + frame;
    }
}

// Types the stamps, then fades them out.
void UpdateXFilesText(float dt) {
    if (bDoneShowingXFilesText) {
        return;
    }
    if (bDisplayLandmarkStamp) {
        DisplayLandmarkStamp(dt);
    }
    if (bDisplayTimeStamp) {
        DisplayTimeStamp(dt);
    }
    if (bDoneParsingTimeStamp && !bDoneFading) {
        static float xxxFadeValue = 21.0f;
        alpha = alpha - 0.016683351f * (xxxFadeValue * dt);
        if (alpha < 0.0f) {
            bDoneFading = 1;
            bDoneShowingXFilesText = 1;
        }
        if (0.0f == alpha) {
            bDoneFading = 0;
            bDoneShowingXFilesText = 0;
            alpha = 63.0f;
        }
    }
}

void SetXFilesTextOnscreenTime(float time) {
    onscreenTime = time;
}

// A single space means none.
void SetTimeStampString(char* text) {
    if (text != NULL) {
        strcpy(timestamp, text);
        if (strcmp(text, " ") == 0) {
            bDoneParsingTimeStamp = 1;
            timestamp[0] = 0;
            bDoneFading = 1;
            bDoneShowingXFilesText = 1;
        }
    } else {
        bDoneParsingTimeStamp = 1;
        timestamp[0] = 0;
        bDoneFading = 1;
        bDoneShowingXFilesText = 1;
    }
}

void SetLandmarkStampString(char* text) {
    if (text != NULL) {
        strcpy(landmark, text);
        bDisplayLandmarkStamp = 1;
        if (strcmp(text, " ") == 0) {
            bDoneParsingLandmark = 1;
            landmark[0] = 0;
        }
    } else {
        bDoneParsingLandmark = 1;
        landmark[0] = 0;
    }
}

static inline void ClearStampText() {
    memset(landmark, 0, sizeof(landmark));
    memset(timestamp, 0, sizeof(timestamp));
    memset(landmarkDisplayText, 0, sizeof(landmarkDisplayText));
    memset(timestampDisplayText, 0, sizeof(timestampDisplayText));
}

// (The font is deleted but its pointer kept, as in the original.)
void ResetTimeStampStuff() {
    if (timestampFont != NULL) {
        delete timestampFont;
    }
    onscreenTime = 0.0f;
    ClearStampText();
    charMarkerForLandmarkString = 0;
    charMarkerForTimeStampString = 0;
    bDisplayLandmarkStamp = 0;
    bDisplayTimeStamp = 0;
    bDoneParsingLandmark = 0;
    bDoneParsingTimeStamp = 0;
    bDoneFading = 0;
    bDoneShowingXFilesText = 0;
}

// The stamps' font is the level's subtitle font.
void InitTimeStampStuff() {
    charMarkerForLandmarkString = 0;
    ClearStampText();
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    void* font = TLT_LoadFileFromLevelBigFile((char*)level->fields[2].value, NULL);
    timestampFont = new CFont((const char*)font, (ERenderPriority)0x6F);
    timestampFont->SetColor(CColor(0x80, 0x80, 0x80, 0x80));
    timestampFont->SetScale(1.0f, 1.0f);
    CScreen* screen = CScreen::GetCurrent();
    if (screen != NULL) {
        timestampFont->SetDepth(screen->GetDepthBufferMax());
    }
}

// The script health of `player` (0 without script data).
static inline int ScriptHealth(CPlayerObject* player) {
    BSObject* script = player->GetScriptObject();
    int* data = NULL;
    if (script != NULL) {
        data = (int*)script->m_gameObject->GetScriptData();
    }
    if (data != NULL) {
        return data[0];
    }
    return 0;
}

// Multiplayer radar (when the player has it on): a dot per living player
// around the compass, others placed by direction and distance (up to 50
// units), in their team colors.
void UserInterface::DrawRadar() {
    CPlayerObject* me = g_scene.GetPlayer(m_playerIndex);
    if (g_bInMultiplayerMode && !g_Shell.m_players[me->m_controllerPort].m_radar) {
        return;
    }
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    CVector3 forward;
    me->GetForward(forward);
    CVector3 pos;
    me->GetPosition(pos);
    CVector3 right;
    me->GetRightward(right);
    float centerX = 92.0f * vpWidth;
    float centerY = 384.0f * vpHeight;
    for (int i = 0; i < g_NumPlayers; i++) {
        CPlayerObject* other = g_scene.GetPlayer(i);
        if (other->m_dead) {
            continue;
        }
        float y = ((float)height * g_viewport.m_top + centerY) - 16.0f;
        float x = (float)width * g_viewport.m_left + centerX;
        memcpy(TempVerts, DotSpriteVerts, sizeof(DotSpriteVerts));
        u8* color = (u8*)&teamColors[other->m_team];
        _spritepolyvert* v = TempVerts;
        for (int k = 0; k < 4; k++, v++) {
            if (other == me) {
                v->x = v->x + x;
                v->y = v->y + y;
            } else {
                CVector3 d;
                other->GetPosition(d);
                d.x = d.x - pos.x;
                d.y = d.y - pos.y;
                d.z = d.z - pos.z;
                float dist = d.Length();
                d.Normalize();
                float r;
                if (dist >= 50.0f) {
                    r = 34.0f;
                } else {
                    r = 34.0f * (dist / 50.0f);
                }
                float angle = MathFunAtan2F(forward.x * d.x + forward.y * d.y + forward.z * d.z,
                                            right.x * d.x + right.y * d.y + right.z * d.z);
                if (g_NumPlayers > 2) {
                    d.x = 0.6666f * (r * (float)cos(angle));
                    d.y = 0.6666f * (r * (float)sin(angle));
                } else {
                    d.x = r * (float)cos(angle);
                    d.y = r * (float)sin(angle);
                }
                v->x = v->x + (x + d.x);
                v->y = v->y + (y + d.y);
            }
            v->color[0] = color[0];
            v->color[1] = color[1];
            v->color[2] = color[2];
            v->color[3] = (int)(m_alpha * (m_healthAlpha * (float)color[3]));
        }
        dotSprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
}

// How visible an element is from its timer.
static inline float TimerAlpha(float time) {
    return time > 1.0f ? 1.0f : time;
}

// Fades the HUD in, counts down how long the health, ammo and weapon name
// stay up (any change shows them again), pulses and rumbles at low
// health, turns the compass and counts down the hit markers.
void UserInterface::Update(float dt) {
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    m_pulse = 0.016683351f * (6.2831855f * dt) + m_pulse;
    if (m_pulse > 6.2831855f) {
        m_pulse = m_pulse - 6.2831855f;
    }
    if (m_fadeIn > 0.0f) {
        m_fadeIn = m_fadeIn - 0.016683351f * dt;
    }
    if (m_fadeIn >= 1.0f) {
        m_alpha = 0.0f;
    } else if (m_fadeIn > 0.0f) {
        m_alpha = 1.0f - m_fadeIn;
    } else {
        m_alpha = 1.0f;
    }
    if (m_alwaysShow) {
        m_healthTime = 1.0f;
        m_ammoTime = 1.0f;
    }
    int health = ScriptHealth(player);
    int reserve = player->GetCurrentWeaponReserveAmoCount();
    int clip = player->GetCurrentWeaponClipAmoCount();
    if ((u32)player->GetCurrentWeapon() != m_lastWeapon) {
        m_ammoTime = 4.5f;
        m_nameTime = 4.5f;
    }
    if (reserve != m_lastReserve || clip != m_lastClip) {
        m_ammoTime = 4.5f;
    }
    if (health != m_lastHealthShown) {
        m_healthTime = 4.5f;
    }
    if (!g_Pause.bIsGamePaused()) {
        float seconds = 0.016683351f * dt;
        if (m_healthMode == UI_SHOW_FADE) {
            if (!m_alwaysShow) {
                m_healthTime = MathFunClamp<float>(m_healthTime - seconds, 0.0f, 4.5f);
                m_healthAlpha = TimerAlpha(m_healthTime);
                m_healthAlpha = m_healthAlpha * m_alpha;
            } else {
                m_healthAlpha = m_alpha;
            }
        } else if (m_healthMode == UI_SHOW_PULSE) {
            m_healthTime = m_healthTime - seconds;
            if (m_healthTime <= 0.0f) {
                m_healthTime = 0.0f;
                m_healthAlpha = m_alpha;
            } else {
                m_healthAlpha = m_alpha * (0.5f * (float)cos(m_pulse) + 0.5f);
            }
            m_heartbeat = m_heartbeat - seconds;
            if (m_heartbeat <= 0.0f) {
                CPlayerObject* p = g_scene.GetPlayer(m_playerIndex);
                g_inputMgr.SetFeedback(p->m_controllerPort, 9, 0, 0.0125f, 0.25f);
                m_heartbeat = 1.0f;
            }
        }
    }
    UpdateCompassRotation(player, dt);
    float seconds = 0.016683351f * dt;
    if (m_ammoMode == UI_SHOW_FADE) {
        if (!m_alwaysShow) {
            m_ammoTime = MathFunClamp<float>(m_ammoTime - seconds, 0.0f, 4.5f);
            m_ammoAlpha = TimerAlpha(m_ammoTime);
            m_ammoAlpha = m_ammoAlpha * m_alpha;
        } else {
            m_ammoAlpha = m_alpha;
        }
    } else if (m_ammoMode == UI_SHOW_PULSE) {
        m_ammoTime = 1.0f;
        m_ammoAlpha = m_alpha * (0.5f * (float)cos(m_pulse) + 0.5f);
    }
    for (int i = 0; i < 4; i++) {
        if (m_hitTime[i] > 0.0f) {
            m_hitTime[i] = m_hitTime[i] - seconds;
            if (m_hitTime[i] < 0.0f) {
                m_hitTime[i] = 0.0f;
            }
        }
    }
    m_nameTime = MathFunClamp<float>(m_nameTime - seconds, 0.0f, 4.5f);
    m_nameAlpha = TimerAlpha(m_nameTime);
    m_nameAlpha = m_nameAlpha * m_alpha;
    if (g_bInMultiplayerMode && m_mpTimeWarning > 0.0f) {
        m_mpTimeWarning = m_mpTimeWarning - seconds;
    }
    m_lastHealthShown = health;
    m_lastReserve = player->GetCurrentWeaponReserveAmoCount();
    m_lastClip = player->GetCurrentWeaponClipAmoCount();
    m_lastWeapon = (u32)player->GetCurrentWeapon();
}

// The red markers around the compass for recent hits (smaller with three
// or four players).
void UserInterface::DrawHitStatus() {
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    float scale = 1.0f;
    if (g_bInMultiplayerMode && g_NumPlayers > 2) {
        scale = 0.75f;
    }
    float centerX = 92.0f * vpWidth;
    float centerY = 384.0f * vpHeight;
    _spritepolyvert* src = HitSpriteVerts;
    for (int i = 0; i < 4; i++, src += 3) {
        if (m_hitTime[i] <= 0.0f) {
            continue;
        }
        memcpy(TempVerts, src, 3 * sizeof(_spritepolyvert));
        int a = (int)(96.0f * ((m_healthAlpha * m_alpha) * TimerAlpha(m_hitTime[i])));
        float x0 = (float)width * g_viewport.m_left;
        float y0 = (float)height * g_viewport.m_top;
        float ws = g_screen.m_widthScale;
        float hs = g_screen.m_heightScale;
        for (int k = 0; k < 3; k++) {
            _spritepolyvert* v = &TempVerts[k];
            v->color[3] = a;
            if (!g_bInMultiplayerMode) {
                v->x = v->x * ws;
                v->y = v->y * hs;
            } else {
                v->x = centerX + (scale * (v->x - 92.0f) + x0);
                v->y = (centerY + (scale * (v->y - 384.0f) + y0)) - 16.0f;
            }
        }
        hitMeterSprite->DrawPoly(TempVerts, 3, (CSprite::EPolyPrimType)3);
    }
}

// The HUD alpha of an element at `alpha`, as a vertex alpha.
static inline u8 HudAlpha(float scale, float alpha, float hud) {
    return (int)((scale * alpha) * hud);
}

// The compass dial (turned to the player's heading; a plain disc in
// multiplayer), its edge, and the health meter around it: eight wedges
// of 12.5 health, the last one partly shrunk, red up to 33, yellow up to
// 67, else green. The meter pulses at low health (below 25).
void UserInterface::DrawCompass() {
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    if (m_healthAlpha * m_alpha <= 0.0f) {
        return;
    }
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float scale = 1.0f;
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    if (g_bInMultiplayerMode && g_NumPlayers > 2) {
        scale = 0.75f;
    }
    if (!g_bInMultiplayerMode) {
        memcpy(TempVerts, CompassVerts, sizeof(CompassVerts));
        _spritepolyvert* v = TempVerts;
        for (int i = 0; i < 24; i++, v++) {
            v->x = v->x - 92.0f;
            v->y = v->y - 384.0f;
            MathFunRotateAboutY(&v->x, &v->y, m_compassAngle);
            v->x = v->x + 92.0f;
            v->y = v->y + 384.0f;
            v->x = v->x * g_screen.m_widthScale;
            v->y = v->y * g_screen.m_heightScale;
            v->color[3] = HudAlpha(128.0f, m_healthAlpha, m_alpha);
        }
        compassSprite->DrawPoly(TempVerts, 24, (CSprite::EPolyPrimType)3);
    } else {
        memcpy(TempVerts, CompassVerts, sizeof(CompassVerts));
        float x0 = (float)width * g_viewport.m_left;
        float y0 = (float)height * g_viewport.m_top;
        _spritepolyvert* v = TempVerts;
        for (int i = 0; i < 24; i++, v++) {
            v->x = 92.0f * vpWidth + (0.95f * (scale * (v->x - 92.0f)) + x0);
            v->y = (384.0f * vpHeight + (0.95f * (scale * (v->y - 384.0f)) + y0)) - 16.0f;
            v->color[3] = 0x40;
        }
        circleSprite->DrawPoly(TempVerts, 24, (CSprite::EPolyPrimType)3);
    }

    u32 health = 0;
    BSObject* script = player->GetScriptObject();
    if (script != NULL) {
        int* data = (int*)script->m_gameObject->GetScriptData();
        if (data != NULL) {
            health = data[0];
        }
        if (m_lastHealth != health) {
            if (health < 25) {
                m_healthTime = 5.0f;
                m_healthMode = UI_SHOW_PULSE;
            } else {
                m_healthTime = 4.5f;
                m_healthMode = UI_SHOW_FADE;
            }
        }
    }
    m_lastHealth = health;

    if (!g_bInMultiplayerMode) {
        memcpy(TempVerts, CompassVerts, sizeof(CompassVerts));
        _spritepolyvert* v = TempVerts;
        for (int i = 0; i < 24; i++, v++) {
            v->x = v->x * g_screen.m_widthScale;
            v->y = v->y * g_screen.m_heightScale;
            v->color[3] = HudAlpha(128.0f, m_healthAlpha, m_alpha);
        }
        compassEdgeSprite->DrawPoly(TempVerts, 24, (CSprite::EPolyPrimType)3);
    } else if (!player->m_dead) {
        memcpy(TempVerts, CompassVerts, sizeof(CompassVerts));
        float x0 = (float)width * g_viewport.m_left;
        float y0 = (float)height * g_viewport.m_top;
        _spritepolyvert* v = TempVerts;
        for (int i = 0; i < 24; i++, v++) {
            v->x = 92.0f * vpWidth + (scale * (v->x - 92.0f) + x0);
            v->y = (384.0f * vpHeight + (scale * (v->y - 384.0f) + y0)) - 16.0f;
            v->color[3] = HudAlpha(128.0f, m_healthAlpha, m_alpha);
        }
        healthSprite->DrawPoly(TempVerts, 24, (CSprite::EPolyPrimType)3);
    }

    // The health meter.
    memcpy(TempVerts, CompassVerts, sizeof(CompassVerts));
    u8 r, g, b;
    if (health <= 33) {
        r = 0x80;
        g = 0;
        b = 0;
    } else if (health <= 67) {
        r = 0x80;
        g = 0x80;
        b = 0;
    } else {
        r = 0;
        g = 0x80;
        b = 0;
    }
    {
        float x0 = (float)width * g_viewport.m_left;
        float y0 = (float)height * g_viewport.m_top;
        _spritepolyvert* v = TempVerts;
        for (int i = 0; i < 24; i++, v++) {
            if (!g_bInMultiplayerMode) {
                v->x = v->x * g_screen.m_widthScale;
                v->y = v->y * g_screen.m_heightScale;
            } else {
                v->x = 92.0f * vpWidth + (scale * (v->x - 92.0f) + x0);
                v->y = (384.0f * vpHeight + (scale * (v->y - 384.0f) + y0)) - 16.0f;
            }
            v->color[0] = r;
            v->color[1] = g;
            v->color[2] = b;
            v->color[3] = HudAlpha(128.0f, m_healthAlpha, m_alpha);
        }
    }
    if (health < 100 && health != 0) {
        float wedges = (float)health / 12.5f;
        int full = (int)(float)floor(wedges);
        int empty = 8 - full;
        float part = wedges - (float)full;
        if (empty > 0) {
            // Hide the empty wedges (three vertices each).
            for (int k = 1; k < empty; k++) {
                _spritepolyvert* w = &TempVerts[(k - 1) * 3];
                w[0].color[3] = 0;
                w[1].color[3] = 0;
                w[2].color[3] = 0;
            }
            // Shrink the partly full one toward its far edge.
            float left = 1.0f - part;
            int j = empty - 1;
            _spritepolyvert* edge = &TempVerts[j * 3 + 1];
            switch (j) {
            case 0:
            case 7:
                edge->x = (64.0f * scale) * left + edge->x;
                edge->u = 0.5f * left + edge->u;
                break;
            case 1:
            case 2:
                edge->y = (64.0f * scale) * left + edge->y;
                edge->v = 0.5f * left + edge->v;
                break;
            case 3:
            case 4:
                edge->x = edge->x - (64.0f * scale) * left;
                edge->u = edge->u - 0.5f * left;
                break;
            case 5:
            case 6:
                edge->y = edge->y - (64.0f * scale) * left;
                edge->v = edge->v - 0.5f * left;
                break;
            }
        }
    }
    if (health != 0) {
        healthSprite->DrawPoly(TempVerts, 24, (CSprite::EPolyPrimType)3);
    }
}

// Turns the compass toward the player's heading with a damped spring.
void UserInterface::UpdateCompassRotation(CPlayerObject* player, float dt) {
    for (int i = 0; (float)i < dt; i++) {
        m_compassSpeed = m_compassSpeed * 0.95f;
    }
    CVector3 forward;
    player->GetForward(forward);
    forward.Normalize();
    float angle = (float)acos(m_northX * forward.x + m_northY * forward.y + m_northZ * forward.z);
    if (m_compassFlip) {
        angle = angle + 3.1415927f;
    }
    // (As in the original, the side test compares against forward.x.)
    if (forward.y * m_northZ - forward.z * m_northY > forward.x) {
        angle = angle * -1.0f;
    }
    float turn = 0.0f;
    if (angle > m_compassAngle) {
        turn = angle - m_compassAngle;
        if (turn > 3.1415927f) {
            turn = turn - 6.2831855f;
        }
    } else if (angle < m_compassAngle) {
        turn = angle - m_compassAngle;
        if (turn < -3.1415927f) {
            turn = 6.2831855f + turn;
        }
    }
    if (turn < 0.0f || turn > 0.0f) {
        m_compassSpeed = m_compassSpeed + turn;
        m_compassAngle = 0.016683351f * (m_compassSpeed * dt) + m_compassAngle;
    }
    float a = m_compassAngle;
    if (a > 3.1415927f) {
        a = -6.2831855f + a;
    } else if (a < -3.1415927f) {
        a = 6.2831855f + a;
    }
    m_compassAngle = a;
}

// The clip and reserve counts and the ammo icon (grenades show a single
// total; weapon type 0x25 shows only the icon).
void UserInterface::DrawBulletStats() {
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    if (player->GetCurrentWeapon() == NULL || player->m_onMG) {
        return;
    }
    int type = player->GetCurrentWeapon()->m_weaponType;
    bool counts = true;
    if (type == 0x25) {
        counts = false;
    }
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float scale = 1.0f;
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    if (g_bInMultiplayerMode && g_NumPlayers > 2) {
        scale = 0.75f;
    }
    if (counts) {
        u16 clip = player->GetCurrentWeaponClipAmoCount();
        u16 reserve = player->GetCurrentWeaponReserveAmoCount();
        bulletText->SetColor(CColor(0x80, 0x80, 0x80, HudAlpha(128.0f, m_ammoAlpha, m_alpha)));
        float sx, sy;
        bulletText->GetScale(sx, sy);
        bulletText->SetScale(scale, scale);
        char text[0x50];
        if (type != 0x21 && type != 0x22) {
            sprintf(text, "%d", clip);
            float rx, ry, rw, rh;
            FONT_getrecta(bulletText->m_font, text, &rx, &ry, &rw, &rh);
            rw = rw * screen->GetWidthScale();
            int x = 506.0f * g_screen.m_widthScale - rw;
            if (g_bInMultiplayerMode) {
                int y = (int)(vpHeight * (400.0f * g_screen.m_heightScale)) +
                        (int)((float)height * g_viewport.m_top) - 10;
                int sx2 = (int)((float)x * vpWidth) + (int)((float)width * g_viewport.m_left);
                bulletText->DrawText((float)sx2, (float)y, text);
            } else {
                bulletText->DrawText((float)x, (float)(int)(400.0f * g_screen.m_heightScale), text);
            }
        }
        if (type == 0x21 || type == 0x22) {
            sprintf(text, "%d", reserve + clip);
        } else {
            sprintf(text, "%d", reserve);
        }
        int x = 554.0f * g_screen.m_widthScale;
        if (g_bInMultiplayerMode) {
            int y = (int)(vpHeight * (400.0f * g_screen.m_heightScale)) + (int)((float)height * g_viewport.m_top) - 10;
            int sx2 = (int)((float)x * vpWidth + (float)width * g_viewport.m_left);
            bulletText->DrawText((float)sx2, (float)y, text);
        } else {
            bulletText->DrawText((float)x, (float)(int)(400.0f * g_screen.m_heightScale), text);
        }
        bulletText->SetScale(sx, sy);
    }
    ammoSprite = player->GetCurrentBulletSprite();
    if (ammoSprite == NULL) {
        return;
    }
    memcpy(TempVerts, AmmoSpriteVerts, sizeof(AmmoSpriteVerts));
    // (As in the original, each vertex's alpha is scaled from the first
    // vertex's, which is updated first.)
    for (int i = 0; i < 4; i++) {
        TempVerts[i].color[3] = (int)(m_alpha * (m_ammoAlpha * (float)TempVerts[0].color[3]));
        TempVerts[i].x = TempVerts[i].x * g_screen.m_widthScale;
        TempVerts[i].y = TempVerts[i].y * g_screen.m_heightScale;
    }
    if (g_bInMultiplayerMode) {
        _spritepolyvert* v = TempVerts;
        int h = (int)(scale * (v[3].y - v[0].y));
        int w = (int)(scale * (v[1].x - v[0].x));
        float cy = vpHeight * (0.5f * (v[0].y + v[3].y)) + (float)height * g_viewport.m_top;
        float cx = vpWidth * (0.5f * (v[0].x + v[1].x)) + (float)width * g_viewport.m_left;
        float top = (cy - 0.5f * (float)h) - 5.0f;
        float left = cx - 0.5f * (float)w;
        float bottom = (top + (float)h) - 5.0f;
        float right = left + (float)w;
        v[3].x = left;
        v[0].x = left;
        v[1].y = top;
        v[0].y = top;
        v[2].x = right;
        v[1].x = right;
        v[3].y = bottom;
        v[2].y = bottom;
    }
    ammoSprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
}

// The weapon's name above the ammo count, in the subtitle font.
void UserInterface::DrawWeaponName(const char* name) {
    if (name[0] == 0) {
        return;
    }
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    CFont* font = g_pPopUpMessageHandler[player->field_0x768]->m_pSubtitleFont;
    CColor old = font->GetColor();
    font->SetColor(CColor(0x80, 0x80, 0x80, HudAlpha(96.0f, m_nameAlpha, m_alpha)));
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    float sx, sy;
    font->GetScale(sx, sy);
    font->SetScale(1.0f, 1.0f);
    float rx, ry, rw, rh;
    FONT_getrecta(font->m_font, name, &rx, &ry, &rw, &rh);
    rw = rw * screen->GetWidthScale();
    if (!g_bInMultiplayerMode) {
        float ws = g_screen.m_widthScale;
        float x = ((float)g_screen.GetWidth() - rw) - 56.0f * ws;
        font->DrawText(x, 356.0f * g_screen.m_heightScale, name);
    } else {
        float y0 = (float)height * g_viewport.m_top;
        float x = ((float)width * g_viewport.m_right - rw) - vpWidth * (56.0f * g_screen.m_widthScale);
        float y = (vpHeight * (356.0f * g_screen.m_heightScale) + y0) - 15.0f;
        font->DrawText(x, y, name);
    }
    font->SetScale(sx, sy);
    font->SetColor(old);
}

// The quad template for full-screen backgrounds.
static const _spritepolyvert s_quadVerts[4] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0xFF}},
};

// The multiplayer scoreboard, centered in the view (over black when
// `full`): each player's character headshot, name in the team color,
// score, and a star for the leader.
void UserInterface::DrawFragCount(bool full) {
    static char msg[0x50];
    static char score[0xA];
    if (!g_bInMultiplayerMode) {
        return;
    }
    CScreen* screen = CScreen::GetCurrent();
    if (full) {
        float left = g_viewport.m_left * (float)screen->GetWidth();
        float top = g_viewport.m_top * (float)screen->GetHeight();
        float right = g_viewport.m_right * (float)screen->GetWidth();
        float bottom = g_viewport.m_bottom * (float)screen->GetHeight();
        _spritepolyvert v[4];
        memcpy(v, s_quadVerts, sizeof(v));
        v[0].x = left;
        v[0].y = top;
        v[1].x = right;
        v[1].y = top;
        v[2].x = right;
        v[2].y = bottom;
        v[3].x = left;
        v[3].y = bottom;
        blackSprite->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
    }
    int rowsHeight = g_NumPlayers * 32 + (g_NumPlayers - 1) * 16;
    float margin = 0.5f * (g_viewport.m_height * (float)screen->GetHeight() - (float)rowsHeight);
    int y = g_viewport.m_top * (float)screen->GetHeight() + margin;
    float sx, sy;
    smallMPFont->GetScale(sx, sy);
    smallMPFont->SetScale(1.0f, 1.0f);

    // Column widths.
    int nameWidth = 0;
    int scoreWidth = 0;
    float rx, ry, rw, rh;
    for (int i = 0; i < g_NumPlayers; i++) {
        CPlayerObject* p = g_scene.GetPlayer(i);
        sprintf(msg, "%sXX", p->GetPlayerName());
        FONT_getrecta(smallMPFont->m_font, msg, &rx, &ry, &rw, &rh);
        nameWidth = (float)nameWidth >= rw ? (float)nameWidth : rw;
        sprintf(score, "%d", g_Shell.Get_MPPlayerScore(p->m_playerIndex));
        FONT_getrecta(smallMPFont->m_font, score, &rx, &ry, &rw, &rh);
        scoreWidth = (float)scoreWidth >= rw ? (float)scoreWidth : rw;
    }
    nameWidth = (float)nameWidth * screen->GetWidthScale();
    scoreWidth = (float)scoreWidth * screen->GetWidthScale();
    float vpWidth = g_viewport.m_width;
    float vpLeft = g_viewport.m_left;
    float half = 0.5f * (vpWidth * (float)screen->GetWidth() - (float)(nameWidth + scoreWidth + 0x30));
    int x = vpLeft * (float)screen->GetWidth() + half;
    int nameX = x + 0x30;
    int scoreRight = x + nameWidth + scoreWidth + 0x30;

    for (int i = 0; i < g_NumPlayers; i++, y += 0x30) {
        CPlayerObject* p = g_scene.GetPlayer(i);
        int character = g_Shell.m_players[p->m_controllerPort].m_character;
        memcpy(TempVerts, headshotImageVerts, sizeof(headshotImageVerts));
        for (int k = 0; k < 4; k++) {
            TempVerts[k].x = TempVerts[k].x + (float)x;
            TempVerts[k].y = TempVerts[k].y + (float)y;
        }
        headshotImages[character]->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
        smallMPFont->SetColor(teamColors[p->m_team]);
        sprintf(msg, "%s", p->GetPlayerName());
        // (The text is centered on the last measured height.)
        smallMPFont->DrawText((float)nameX, (float)(y + 0x10) - 0.5f * rh, msg);
        sprintf(score, "%d", g_Shell.Get_MPPlayerScore(p->m_playerIndex));
        FONT_getrecta(smallMPFont->m_font, score, &rx, &ry, &rw, &rh);
        rw = rw * screen->GetWidthScale();
        smallMPFont->DrawText((float)scoreRight - rw, (float)(y + 0x10) - 0.5f * rh, score);
        if (g_Shell.GetMPPlayerStats(i)->m_star) {
            for (int k = 0; k < 4; k++) {
                TempVerts[k].x = TempVerts[k].x - 40.0f;
            }
            starSprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
        }
    }
    smallMPFont->SetScale(sx, sy);
}

// Split screen: thin grey lines along the view's inner edges, in the
// player's team color.
void UserInterface::DrawViewportEdge() {
    memcpy(TempVerts, LetterBoxVerts, 4 * sizeof(_spritepolyvert));
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    switch (player->m_team) {
    case 0:
    case 1:
    case 2:
    case 3:
        r = teamColors[player->m_team].r;
        g = teamColors[player->m_team].g;
        b = teamColors[player->m_team].b;
        break;
    }
    for (int k = 0; k < 4; k++) {
        TempVerts[k].color[0] = r;
        TempVerts[k].color[1] = g;
        TempVerts[k].color[2] = b;
        TempVerts[k].color[3] = 0xFF;
    }
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    int view = player->field_0x768;
    bool top, bottom, left, right;
    if (g_NumPlayers == 2) {
        top = view == 1;
        bottom = view == 0;
        left = false;
        right = false;
    } else {
        top = view == 2 || view == 3;
        bottom = view == 0 || view == 1;
        left = view == 1 || view == 3;
        right = view == 2 || view == 0;
    }
    float x0 = (float)width * g_viewport.m_left;
    float x1 = (float)width * g_viewport.m_right - 1.0f;
    float y0 = (float)height * g_viewport.m_top;
    float y1 = (float)height * g_viewport.m_bottom - 1.0f;
    _spritepolyvert* v = TempVerts;
    if (top) {
        v[3].x = x0;
        v[0].x = x0;
        v[2].x = x1;
        v[1].x = x1;
        v[1].y = y0;
        v[0].y = y0;
        v[3].y = 2.0f + y0;
        v[2].y = 2.0f + y0;
        greySprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
    if (bottom) {
        v[3].x = x0;
        v[0].x = x0;
        v[2].x = x1;
        v[1].x = x1;
        v[1].y = y1 - 2.0f;
        v[0].y = y1 - 2.0f;
        v[3].y = y1;
        v[2].y = y1;
        greySprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
    if (left) {
        v[1].y = y0;
        v[0].y = y0;
        v[3].x = 1.0f + x0;
        v[0].x = 1.0f + x0;
        v[2].x = 5.0f + x0;
        v[1].x = 5.0f + x0;
        v[3].y = y1;
        v[2].y = y1;
        greySprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
    if (right) {
        v[2].x = x1;
        v[1].x = x1;
        v[3].x = x1 - 4.0f;
        v[0].x = x1 - 4.0f;
        v[1].y = y0;
        v[0].y = y0;
        v[3].y = y1;
        v[2].y = y1;
        greySprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
}

// Multiplayer: this player's score at the view's top right (out of the
// score limit when there is one, with a star when leading) and, after
// the 5 and 1 minute warnings, the time left.
void UserInterface::DrawScore() {
    static char buffer[0x50];
    CScreen* screen = CScreen::GetCurrent();
    int width = screen->GetWidth();
    int height = screen->GetHeight();
    float vpWidth = g_viewport.m_width;
    int viewWidth = (float)width * vpWidth;
    int top = (float)height * g_viewport.m_top;
    int left = (float)width * g_viewport.m_left;
    MPPlayerStats* stats = g_Shell.GetMPPlayerStats(m_playerIndex);
    if (g_Shell.field_0x1430 != 0) {
        sprintf(buffer, "%d / %d", stats->m_goalScore, g_Shell.field_0x1428);
    } else {
        sprintf(buffer, "%d", stats->m_goalScore);
    }
    float sx, sy;
    smallMPFont->GetScale(sx, sy);
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    smallMPFont->SetScale(1.0f, 1.0f);
    float rx, ry, rw, rh;
    FONT_getrecta(smallMPFont->m_font, buffer, &rx, &ry, &rw, &rh);
    rw = rw * screen->GetWidthScale();
    float inset = 54.0f * vpWidth;
    int x = ((float)(left + viewWidth) - inset) - (float)(int)rw;
    int y = top + 0x10;
    smallMPFont->SetColor(teamColors[player->m_team]);
    smallMPFont->DrawText((float)x, (float)y, buffer);
    if (stats->m_star) {
        memcpy(TempVerts, StarSpriteVerts, sizeof(StarSpriteVerts));
        for (int k = 0; k < 4; k++) {
            _spritepolyvert* v = &TempVerts[k];
            v->x = ((((0.75f * v->x + (float)left) + (float)viewWidth) - inset) - 12.0f);
            v->y = 12.0f + (12.0f + (rh + (0.75f * v->y + (float)y)));
        }
        starSprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
    }
    if (m_mpTimeWarning > 0.0f) {
        float minutes = (float)floor(m_mpTimeRemaining);
        float fraction = m_mpTimeRemaining - minutes;
        if (m_mpTimeWarning < 2.0f) {
            smallMPFont->SetColor(CColor(0xFF, 0xFF, 0xFF, (int)(0.5f * (128.0f * m_mpTimeWarning))));
        } else {
            smallMPFont->SetColor(CColor(0xFF, 0xFF, 0xFF, 0x80));
        }
        sprintf(buffer, "%d:%02d remaining", (int)minutes, (int)(60.0f * fraction));
        smallMPFont->DrawTextCentered((float)(top + 0x48), buffer);
    }
    smallMPFont->SetScale(sx, sy);
}

// Updates and draws the HUD (nothing on the first frame; the letterbox
// bars in cinematics; in multiplayer the scoreboard while dead or holding
// the score button).
void UserInterface::Draw(float dt) {
    static bool firstframe = true;
    CPlayerObject* player = g_scene.GetPlayer(m_playerIndex);
    Update(dt);
    if (firstframe) {
        firstframe = false;
        return;
    }
    if (m_letterbox) {
        memcpy(TempVerts, LetterBoxVerts, sizeof(LetterBoxVerts));
        for (int k = 0; k < 8; k++) {
            TempVerts[k].x = TempVerts[k].x * g_screen.m_widthScale;
            TempVerts[k].y = TempVerts[k].y * g_screen.m_heightScale;
        }
        blackSprite->DrawPoly(TempVerts, 4, (CSprite::EPolyPrimType)5);
        blackSprite->DrawPoly(TempVerts + 4, 4, (CSprite::EPolyPrimType)5);
    } else if (!player->m_hideWeapon && (g_Pause.field_0x88 < 3 || g_Pause.field_0x88 >= 6)) {
        if (g_bInMultiplayerMode) {
            CPlayerObject* me = g_scene.GetPlayer(m_playerIndex);
            if (g_bMPEndLevel) {
                // nothing
            } else if (g_Pause.bIsGamePaused()) {
                if (g_PauseScreenLockedController != me->m_controllerPort) {
                    DrawFragCount(false);
                }
            } else if (me->m_dead || g_inputMgr.GetValue(me->m_controllerPort, 0x17) != 0) {
                DrawFragCount(false);
                DrawViewportEdge();
            } else {
                DrawBulletStats();
                DrawCompass();
                DrawHitStatus();
                CPlayerWeaponObject* weapon = player->GetCurrentPlayerWeapon();
                if (weapon != NULL) {
                    DrawWeaponName(weapon->m_name);
                }
                DrawViewportEdge();
                DrawRadar();
                DrawScore();
            }
        } else {
            DrawBulletStats();
            DrawCompass();
            DrawHitStatus();
            CPlayerWeaponObject* weapon = player->GetCurrentPlayerWeapon();
            if (weapon != NULL) {
                DrawWeaponName(weapon->m_name);
            }
        }
    }
    firstframe = false;
}

static inline CSprite* NewSprite(const char* name, int priority) {
    void* mem = DWI_alloc(NULL, sizeof(CSprite), 0x400);
    CSprite* sprite = NULL;
    if (mem != NULL) {
        sprite = ::new (mem) CSprite(name, (CSprite::ESpriteLoadType)1, (ERenderPriority)priority);
    }
    return sprite;
}

// Loads the HUD sprites and fonts (once) and resets the display state.
void UserInterface::Init() {
    if (g_bInMultiplayerMode && circleSprite == NULL) {
        circleSprite = NewSprite("data\\blackCircle.gsh", 0x6F);
    }
    if (compassSprite == NULL) {
        compassSprite = NewSprite("data\\compassDial.gsh", 0x6F);
    }
    if (compassEdgeSprite == NULL) {
        compassEdgeSprite = NewSprite("data\\compassEdge.gsh", 0x6F);
    }
    if (healthSprite == NULL) {
        healthSprite = NewSprite("data\\healthMeter.gsh", 0x6F);
    }
    if (hitMeterSprite == NULL) {
        hitMeterSprite = NewSprite("data\\hitMeter.gsh", 0x6F);
    }
    if (g_bInMultiplayerMode) {
        if (dotSprite == NULL) {
            dotSprite = NewSprite("data\\dot.gsh", 0x6F);
        }
        if (greySprite == NULL) {
            greySprite = NewSprite("data\\grey.gsh", 0x6F);
        }
        if (starSprite == NULL) {
            starSprite = NewSprite("data\\star.gsh", 0x6F);
        }
        char name[20] = {0};
        for (int i = 0; i < 20; i++) {
            if (headshotImages[i] == NULL) {
                sprintf(name, "bitmaps\\Model%02d.gsh", i + 1);
                headshotImages[i] = NewSprite(name, 0x6F);
            }
        }
    }
    if (blackSprite == NULL) {
        blackSprite = NewSprite("data\\blackSprite.gsh", 0xA);
    }
    ammoSprite = NULL;
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    void* font = TLT_LoadFileFromLevelBigFile((char*)level->fields[1].value, NULL);
    if (bulletText == NULL) {
        bulletText = new CFont((const char*)font, (ERenderPriority)0x6F);
    }
    bulletText->SetColor(CColor(0x40, 0x40, 0x40, 0x40));
    bulletText->SetScale(1.0f, 1.0f);
    CScreen* screen = CScreen::GetCurrent();
    if (screen != NULL) {
        bulletText->SetDepth(screen->GetDepthBufferMax());
    }
    if (g_bInMultiplayerMode && smallMPFont == NULL) {
        void* mpFont = TLT_LoadFileFromLevelBigFile((char*)level->fields[3].value, NULL);
        smallMPFont = new CFont((const char*)mpFont, (ERenderPriority)0x6F);
        smallMPFont->SetColor(CColor(0x80, 0x80, 0x80, 0x80));
    }
    m_healthTime = 4.5f;
    m_healthMode = UI_SHOW_FADE;
    m_ammoTime = 4.5f;
    m_ammoMode = UI_SHOW_FADE;
    m_hitTime[0] = 0.0f;
    m_hitTime[1] = 0.0f;
    m_hitTime[2] = 0.0f;
    m_hitTime[3] = 0.0f;
    m_compassAngle = 0.0f;
    m_compassSpeed = 0.0f;
    m_letterbox = 0;
    if (g_bInMultiplayerMode) {
        m_mpTimeWarning = 0.0f;
    }
    LevelFileContentsStruct_* map = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    if (map != NULL && map->fields[58].value != 0) {
        m_compassFlip = 1;
    }
}

// (The shared sprites are deleted in single player but their pointers
// kept, as in the original.)
UserInterface::~UserInterface() {
    if (!g_bInMultiplayerMode) {
        if (healthSprite != NULL) {
            delete healthSprite;
        }
        if (compassSprite != NULL) {
            delete compassSprite;
        }
        if (hitMeterSprite != NULL) {
            delete hitMeterSprite;
        }
        if (compassEdgeSprite != NULL) {
            delete compassEdgeSprite;
        }
        if (blackSprite != NULL) {
            delete blackSprite;
        }
        if (bulletText != NULL) {
            delete bulletText;
        }
    }
}

UserInterface::UserInterface() {
    m_lastHealth = 100;
    m_northX = 0.0f;
    m_northY = 1.0f;
    m_northZ = 0.0f;
    f7A4_4 = 0;
    f7A4_7 = 0;
    f7A4_5 = 0;
    f7A4_6 = 0;
    m_alpha = 1.0f;
    m_fadeIn = 0.0f;
    m_healthAlpha = 1.0f;
    m_ammoAlpha = 1.0f;
    m_healthMode = UI_SHOW_FADE;
    m_ammoMode = UI_SHOW_FADE;
    m_healthTime = 4.5f;
    m_ammoTime = 4.5f;
    m_nameTime = 4.5f;
    m_heartbeat = 0.0f;
    field_0x7EC = 4.5f;
    m_alwaysShow = 1;
    m_lastReserve = 0;
    m_lastClip = 0;
    m_lastWeapon = 0;
    m_lastHealthShown = 0;
    m_compassFlip = 0;
}
