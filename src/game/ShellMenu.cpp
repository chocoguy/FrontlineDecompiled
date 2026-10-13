// CShellMenu: the front-end shell (menus, briefings, credits, memory card
// dialogs, passwords) and the game progress it keeps.
#include "game/ShellMenu.h"
#include "game/ConfigGC.h"
#include "game/IStudioFuncs.h"
#include "game/MemoryCardGC.h"
#include "game/Moh2.h"
#include "game/Screen.h"
#include "game/configfile.h"
#include "game/font.h"
#include "game/input.h"
#include "game/isexportdefs.h"
#include "game/loadelf.h"
#include "game/mainloop.h"
#include "game/player.h"
#include "game/real_bridge.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/sprite.h"
#include "game/toilet.h"
#include "game/weapon.h"
#include "dolphin/gx.h"
#include "dolphin/os.h"
#include "dolphin/vi.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

int g_NumPlayers = 1;
static const char* g_ConfigFilename = "debug_REMOVE_FROM_FINAL.INI";
const char* emptyString = " ";
CSoundStream* pSoundStream;
bool g_bIsRubberGrenadeCheatEnabled;
bool g_bInMultiplayerMode;

// User message tables: evaluation and best weapon texts.
enum {
    MSG_EVALUATION = 0x174,
    MSG_BEST_WEAPON = 0x197,
};

// Raises a 2-bit star field.
#define RAISE_STARS(byte, shift)                                 \
    if ((u32)(((byte) >> (shift)) & 3) < stars) {                \
        (byte) = ((byte) & ~(3 << (shift))) | ((stars & 3) << (shift)); \
    }

void SetStarRatingStatus(StarLevelsInBits* bits, unsigned int id, unsigned int stars) {
    u8* b = bits->m_bits;
    switch (id) {
    case 0xB:
        RAISE_STARS(b[0], 6);
        break;
    case 0xC:
        RAISE_STARS(b[0], 4);
        break;
    case 0xD:
        RAISE_STARS(b[0], 2);
        break;
    case 0xE:
        RAISE_STARS(b[0], 0);
        break;
    case 0x15:
        RAISE_STARS(b[1], 6);
        break;
    case 0x16:
        RAISE_STARS(b[1], 4);
        break;
    case 0x17:
        RAISE_STARS(b[1], 2);
        break;
    case 0x18:
        b[1] &= ~3;
        break;
    case 0x1F:
        RAISE_STARS(b[2], 6);
        break;
    case 0x20:
        RAISE_STARS(b[2], 4);
        break;
    case 0x21:
        RAISE_STARS(b[2], 2);
        break;
    case 0x22:
        b[2] &= ~3;
        break;
    case 0x29:
        RAISE_STARS(b[3], 6);
        break;
    case 0x2A:
        RAISE_STARS(b[3], 4);
        break;
    case 0x2B:
        RAISE_STARS(b[3], 2);
        break;
    case 0x2C:
        b[3] &= ~3;
        break;
    case 0x33:
        RAISE_STARS(b[4], 6);
        break;
    case 0x34:
        RAISE_STARS(b[4], 4);
        break;
    case 0x35:
        RAISE_STARS(b[4], 2);
        break;
    case 0x36:
        RAISE_STARS(b[4], 0);
        break;
    case 0x3D:
        RAISE_STARS(b[5], 6);
        break;
    case 0x3E:
        RAISE_STARS(b[5], 4);
        break;
    case 0x3F:
        b[5] &= ~0xC;
        break;
    case 0x40:
        b[5] &= ~3;
        break;
    }
}

#undef RAISE_STARS

// out[mission][stage] (6 x 4): stars 0..3.
void CShellMenu::GetAllLevelsStarStatus(int* out) {
    const u8* b = m_stars.m_bits;
    int value = 0;
    for (int mission = 0; mission < 6; mission++) {
        for (int stage = 0; stage < 4; stage++) {
            switch (stage + (mission + 1) * 10) {
            case 0xA:
                value = (b[0] >> 6) & 3;
                break;
            case 0xB:
                value = (b[0] >> 4) & 3;
                break;
            case 0xC:
                value = (b[0] >> 2) & 3;
                break;
            case 0xD:
                value = b[0] & 3;
                break;
            case 0x14:
                value = (b[1] >> 6) & 3;
                break;
            case 0x15:
                value = (b[1] >> 4) & 3;
                break;
            case 0x16:
                value = (b[1] >> 2) & 3;
                break;
            case 0x17:
                value = 0;
                break;
            case 0x1E:
                value = (b[2] >> 6) & 3;
                break;
            case 0x1F:
                value = (b[2] >> 4) & 3;
                break;
            case 0x20:
                value = (b[2] >> 2) & 3;
                break;
            case 0x21:
                value = 0;
                break;
            case 0x28:
                value = (b[3] >> 6) & 3;
                break;
            case 0x29:
                value = (b[3] >> 4) & 3;
                break;
            case 0x2A:
                value = (b[3] >> 2) & 3;
                break;
            case 0x2B:
                value = 0;
                break;
            case 0x32:
                value = (b[4] >> 6) & 3;
                break;
            case 0x33:
                value = (b[4] >> 4) & 3;
                break;
            case 0x34:
                value = (b[4] >> 2) & 3;
                break;
            case 0x35:
                value = b[4] & 3;
                break;
            case 0x3C:
                value = (b[5] >> 6) & 3;
                break;
            case 0x3D:
                value = (b[5] >> 4) & 3;
                break;
            case 0x3E:
                value = 0;
                break;
            case 0x3F:
                value = 0;
                break;
            }
            out[mission * 4 + stage] = value;
        }
    }
}

// out[mission][stage] (6 x 4): 1 if unlocked.
void CShellMenu::GetLevelUnlockedStatus(int* out) {
    const u8* b = m_unlocked.m_bits;
    int value = 0;
    for (int mission = 0; mission < 6; mission++) {
        for (int stage = 0; stage < 4; stage++) {
            switch (stage + (mission + 1) * 10) {
            case 0xA:
                value = (b[0] >> 7) & 1;
                break;
            case 0xB:
                value = (b[0] >> 6) & 1;
                break;
            case 0xC:
                value = (b[0] >> 5) & 1;
                break;
            case 0xD:
                value = (b[0] >> 4) & 1;
                break;
            case 0x14:
                value = (b[0] >> 3) & 1;
                break;
            case 0x15:
                value = (b[0] >> 2) & 1;
                break;
            case 0x16:
                value = (b[0] >> 1) & 1;
                break;
            case 0x17:
                value = 0;
                break;
            case 0x1E:
                value = (b[1] >> 7) & 1;
                break;
            case 0x1F:
                value = (b[1] >> 6) & 1;
                break;
            case 0x20:
                value = (b[1] >> 5) & 1;
                break;
            case 0x21:
                value = 0;
                break;
            case 0x28:
                value = (b[1] >> 3) & 1;
                break;
            case 0x29:
                value = (b[1] >> 2) & 1;
                break;
            case 0x2A:
                value = (b[1] >> 1) & 1;
                break;
            case 0x2B:
                value = 0;
                break;
            case 0x32:
                value = (b[2] >> 7) & 1;
                break;
            case 0x33:
                value = (b[2] >> 6) & 1;
                break;
            case 0x34:
                value = (b[2] >> 5) & 1;
                break;
            case 0x35:
                value = (b[2] >> 4) & 1;
                break;
            case 0x3C:
                value = (b[2] >> 3) & 1;
                break;
            case 0x3D:
                value = (b[2] >> 2) & 1;
                break;
            case 0x3E:
                value = 0;
                break;
            case 0x3F:
                value = 0;
                break;
            }
            out[mission * 4 + stage] = value;
        }
    }
}

// Interface language: the file suffix and subtitle track.
static inline void SetLanguage(CShellMenu* shell, int language) {
    switch (language) {
    case 1:
        shell->m_languageSuffix = "";
        shell->m_languageSubtitleIndex = 0;
        break;
    case 2:
        shell->m_languageSuffix = "s";
        shell->m_languageSubtitleIndex = 1;
        break;
    case 3:
        shell->m_languageSuffix = "i";
        shell->m_languageSubtitleIndex = 2;
        break;
    case 4:
        shell->m_languageSuffix = "";
        shell->m_languageSubtitleIndex = 0;
        break;
    case 5:
        shell->m_languageSuffix = "";
        shell->m_languageSubtitleIndex = 3;
        break;
    default:
        shell->m_languageSuffix = "";
        shell->m_languageSubtitleIndex = 0;
        break;
    }
}

static inline void ResetMPStats(CShellMenu* shell) {
    memset(shell->m_mpStats, 0, sizeof(shell->m_mpStats));
    shell->m_mpStats[0].m_lastHitBy = -1;
    shell->m_mpStats[1].m_lastHitBy = -1;
    shell->m_mpStats[2].m_lastHitBy = -1;
    shell->m_mpStats[3].m_lastHitBy = -1;
}

// Default multiplayer players and match settings.
static inline void ResetMPSetup(CShellMenu* shell) {
    for (u16 i = 0; i < 4; i++) {
        CShellPlayerConfig& player = shell->m_players[i];
        player.m_invertLook = 0;
        player.m_crosshair = 1;
        player.m_cookGrenades = 0;
        player.m_rumble = 1;
        player.m_radar = 1;
        player.m_team = i;
        player.m_character = i;
        player.m_controllerConfig = 1;
        shell->field_0x143C[i] = 100;
        shell->m_mpControllerPort[0] = -1;
        shell->m_mpControllerPort[1] = -1;
        shell->m_mpControllerPort[2] = -1;
        shell->m_mpControllerPort[3] = -1;
        sprintf(player.m_name, "PLAYER %d", i + 1);
    }
    shell->m_mpWeaponSet = 1;
    shell->m_mpStage = 1;
    shell->m_mpTimeLimit = 0;
    shell->field_0x1428 = 10;
    shell->field_0x1434 = 1;
    shell->m_mpMusicTrack = 1;
    shell->m_mpEnabled = 0;
    shell->m_mpNumPlayers = 0;
    shell->m_mpMission = 7;
    shell->m_mpTimeLimitOn = shell->m_mpTimeLimit > 0 ? 1 : 0;
    shell->field_0x1430 = shell->field_0x1428 > 0 ? 1 : 0;
    ResetMPStats(shell);
}

CShellMenu::CShellMenu() {
    m_shellOn = 0;
    field_0x254 = 0;
    m_controllerSetting = 1;
    m_difficulty = 1;
    field_0x2 = 1;
    field_0x3 = 1;
    m_objectivesDone = 0;
    memset(m_secrets, 0, sizeof(m_secrets));
    memset(m_medals, 0, sizeof(m_medals));
    memset(&m_stars, 0, sizeof(m_stars));
    memset(&m_unlocked, 0, sizeof(m_unlocked));
    field_0x28 = 7;
    m_musicVolume = 7;
    if (OSGetSoundMode() == 0) {
        m_soundMode = 2;
    } else {
        m_soundMode = 0;
    }
    memset(&m_levelStats, 0, sizeof(m_levelStats));
    memset(&m_careerStats, 0, sizeof(m_careerStats));
    m_invertLook = 0;
    m_crosshair = 0;
    m_rumble = 1;
    m_cookGrenades = 0;
    field_0x218 &= ~SHELL_SUBTITLES;
    field_0x218 &= ~SHELL_CONTROLS_SET;
    field_0x218 &= ~SHELL_DIFFICULTY_SET;
    field_0x218 &= ~SHELL_NEWGAME;
    field_0x21C = 1;
    SetLanguage(this, field_0x21C);
    m_languageSuffix = "";
    g_ControllerConfig.SetUpDefaultConfig(1);
    field_0x18EC = 0;
    pSoundStream = NULL;
    m_moviePlayTime = 0.0f;
    field_0x1998 = 0;
    field_0x1999 = 0;
    field_0x199A = 0;
    field_0x199B = 0;
    field_0x19A2 = -1;
    field_0x19A3 = -1;
    field_0x19A4 = 0;
    field_0x19A5 = 0;
    field_0x19A6 = 1;
    field_0xF = 0;
    field_0x10 = 0;
    static const s8 pictures[6][4] = {{3, 4, 3, 2}, {4, 4, 3, 0}, {3, 3, 3, 0}, {3, 3, 3, 0}, {3, 3, 3, 3}, {0, 0, 0, 0}};
    for (int mission = 0; mission < 6; mission++) {
        for (int stage = 0; stage < 4; stage++) {
            m_journalPictures[mission][stage] = pictures[mission][stage];
        }
    }
    field_0x19AA = 1;
    field_0x19AB = 1;
    m_mission = 6;
    m_stage = 1;
    field_0xC = 6;
    field_0xD = 1;
    m_stars.m_bits[5] &= ~0xC0;  // mission 6 stage 1: no stars
    m_unlocked.m_bits[2] |= 0x08;  // mission 6 stage 1 unlocked
    m_medals[0] &= ~0x04;
    field_0x13F8 = 0;
    field_0x19A8 = 0;
    field_0x19A9 = 0;
    field_0x218 &= ~SHELL_FLAG_08;
    field_0x218 &= ~SHELL_FLAG_04;
    memset(m_moviesUnlocked, 0, sizeof(m_moviesUnlocked));
    memset(m_extrasUnlocked, 0, sizeof(m_extrasUnlocked));
    for (int i = 0; i < 4; i++) {
        g_ControllerConfig.GetDefaultCustomControllerValues(m_customButtons[i]);
    }
    m_mpStats[0].m_lastHitBy = -1;
    m_mpStats[1].m_lastHitBy = -1;
    m_mpStats[2].m_lastHitBy = -1;
    m_mpStats[3].m_lastHitBy = -1;
    ResetMPSetup(this);
}

// A new game: progress, stats, cheats and the multiplayer setup.
void CShellMenu::ResetShell() {
    field_0x254 = 0;
    m_objectivesDone = 0;
    memset(m_secrets, 0, sizeof(m_secrets));
    memset(m_medals, 0, sizeof(m_medals));
    memset(&m_stars, 0, sizeof(m_stars));
    memset(&m_unlocked, 0, sizeof(m_unlocked));
    g_cheats.m_invincible = 0;
    g_cheats.field_0x1 = 0;
    g_cheats.field_0x2 = 0;
    g_cheats.field_0x3 = 0;
    g_cheats.field_0x4 = 0.0f;
    g_cheats.field_0x8 = 0;
    g_cheats.field_0x9 = 0;
    g_cheats.field_0xA = 0;
    g_cheats.field_0xB = 0;
    g_cheats.field_0xD = 0;
    g_cheats.field_0xE = 0;
    g_cheats.field_0xC = 0;
    g_cheats.field_0xF = 0;
    g_cheats.field_0x10 = 0;
    memset(&m_levelStats, 0, sizeof(m_levelStats));
    memset(&m_careerStats, 0, sizeof(m_careerStats));
    field_0x218 &= ~SHELL_CONTROLS_SET;
    field_0x218 &= ~SHELL_DIFFICULTY_SET;
    field_0x21C = 1;
    SetLanguage(this, field_0x21C);
    m_languageSuffix = "";
    field_0x18EC = 0;
    field_0x19A5 = 0;
    field_0x19A6 = 1;
    field_0xF = 0;
    field_0x10 = 0;
    field_0x19AA = 1;
    field_0x19AB = 1;
    m_mission = 6;
    m_stage = 1;
    field_0xC = 6;
    field_0xD = 1;
    m_stars.m_bits[5] &= ~0xC0;
    m_unlocked.m_bits[2] |= 0x08;
    m_medals[0] &= ~0x04;
    field_0x13F8 = 0;
    field_0x218 &= ~SHELL_FLAG_08;
    field_0x218 &= ~SHELL_FLAG_04;
    field_0x19A9 = 0;
    memset(m_moviesUnlocked, 0, sizeof(m_moviesUnlocked));
    memset(m_extrasUnlocked, 0, sizeof(m_extrasUnlocked));
    ResetMPSetup(this);
}

CShellMenu::~CShellMenu() {
    if (m_pMemoryCard != NULL) {
        delete m_pMemoryCard;
        m_pMemoryCard = NULL;
    }
}

// The loading splash (by why the game restarted), the memory card, the menu
// script and its first screen, the font and the controller config.
void CShellMenu::ShellInit(unsigned long frames) {
    const char* splashName;
    char name[0x40];
    switch (LoadElf::Get()->m_reason) {
    case 2:
        splashName = "data/victory.gsh";
        break;
    case 1:
    case 3:
        splashName = "data/return.gsh";
        break;
    case 4: {
        int stage = LoadElf::Get()->m_stage;
        sprintf(name, "data/vict%d_%d.gsh", LoadElf::Get()->m_mission, stage);
        splashName = name;
        break;
    }
    default:
        splashName = "data/start.gsh";
        break;
    }
    CSprite* splash = new CSprite(splashName, (CSprite::ESpriteLoadType)1, (ERenderPriority)0x34);
    _spritepolyvert verts[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0xFF, 0xFF, 0xFF, 0xFF}},
        {1.0f, 0.0f, 640.0f, 0.0f, 0.0f, {0xFF, 0xFF, 0xFF, 0xFF}},
        {1.0f, 1.0f, 640.0f, 448.0f, 0.0f, {0xFF, 0xFF, 0xFF, 0xFF}},
        {0.0f, 1.0f, 0.0f, 448.0f, 0.0f, {0xFF, 0xFF, 0xFF, 0xFF}},
    };
    for (int i = 0; i < 2; i++) {
        splash->DrawPoly(verts, 4, CSprite::POLY_TRIANGLE_FAN);
        UpdateREAL();
        WaitRenderList();
        WaitScreen();
        SendRenderList();
        g_inputMgr.Update(1.0f);
    }
    if (splash != NULL) {
        delete splash;
    }

    field_0x19A2 = -1;
    field_0x19A3 = -1;
    field_0x19A4 = 0;
    m_pMemoryCard = new CMemoryCard;
    g_soundStreamFilename[0] = 0;
    CConfigFile config(g_ConfigFilename);
    if (config.SetSection("shell menu")) {
        config.GetString("filename", g_soundStreamFilename, sizeof(g_soundStreamFilename));
        field_0x19A6 = config.GetBool("bPromptsOn", true);
    } else {
        strcpy(g_soundStreamFilename, "data\\shell\\Shell1.asf");
    }
    LoadShellBigFile();
    m_studio.ISirenStartup(0x32, 8, 0x1E, 0xC8, 0x10);

    // The first screen: back from a level shows its result.
    u16 screen = 0;
    if (field_0x18EC) {
        m_messages.Load("data\\chars.bum", "data\\binum.bum");
        if (m_mpEnabled == 0) {
            screen = field_0xF ? 0xE : 0xF;
            if (field_0xF) {
                u8 lastStage[7] = {0, 4, 3, 3, 3, 4, 2};
                screen = lastStage[m_mission] == m_stage ? 0x23 : 0xE;
                if (m_mission == 6 && m_stage == 1) {
                    screen = 0xF;
                    SetStarRatingStatus(&m_stars, m_stage + m_mission * 10, 3);
                }
            }
        } else {
            screen = 0x2A;
            MoveStatsIntoProperAreaForIstudio();
        }
    }
    LoadShellSoundModules();
    m_pMemoryCard->Initialize();
    if (!field_0x18EC) {
        m_messages.Load("data\\chars.bum", "data\\binum.bum");
        MemoryCardBootUpCheck();
        if (!MemoryCardAutoLoad(0)) {
            MemoryCardAutoLoad(1);
        }
    }
    m_studio.iSirenLoadScreen(0, screen);
    m_studio.iSirenUpdate(frames);
    m_studio.iSirenActivateScreen(0, screen);

    m_pUnpluggedSprite = new CSprite("unplugged.gsh", (CSprite::ESpriteLoadType)2, (ERenderPriority)0x66);
    void* fontData = TLT_LoadFileNormal("data/objFont.gfn", NULL, 0);
    m_pFont = new CFont((const char*)fontData, (ERenderPriority)0x6F);
    m_pFont->SetColor(CColor(0x3F, 0x3F, 0x3F, 0x80));
    m_pFont->SetScale(1.0f, 1.0f);
    m_pFont->SetDepth(g_screen.GetDepthBufferMax());
    int unlocked[6 * 4];
    memset(unlocked, 0, sizeof(unlocked));
    GetLevelUnlockedStatus(unlocked);
    if (field_0x18EC && field_0xF) {
        GetNextLevel();
    }
    g_ControllerConfig.m_messages = &m_messages;
    g_ControllerConfig.Init();
    m_mpEnabled = 0;
    if (!field_0x18EC) {
        field_0x143C[0] = 100;
        field_0x143C[1] = 100;
        field_0x143C[2] = 100;
        field_0x143C[3] = 100;
    }
    ResetMPStats(this);
}

void CShellMenu::SetMoviePlaytime() {
    m_moviePlayTime = GetGameTime() + 30.0f;
}

// The menus, the shell music stream (requeued when it ends) and the
// missing-controller message.
void CShellMenu::ShellUpdate(unsigned long frames) {
    field_0x19A7 = 0;
    if (m_shellOn) {
        m_studio.iSirenUpdate(frames);
        if (g_ControllerConfig.m_active) {
            g_ControllerConfig.RunNewConfigRendererer();
        }
    }
    if (g_soundStreamRequest != -1) {
        SSndStreamStatus status;
        SSndStrmRequestStatus request;
        {
            CSoundSysLock lock;
            pSoundStream->GetStatus(status);
            pSoundStream->GetRequestStatus(g_soundStreamRequest, request);
        }
        if (request.m_state == 3) {
            g_soundStreamRequest = pSoundStream->QueueFile(g_soundStreamFilename, 0x32, 0);
        }
    }
    if (field_0x19A7 && !field_0x199B) {
        CScreen* screen = CScreen::GetCurrent();
        screen->GetWidth();
        m_pFont->DrawTextCentered((float)screen->GetHeight() * 0.5f, "No Controller Inserted\n");
        float cx = (float)(screen->GetWidth() / 2);
        float cy = (float)(screen->GetHeight() / 2);
        CSprite* sprite = m_pUnpluggedSprite;
        float halfH = 0.5f * (float)sprite->m_texture.m_height;
        float halfW = 0.5f * (float)(s16)sprite->m_texture.m_width;
        static _spritepolyvert verts[4] = {
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        };
        static bool init;
        if (!init) {
            init = true;
            verts[0].x = cx - halfW;
            verts[0].y = cy - halfH;
            verts[1].x = cx + halfW;
            verts[1].y = cy - halfH;
            verts[2].x = cx + halfW;
            verts[2].y = cy + halfH;
            verts[3].x = cx - halfW;
            verts[3].y = cy + halfH;
        }
        m_pUnpluggedSprite->DrawPoly(verts, 4, CSprite::POLY_TRIANGLE_FAN);
    }
}

void CShellMenu::ShellDraw(unsigned long frames) {
    if (m_shellOn) {
        m_studio.iSirenDrawProcessFnc(frames);
    }
}

bool CShellMenu::bIsShellOn() {
    return m_shellOn;
}

// Fades the shell music out and stops it.
void CShellMenu::TurnShellOff() {
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        pSoundStream->GetVolume();
        pSoundStream->FadeVolume(0x5DC, 0);
    }
    if (pSoundStream != NULL) {
        int volume;
        do {
            UpdateREAL();
            CSoundSysLock lock;
            volume = pSoundStream->GetVolume();
        } while (volume != 0);
    }
    field_0x19A9 = 1;
    if (pSoundStream != NULL) {
        pSoundStream->PurgeQueue();
        g_soundStreamRequest = -1;
        delete pSoundStream;
        pSoundStream = NULL;
    }
    m_shellOn = 0;
}

// Starts the shell music and applies the volume options.
void CShellMenu::TurnShellOn() {
    m_shellOn = 1;
    pSoundStream = new CSoundStream(1, 0x32);
    if (g_soundStreamFilename[0] != 0) {
        g_soundStreamRequest = pSoundStream->QueueFile(g_soundStreamFilename, 0x32, 0);
        CSoundSysLock lock;
        pSoundStream->SetVolume((int)((127.0f * (float)(m_musicVolume - 1)) / 6.0f));
    }
    SoundSetVolumes((float)((s8)field_0x28 - 1) / 6.0f, (float)(m_musicVolume - 1) / 6.0f);
}

void CShellMenu::ShutdownShell() {
    if (m_pFont != NULL) {
        delete m_pFont;
    }
    if (m_pUnpluggedSprite != NULL) {
        delete m_pUnpluggedSprite;
    }
    UnloadShellSoundModules();
    m_studio.iSirenShutDown();
    delete m_pMemoryCard;
    m_pMemoryCard = NULL;
    m_messages.Unload();
    UnloadShellBigFile();
}

void CShellMenu::Set_currentControllerSetting(unsigned char setting) {
    m_controllerSetting = setting;
}

// Until a new game starts, easy (1) also sets the easy option.
void CShellMenu::Set_currentDifficulty(unsigned char difficulty) {
    m_difficulty = difficulty;
    if (!(field_0x218 & SHELL_NEWGAME)) {
        if (m_difficulty == 1) {
            m_crosshair = 1;
        } else {
            m_crosshair = 0;
        }
        if (field_0x19A8) {
            field_0x218 |= SHELL_NEWGAME;
        }
    }
}

void CShellMenu::Set_currentMission(unsigned int mission) {
    m_mission = mission;
}

void CShellMenu::Set_currentStage(unsigned int stage) {
    m_stage = stage;
}

u8 CShellMenu::Get_currentControllerSetting() {
    return m_controllerSetting;
}

u8 CShellMenu::Get_currentDifficulty() {
    return m_difficulty;
}

int CShellMenu::Get_currentMission() {
    return m_mission;
}

int CShellMenu::Get_currentStage() {
    return m_stage;
}

// out[9]: medals won (the 7th is 2 bits).
void CShellMenu::GetMedalStatus(int* out) {
    int value = 0;
    for (int i = 0; i < 9; i++) {
        switch (i + 1) {
        case 1:
            value = (m_medals[0] >> 7) & 1;
            break;
        case 2:
            value = (m_medals[0] >> 6) & 1;
            break;
        case 3:
            value = (m_medals[0] >> 5) & 1;
            break;
        case 4:
            value = (m_medals[0] >> 4) & 1;
            break;
        case 5:
            value = (m_medals[0] >> 3) & 1;
            break;
        case 6:
            value = (m_medals[0] >> 2) & 1;
            break;
        case 7:
            value = m_medals[0] & 3;
            break;
        case 8:
            value = (m_medals[1] >> 7) & 1;
            break;
        case 9:
            value = (m_medals[1] >> 6) & 1;
            break;
        case 10:
            value = (m_medals[1] >> 5) & 1;
            break;
        }
        *out++ = value;
    }
}

// Secret `index` (1..10) found state (2 bits).
void CShellMenu::SetIndividualSecretsStatus(int index, int value) {
    int byte = (index - 1) / 4;
    int shift = 6 - ((index - 1) % 4) * 2;
    if (index < 1 || index > 10) {
        return;
    }
    m_secrets[byte] = (m_secrets[byte] & ~(3 << shift)) | ((value << shift) & (3 << shift));
}

// out[10]: secrets' found state.
void CShellMenu::GetSecretsStatus(int* out) {
    int value = 0;
    for (int i = 0; i < 10; i++) {
        value = (m_secrets[i / 4] >> (6 - (i % 4) * 2)) & 3;
        *out++ = value;
    }
}

void CShellMenu::ResetCurrentLevelStats() {
    m_levelStats.m_accuracy = 0.0f;
    m_levelStats.m_enemiesKilled = 0;
    m_levelStats.m_evaluation = 0;
    m_levelStats.m_hits = 0;
    m_levelStats.m_hitsTaken = 0;
    m_levelStats.m_unk0xC = 0;
    m_levelStats.m_rating = 0;
    m_levelStats.m_bestWeapon = 0;
    for (int i = 0; i < NUM_SHOT_LOCATIONS; i++) {
        m_levelStats.m_shotLocations[i] = 0;
    }
    for (int i = 0; i < NUM_STAT_WEAPONS; i++) {
        m_levelStats.m_weaponKills[i] = 0;
    }
    m_levelStats.m_shotsFired = 0;
    m_levelStats.m_time = 0;
    m_levelStats.m_minutes = 0;
    m_objectivesDone = 0;
}

// Adds to a level counter (they stop at STAT_MAX).
static inline void AddStat(u32& stat, u32 count) {
    if (stat < STAT_MAX) {
        stat += count;
    }
}

void CShellMenu::Set_shotsFired(unsigned int count, bool) {
    AddStat(m_levelStats.m_shotsFired, count);
}

void CShellMenu::Set_hits(unsigned int count, bool) {
    AddStat(m_levelStats.m_hits, count);
}

void CShellMenu::Set_hitsTaken(unsigned int count, bool) {
    AddStat(m_levelStats.m_hitsTaken, count);
}

// Also counts the kill for the player's current weapon.
void CShellMenu::Set_enemiesKilled(unsigned int count, bool) {
    AddStat(m_levelStats.m_enemiesKilled, count);
    CPlayerObject* player = g_scene.GetPlayer(0);
    if (player->GetCurrentWeapon() != NULL) {
        AddStat(m_levelStats.m_weaponKills[player->GetCurrentWeapon()->m_weaponType], count);
    }
}

void CShellMenu::Set_shotLocation(unsigned int count, EShotLocations location, bool) {
    AddStat(m_levelStats.m_shotLocations[location], count);
}

void CShellMenu::ResetMPPlayersStats() {
    ResetMPStats(this);
}

// Adds to a match counter (clamped to STAT_MAX).
static inline void AddMPStat(s32& stat, int count) {
    stat += count;
    if (stat >= STAT_MAX) {
        stat = STAT_MAX;
    }
}

void CShellMenu::Add_MPShotFired(int player, int count) {
    if (!g_bMPEndLevel) {
        AddMPStat(m_mpStats[player].m_shotsFired, count);
    }
}

void CShellMenu::Add_MPShotLocation(int shooter, int victim, int count, EShotLocations location) {
    if (!g_bMPEndLevel) {
        AddMPStat(m_mpStats[shooter].m_shotLocations[location], count);
        m_mpStats[victim].m_lastHitBy = shooter;
    }
}

// Kill statistics and team scores; the score limit ends the match and the
// leading team gets the star.
void CShellMenu::Add_MPPlayerKilled(int killer, int victim) {
    if (g_bMPEndLevel) {
        return;
    }
    MPPlayerStats& stats = m_mpStats[killer];
    s32& weaponKills = stats.m_weaponKills[g_scene.GetPlayer(killer)->GetCurrentWeapon()->m_weaponType];
    if (weaponKills < STAT_MAX) {
        weaponKills++;
    }
    AddMPStat(stats.m_killedPlayer[g_scene.GetPlayer(victim)->m_controllerPort], 1);
    AddMPStat(m_mpStats[victim].m_deaths, 1);
    int team = g_scene.GetPlayer(killer)->m_team;
    if (team == g_scene.GetPlayer(victim)->m_team) {
        AddMPStat(stats.m_teamKills, 1);
    } else {
        AddMPStat(stats.m_kills, 1);
    }
    stats.m_score = stats.m_kills - stats.m_teamKills;

    int teamScore = 0;
    for (int i = 0; i < m_mpNumPlayers; i++) {
        if (team == g_scene.GetPlayer(i)->m_team) {
            teamScore += m_mpStats[i].m_score;
        }
    }
    for (int i = 0; i < m_mpNumPlayers; i++) {
        if (team == g_scene.GetPlayer(i)->m_team) {
            m_mpStats[i].m_goalScore = teamScore;
        }
    }
    if (field_0x1430 != 0 && field_0x1428 <= teamScore) {
        g_bMPEndLevel = 1;
    }
    int best = -0x7FFF;
    for (int i = 0; i < m_mpNumPlayers; i++) {
        if (m_mpStats[i].m_goalScore > best) {
            best = m_mpStats[i].m_goalScore;
        }
    }
    for (int i = 0; i < m_mpNumPlayers; i++) {
        if (m_mpStats[i].m_goalScore == best) {
            m_mpStats[i].m_star = 1;
        } else {
            m_mpStats[i].m_star = 0;
        }
    }
}

int CShellMenu::Get_MPPlayerScore(int player) {
    return m_mpStats[player].m_score;
}

MPPlayerStats* CShellMenu::GetMPPlayerStats(int player) {
    return &m_mpStats[player];
}

void CShellMenu::SetPlayersFinishedLevelSuccessfully(int mission, int stage) {
    m_finishedLevels[mission - 1][stage - 1].m_finished = 1;
}

void CShellMenu::SetPlayersFinishedLevelHealth(int mission, int stage, float health) {
    m_finishedLevels[mission - 1][stage - 1].m_health = health;
}

void CShellMenu::SetPlayersFinishedLevelAmmo(int mission, int stage, EWeaponTypes type, unsigned int ammo) {
    m_finishedLevels[mission - 1][stage - 1].m_ammo[type] = ammo;
}

float CShellMenu::GetPlayersFinishedLevelHealth(int mission, int stage) {
    return m_finishedLevels[mission - 1][stage - 1].m_health;
}

u32 CShellMenu::GetPlayersFinishedLevelAmmo(int mission, int stage, EWeaponTypes type) {
    return m_finishedLevels[mission - 1][stage - 1].m_ammo[type];
}

// A later stage starts with what the previous stage was finished with.
bool CShellMenu::ShouldIUsePlayersFinishedLevelData(int mission, int stage) {
    if (stage > 1 && mission < 7) {
        return m_finishedLevels[mission - 1][stage - 2].m_finished;
    }
    return false;
}

void CShellMenu::ResetLevelTimer() {
    m_levelStats.m_time = 0;
}

void CShellMenu::UpdateLevelTimer(CPlayerObject* player, unsigned int frames) {
    if (!player->m_hideWeapon) {
        m_levelStats.m_time += frames;
    }
}

void iDebugMsg(const char*, ...) {}

// Rounded minutes of `frames` (60 per second... as the original counts).
static inline u32 StatMinutes(u32 time) {
    return time / 60 + ((time / 30) & 1);
}

// The body area hit most (0..6; ties keep the first).
static inline int MostHitLocation(const SLevelStats& stats) {
    u32 best = 0;
    int location = 0;
    for (int i = 0; i < NUM_SHOT_LOCATIONS; i++) {
        if (stats.m_shotLocations[i] > best) {
            best = stats.m_shotLocations[i];
            location = i;
        }
    }
    return location;
}

static inline float Accuracy(const SLevelStats& stats) {
    if (stats.m_shotsFired != 0) {
        return 100.0f * ((float)stats.m_hits / (float)stats.m_shotsFired);
    }
    return 0.0f;
}

void CShellMenu::CalculateLevelStats() {
    m_levelStats.m_accuracy = Accuracy(m_levelStats);
    u8 bestWeapon = 0;
    u32 best = 0;
    for (u8 i = 0; i < NUM_STAT_WEAPONS; i++) {
        iDebugMsg("Current hit stats for Weapon[ %d ] = %d \n", i, m_levelStats.m_weaponKills[i]);
        if (m_levelStats.m_weaponKills[i] > best) {
            best = m_levelStats.m_weaponKills[i];
            bestWeapon = i;
        }
    }
    m_levelStats.m_bestWeapon = bestWeapon;
    m_levelStats.m_evaluation = MostHitLocation(m_levelStats) * 5 + rand() % 5;
    m_levelStats.m_minutes = StatMinutes(m_levelStats.m_time);
}

static inline void ClampStat(u32& stat) {
    if (stat > STAT_MAX) {
        stat = STAT_MAX;
    }
}

// The level's result for the menu: rating (stars), statistics; adds the
// level to the career statistics.
u32* CShellMenu::GetLevelStats() {
    if (m_mission != 7) {
        u8 enemies = field_0x10 != 0 ? field_0x10 : 1;
        float killed = ((float)m_levelStats.m_enemiesKilled / (float)enemies) * 100.0f;
        if (killed < 0.0f) {
            killed = 0.0f;
        }
        if (killed > 100.0f) {
            killed = 100.0f;
        }
        if (killed >= 95.0f && m_finishedLevels[m_mission - 1][m_stage - 1].m_health >= 75.0f) {
            m_levelStats.m_rating = 3;
        } else if (killed >= 95.0f) {
            m_levelStats.m_rating = 2;
        } else {
            m_levelStats.m_rating = 1;
        }
        SetStarRatingStatus(&m_stars, m_stage + m_mission * 10, m_levelStats.m_rating);
    }
    u32* out = m_statsOut;
    out[0] = m_stage + m_mission * 10;
    out[1] = m_levelStats.m_rating;
    out[2] = m_objectivesDone;
    out[3] = m_levelStats.m_shotsFired;
    out[4] = m_levelStats.m_hits;
    out[5] = (u32)m_levelStats.m_accuracy;
    out[6] = m_levelStats.m_bestWeapon;
    out[7] = m_levelStats.m_hitsTaken;
    out[8] = m_levelStats.m_unk0xC;
    out[9] = m_levelStats.m_enemiesKilled;
    out[10] = 0;
    out[11] = m_levelStats.m_shotLocations[0];
    out[12] = m_levelStats.m_shotLocations[3];
    out[13] = m_levelStats.m_shotLocations[6];
    out[14] = m_levelStats.m_shotLocations[4];
    out[15] = m_levelStats.m_shotLocations[1];
    out[16] = m_levelStats.m_shotLocations[5];
    out[17] = m_levelStats.m_shotLocations[2];
    out[18] = m_levelStats.m_evaluation;
    out[19] = StatMinutes(m_levelStats.m_time);
    bool unlockedMovie = CheckIfMovieAndMedalNeedsToBeUnlocked();
    bool unlockedMedal = CheckIfSpecialMedalsNeedToBeUnlocked();
    out[20] = unlockedMovie || unlockedMedal;
    for (u32 i = 0; i < 0x15; i++) {
        if (i != 2 && i != 10 && i != 20) {
            ClampStat(out[i]);
        }
    }

    AddStat(m_careerStats.m_shotsFired, m_levelStats.m_shotsFired);
    AddStat(m_careerStats.m_hits, m_levelStats.m_hits);
    AddStat(m_careerStats.m_hitsTaken, m_levelStats.m_hitsTaken);
    AddStat(m_careerStats.m_unk0xC, m_levelStats.m_unk0xC);
    AddStat(m_careerStats.m_enemiesKilled, m_levelStats.m_enemiesKilled);
    AddStat(m_careerStats.m_shotLocations[0], m_levelStats.m_shotLocations[0]);
    AddStat(m_careerStats.m_shotLocations[3], m_levelStats.m_shotLocations[3]);
    AddStat(m_careerStats.m_shotLocations[6], m_levelStats.m_shotLocations[6]);
    AddStat(m_careerStats.m_shotLocations[4], m_levelStats.m_shotLocations[4]);
    AddStat(m_careerStats.m_shotLocations[1], m_levelStats.m_shotLocations[1]);
    AddStat(m_careerStats.m_shotLocations[5], m_levelStats.m_shotLocations[5]);
    AddStat(m_careerStats.m_shotLocations[2], m_levelStats.m_shotLocations[2]);
    for (u32 i = 0; i < NUM_STAT_WEAPONS; i++) {
        m_careerStats.m_weaponKills[i] += m_levelStats.m_weaponKills[i];
    }
    return m_statsOut;
}

// The career statistics for the menu (a new evaluation once more shots
// were fired since the last one).
u32* CShellMenu::GetCumulativeStats() {
    m_careerStats.m_accuracy = Accuracy(m_careerStats);
    m_careerStats.m_rating = (int)((float)m_careerStats.m_rating / 17.0f);
    u8 bestWeapon = 0;
    u32 best = 0;
    for (u8 i = 0; i < NUM_STAT_WEAPONS; i++) {
        if (m_careerStats.m_weaponKills[i] > best) {
            best = m_careerStats.m_weaponKills[i];
            bestWeapon = i;
        }
    }
    m_careerStats.m_bestWeapon = bestWeapon;
    if (field_0x13F8 < m_careerStats.m_shotsFired) {
        m_careerStats.m_evaluation = MostHitLocation(m_careerStats) * 5 + rand() % 5;
    }
    field_0x13F8 = m_careerStats.m_shotsFired;
    u32* out = m_statsOut;
    out[0] = m_careerStats.m_shotsFired;
    out[1] = m_careerStats.m_hits;
    out[2] = (u32)m_careerStats.m_accuracy;
    out[3] = m_careerStats.m_bestWeapon;
    out[4] = m_careerStats.m_hitsTaken;
    out[5] = m_careerStats.m_unk0xC;
    out[6] = m_careerStats.m_enemiesKilled;
    out[8] = m_careerStats.m_shotLocations[0];
    out[9] = m_careerStats.m_shotLocations[3];
    out[10] = m_careerStats.m_shotLocations[6];
    out[11] = m_careerStats.m_shotLocations[4];
    out[12] = m_careerStats.m_shotLocations[1];
    out[13] = m_careerStats.m_shotLocations[5];
    out[14] = m_careerStats.m_shotLocations[2];
    out[15] = m_careerStats.m_evaluation;
    for (int i = 0; i < 16; i++) {
        ClampStat(out[i]);
    }
    return m_statsOut;
}

const char* CShellMenu::GetEvalMessage() {
    m_levelStats.m_evaluation = MostHitLocation(m_levelStats) * 5 + rand() % 5;
    if (m_levelStats.m_shotsFired == 0) {
        return emptyString;
    }
    return Message(m_levelStats.m_evaluation + MSG_EVALUATION);
}

// The weapon with the most kills, or -1 for none.
static inline int MostUsedWeapon(const SLevelStats& stats) {
    u32 best = 0;
    int weapon = -1;
    for (u8 i = 0; i < NUM_STAT_WEAPONS; i++) {
        if (stats.m_weaponKills[i] > best) {
            best = stats.m_weaponKills[i];
            weapon = i;
        }
    }
    return weapon;
}

const char* CShellMenu::GetWeaponMessage() {
    int weapon = MostUsedWeapon(m_levelStats);
    if (m_levelStats.m_shotsFired == 0 || weapon == -1) {
        return emptyString;
    }
    m_levelStats.m_bestWeapon = weapon;
    return Message(m_levelStats.m_bestWeapon + MSG_BEST_WEAPON);
}

const char* CShellMenu::GetEvalMessageForTotalStats() {
    bool none;
    if (m_careerStats.m_shotLocations[0] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[3] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[6] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[4] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[1] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[5] != 0) {
        none = false;
    } else if (m_careerStats.m_shotLocations[2] != 0) {
        none = false;
    } else {
        none = true;
    }
    if (none) {
        return emptyString;
    }
    return Message(m_careerStats.m_evaluation + MSG_EVALUATION);
}

const char* CShellMenu::GetWeaponMessageForTotalStats() {
    int weapon = MostUsedWeapon(m_careerStats);
    if (m_careerStats.m_shotsFired == 0 || weapon == -1) {
        return emptyString;
    }
    m_careerStats.m_bestWeapon = weapon;
    return Message(m_careerStats.m_bestWeapon + MSG_BEST_WEAPON);
}

// The memory card slot `port` is usable (opened if it isn't the current one).
static inline bool UsePort(CMemoryCard* card, int port) {
    return port == card->GetPort() || card->OpenSlot(port);
}

bool CShellMenu::MemoryCardQueryPort(int port) {
    return m_pMemoryCard->OpenSlot(port);
}

int CShellMenu::MemoryCardIsFormated(int port) {
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    return m_pMemoryCard->IsFormated();
}

bool CShellMenu::MemoryCardFormat(int port) {
    if (!UsePort(m_pMemoryCard, port)) {
        return false;
    }
    return m_pMemoryCard->Format();
}

bool CShellMenu::MemoryCardArePrefsPresent(int port) {
    if (!UsePort(m_pMemoryCard, port)) {
        return false;
    }
    return m_pMemoryCard->ArePrefsPresent();
}

void CShellMenu::MemoryCardRewindSaveGameList(int port) {
    if (UsePort(m_pMemoryCard, port)) {
        m_pMemoryCard->RewindSaveGameList();
    }
}

int CShellMenu::MemoryCardGetNextSaveGame(int port, unsigned int* slot, unsigned int* size, char* name) {
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    return m_pMemoryCard->GetNextSaveGame(slot, size, name);
}

bool CShellMenu::MemoryCardDeleteSaveGame(int port, unsigned int slot) {
    if (!UsePort(m_pMemoryCard, port)) {
        return false;
    }
    return m_pMemoryCard->DeleteSaveGame(slot);
}

// CRC-32 (reflected, polynomial 0x04C11DB7) of the shell's save data.
class CShellCRC {
public:
    CShellCRC() {
        m_width = 32;
        m_poly = 0x04C11DB7;
        m_init = 0xFFFFFFFF;
        m_xorOut = 0xFFFFFFFF;
        m_reflect = 1;
        m_mask = 0xFFFFFFFF;
        m_crc = m_init;
    }
    void Add(const u8* data, u32 size) {
        for (; size != 0; data++, size--) {
            u32 top = 1 << (m_width - 1);
            u8 byte = *data;
            if (m_reflect) {
                u8 in = byte;
                for (int i = 0; i < 8; i++) {
                    if (in & 1) {
                        byte |= 1 << (7 - i);
                    } else {
                        byte &= ~(1 << (7 - i));
                    }
                    in >>= 1;
                }
            }
            m_crc ^= (u32)byte << (m_width - 8);
            for (int i = 0; i < 8; i++) {
                if (m_crc & top) {
                    m_crc = (m_crc << 1) ^ m_poly;
                } else {
                    m_crc <<= 1;
                }
                m_crc &= m_mask;
            }
        }
    }
    u32 Result() const {
        u32 crc = m_crc;
        if (m_reflect) {
            u32 in = m_crc;
            for (u32 i = 0; i < m_width; i++) {
                u32 bit = 1 << (m_width - 1 - i);
                if (in & 1) {
                    crc |= bit;
                } else {
                    crc &= ~bit;
                }
                in >>= 1;
            }
        }
        return m_xorOut ^ crc;
    }

    /* 0x00 */ u32 m_width;
    /* 0x04 */ u32 m_mask;
    /* 0x08 */ u32 m_poly;
    /* 0x0C */ u32 m_init;
    /* 0x10 */ u32 m_reflect;
    /* 0x14 */ u32 m_xorOut;
    /* 0x18 */ u32 m_crc;
};

static inline u32 ShellDataCRC(const u8* data, u32 size) {
    CShellCRC crc;
    crc.Add(data, size);
    return crc.Result();
}

// Replaces the save game in `slot` with the shell's data and points the
// preferences at it. -1: the card failed (erase/program error).
int CShellMenu::MemoryCardSaveGame(int port, int slot, char* name) {
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    if (m_pMemoryCard->IsSaveGamePresent(slot) && !m_pMemoryCard->DeleteSaveGame(slot)) {
        if (m_pMemoryCard->IsFormated() == 3) {
            DebugMsg("Detected Erase Error!\n");
            return -1;
        }
        return 0;
    }
    m_crc = ShellDataCRC((const u8*)this, SHELL_CRC_SIZE);
    unsigned int newSlot;
    if (!m_pMemoryCard->CreateNewSaveGame(&newSlot, name)) {
        if (m_pMemoryCard->IsFormated() == 3) {
            DebugMsg("Detected Program Error!\n");
            return -1;
        }
        return 0;
    }
    if (!m_pMemoryCard->Open(newSlot, true)) {
        return 0;
    }
    if (!m_pMemoryCard->Write((u8*)this, SHELL_SAVE_SIZE)) {
        m_pMemoryCard->Close();
        return 0;
    }
    if (!m_pMemoryCard->Close()) {
        return 0;
    }
    m_saveSlot = newSlot;
    if (!m_pMemoryCard->ArePrefsPresent()) {
        int needed = m_pMemoryCard->GetSaveGameOverheadSize(false);
        int available = UsePort(m_pMemoryCard, port) ? m_pMemoryCard->GetAvailableMemory() : 0;
        if (available < needed) {
            return 1;
        }
    }
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    if (!m_pMemoryCard->ArePrefsPresent()) {
        m_pMemoryCard->CreatePrefs();
    }
    SShellPrefs prefs;
    prefs.m_saveSlot = m_saveSlot;
    strncpy(prefs.m_signature, "Frontline", sizeof(prefs.m_signature));
    return m_pMemoryCard->SavePrefs((u8*)&prefs, sizeof(prefs));
}

bool CShellMenu::MemoryCardIs8KSectorSize(int port) {
    return m_pMemoryCard->Is8KCard(port);
}

bool CShellMenu::MemoryCardIsEncodingCorrect(int port) {
    return m_pMemoryCard->IsEncodingCorrect(port);
}

int CShellMenu::MemoryCardGetMaxBlocks(int port) {
    return m_pMemoryCard->GetMaxBlocksOnCard(port);
}

bool CShellMenu::MemoryCardIsAnyObjectInSlot(int port) {
    return m_pMemoryCard->IsAnyDeviceInPort(port);
}

int CShellMenu::MemoryCardNumFilesFree(int port) {
    return m_pMemoryCard->GetFilesFree(port);
}

void CShellMenu::MemoryCardGetCorruptFileName(char* out, int) {
    strcpy(out, m_pMemoryCard->m_corruptFileName);
}

// Applies the audio options (after loading them).
static inline void ApplyAudio(CShellMenu* shell) {
    SoundSetVolumes((float)((s8)shell->field_0x28 - 1) / 6.0f, (float)(shell->m_musicVolume - 1) / 6.0f);
    {
        CSoundSysLock lock;
        if (pSoundStream != NULL) {
            pSoundStream->SetVolume((int)((127.0f * (float)(shell->m_musicVolume - 1)) / 6.0f));
        }
    }
    switch (shell->m_soundMode) {
    case 0:
        SoundSetMode((ESoundMode)1);
        OSSetSoundMode(1);
        break;
    case 1:
        SoundSetMode((ESoundMode)2);
        OSSetSoundMode(1);
        break;
    case 2:
        SoundSetMode((ESoundMode)0);
        OSSetSoundMode(0);
        break;
    }
}

// Loads save game `slot`: 1 loaded, 0 card error, -1 bad CRC. A mono system
// keeps mono.
int CShellMenu::MemoryCardLoadGame(int port, unsigned int slot) {
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    if (!m_pMemoryCard->Open(slot, false)) {
        return 0;
    }
    u32 size = SHELL_SAVE_SIZE;
    u8* data = new u8[size];
    if (!m_pMemoryCard->Read(data, size)) {
        m_pMemoryCard->Close();
        delete[] data;
        return 0;
    }
    if (!m_pMemoryCard->Close()) {
        delete[] data;
        return 0;
    }
    field_0x21C = 1;
    SetLanguage(this, field_0x21C);
    m_languageSuffix = "";
    u32 crc = 0;
    memcpy(&crc, data + SHELL_CRC_SIZE, sizeof(crc));
    if (CheckDataCRC(data, size - 4, crc)) {
        s8 soundMode = m_soundMode;
        memcpy(this, data, size);
        if (m_soundMode == 2 || soundMode == 2) {
            m_soundMode = soundMode;
        }
        delete[] data;
        ApplyAudio(this);
        return 1;
    }
    DebugMsg("CRC check on load game failed: %x\n", crc);
    delete[] data;
    return -1;
}

// At startup: loads the save game the preferences point at (showing the
// corrupt-file dialog when it fails). True if a game was loaded.
bool CShellMenu::MemoryCardAutoLoad(int port) {
    if (!UsePort(m_pMemoryCard, port)) {
        return false;
    }
    if ((UsePort(m_pMemoryCard, port) ? m_pMemoryCard->IsFormated() : 0) == 0) {
        return false;
    }
    if (!(UsePort(m_pMemoryCard, port) ? m_pMemoryCard->ArePrefsPresent() : false)) {
        return false;
    }
    bool prefsOk;
    if (!UsePort(m_pMemoryCard, port)) {
        prefsOk = false;
    } else {
        SShellPrefs prefs;
        if (!m_pMemoryCard->LoadPrefs((u8*)&prefs, sizeof(prefs))) {
            DebugMsg("m_MemoryCard->LoadPrefs() return false\n");
            prefsOk = false;
        } else if (strncmp(prefs.m_signature, "Frontline", sizeof(prefs.m_signature)) != 0) {
            DebugMsg("Prefs file is corrupted since I can't find the frontline signature\n");
            prefsOk = false;
        } else {
            prefsOk = true;
            m_saveSlot = prefs.m_saveSlot;
        }
    }
    if (!prefsOk) {
        char fileName[0x80];
        char title[0x80];
        sprintf(fileName, "%sprefs", "MOHFL");
        char letter = port == 1 ? 'B' : 'A';
        sprintf(title, Message(0x33D), letter);
        CorruptFileDialog(title, letter, fileName, port, false, 0, NULL, NULL);
        return false;
    }
    DebugMsg("Memory Card Prefs point to slot %d for auto-load\n", m_saveSlot);
    if (!m_pMemoryCard->IsSaveGamePresent(m_saveSlot)) {
        DebugMsg("Memory Card Prefs auto-load game is not present!\n");
        return false;
    }
    if (MemoryCardLoadGame(port, m_saveSlot) < 1) {
        char line[0x80];
        char fileName[0x80];
        char letter = port == 1 ? 'B' : 'A';
        sprintf(line, Message(0x33F), letter);
        sprintf(fileName, "%sx%02d", "MOHFL", m_saveSlot);
        CorruptFileDialog((char*)Message(0x33E), letter, fileName, port, true, m_saveSlot,
                          m_pMemoryCard->m_corruptFileName, line);
        return false;
    }
    // "Loaded from the Memory Card in Slot A/B" for a second.
    void* fontData = TLT_LoadFileNormal("data\\objFont.gfn", NULL, 0);
    CFont font((const char*)fontData, (ERenderPriority)0x6F);
    font.SetColor(CColor(0x3F, 0x3F, 0x3F, 0x80));
    for (int i = 0; i < 60; i++) {
        font.GetHeight();
        MohDVDErrorTask(0, 0);
        const char* text = Message(0x302);
        const char* slotText = Message(0x303);
        font.DrawTextCentered(60.0f, text);
        font.DrawTextCenteredF(90.0f, "%s%s", slotText, port == 0 ? "A" : "B");
        WaitRenderList();
        WaitScreen();
        SendRenderList();
    }
    TLT_CloseFile(fontData);
    return true;
}

int CShellMenu::MemoryCardSaveGameSize() {
    return m_pMemoryCard->GetSaveGameOverheadSize(false);
}

int CShellMenu::MemoryCardSavePrefsSize() {
    return m_pMemoryCard->GetSaveGameOverheadSize(false);
}

int CShellMenu::MemoryCardFreeSpace(int port) {
    if (!UsePort(m_pMemoryCard, port)) {
        return 0;
    }
    return m_pMemoryCard->GetAvailableMemory();
}

// Checks slot `port` at boot: true if it can take the preferences and a
// save game; otherwise says why through the flags.
bool MemoryCardSlotOkForStartup(CMemoryCard* card, int port, int prefsSize, int saveSize, int* blocksNeeded,
                                bool* unformatted, int* filesNeeded, bool* encodingOk, bool* sectorOk,
                                bool* cardPresent, bool* skip, int* badFormat, bool* openOk, bool* canSave) {
    int files = 0;
    *filesNeeded = 0;
    *blocksNeeded = 0;
    *encodingOk = true;
    *sectorOk = true;
    *cardPresent = true;
    *badFormat = 0;
    *openOk = true;
    *canSave = true;
    *unformatted = false;
    if (!card->QuerySlot(port)) {
        *cardPresent = false;
        *skip = false;
        return true;
    }
    if (*skip) {
        return true;
    }
    if (!card->OpenSlot(port)) {
        *openOk = false;
        return false;
    }
    if (!card->IsEncodingCorrect(port)) {
        *encodingOk = false;
        return false;
    }
    int format = card->IsFormated();
    if (format == 2 || format == 3) {
        *badFormat = format;
        return false;
    }
    if (format == 0) {
        *unformatted = true;
        return false;
    }
    int needed = 0;
    card->GetCheckCardStatus();
    if (!card->Is8KCard(port)) {
        *sectorOk = false;
        return false;
    }
    if (card->GetNumSaveGames() != 0) {
        *canSave = true;
    } else {
        *canSave = false;
    }
    if (!card->ArePrefsPresent()) {
        needed = prefsSize;
        files = 1;
        if ((u32)card->GetAvailableMemory() < 2 || card->GetFilesFree(port) < 1) {
            *canSave = false;
        }
    }
    needed += saveSize;
    if (card->GetAvailableMemory() < needed) {
        *blocksNeeded = needed;
        return false;
    }
    if (card->GetFilesFree(port) < (u32)(files + 1)) {
        *filesNeeded = files + 1;
        return false;
    }
    return true;
}

// A corrupt file: A continues, B deletes it (a message if that fails).
void CShellMenu::CorruptFileDialog(char* title, char, char*, int port, bool isSave, int slot, const char* line2,
                                   const char* line3) {
    float scale = g_screen.m_heightScale;
    CFont font((const char*)TLT_LoadFileNormal("data\\objFont.gfn", NULL, 0), (ERenderPriority)0x6F);
    float top = 50.0f * scale;
    while (true) {
        font.DrawTextCentered(top, title);
        float y = top + 22.0f;
        if (line2 != NULL) {
            font.DrawTextCentered(y, line2);
            y += 22.0f;
        }
        if (line3 != NULL) {
            font.DrawTextCentered(y, line3);
            y += 22.0f;
        }
        font.DrawTextCentered(y, Message(0x346));
        font.DrawTextCentered(y + 22.0f, Message(0x347));
        WaitRenderList();
        WaitScreen();
        SendRenderList();
        g_inputMgr.Update(1.0f);
        if (g_inputMgr.GetValueAnyPlayer(0x80)) {
            return;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7F)) {
            bool deleted;
            if (isSave) {
                deleted = UsePort(m_pMemoryCard, port) ? m_pMemoryCard->DeleteSaveGame(slot) : false;
            } else {
                deleted = m_pMemoryCard->DeletePrefFile(port);
            }
            if (!deleted) {
                do {
                    font.DrawTextCentered(top, Message(0x348));
                    font.DrawTextCentered(top + 22.0f, Message(0x349));
                    WaitRenderList();
                    WaitScreen();
                    SendRenderList();
                    g_inputMgr.Update(1.0f);
                } while (!g_inputMgr.GetValueAnyPlayer(0x80));
            }
            return;
        }
    }
}

// Boot: checks both slots until each is usable, skipped or empty.
void CShellMenu::MemoryCardBootUpCheck() {
    CFont font((const char*)TLT_LoadFileNormal("data\\objFont.gfn", NULL, 0), (ERenderPriority)0x6F);
    font.SetColor(CColor(0x5F, 0x5F, 0x5F, 0x80));
    float scale = g_screen.m_heightScale;
    g_inputMgr.Update(1.0f);
    const char* checking = Message(0x301);
    float top = 50.0f * scale;
    for (int i = 0; i < 60; i++) {
        MohDVDErrorTask(0, 0);
        font.DrawTextCentered(top, checking);
        WaitRenderList();
        WaitScreen();
        SendRenderList();
    }
    bool doneA = false;
    bool doneB = false;
    do {
        bool cardA = true;
        if (!doneA && !m_pMemoryCard->QuerySlot(0)) {
            cardA = false;
        }
        bool cardB = true;
        if (!doneB && !m_pMemoryCard->QuerySlot(1)) {
            cardB = false;
        }
        if (!cardA && !cardB) {
            font.DrawTextCentered(top, "No Memory Card detected.");
            float y = top + 22.0f;
            font.DrawTextCentered(y, "Please insert a Memory Card or press the");
            font.DrawTextCentered(y + 22.0f, "A Button to continue without saving.");
            g_inputMgr.Update(1.0f);
            if (g_inputMgr.GetValueAnyPlayer(0x80)) {
                doneA = true;
                doneB = true;
            }
        } else if (cardA && !doneA) {
            doneA = MemoryCardBootUpPortCheck(0);
        } else if (cardB && !doneB) {
            doneB = MemoryCardBootUpPortCheck(1);
        } else {
            doneA = true;
            doneB = true;
        }
        MohDVDErrorTask(0, 0);
        WaitRenderList();
        WaitScreen();
        SendRenderList();
    } while (!doneA || !doneB);
}

// Draws a user message formatted with the slot letter.
static inline void DrawSlotLine(CShellMenu* shell, CFont& font, char* buffer, float y, int message, char letter) {
    sprintf(buffer, shell->Message(message), letter);
    font.DrawTextCentered(y, buffer);
}

// Boot check of one slot: shows what is wrong with the card until it is
// fixed or skipped (A); B formats the card, or goes to the system menu when
// it's full. False if the slot has no card.
bool CShellMenu::MemoryCardBootUpPortCheck(int port) {
    enum {
        BOOT_NONE,
        BOOT_TO_SYSTEM_MENU,  // B resets to the IPL memory card manager
        BOOT_FORMAT,  // B formats the card
    };
    char letter = port != 0 ? 'B' : 'A';
    bool skip = false;
    float scale = g_screen.m_heightScale;
    int result = 1;
    void* fontData = TLT_LoadFileNormal("data\\objFont.gfn", NULL, 0);
    CFont font((const char*)fontData, (ERenderPriority)0x6F);
    font.SetColor(CColor(0x5F, 0x5F, 0x5F, 0x80));
    g_inputMgr.Update(1.0f);
    float top = 50.0f * scale;
    int lineStep = (int)22.0f;
    char buffer[0x100];
    char formatLine[0x50];
    bool done;
    do {
        int saveSize = m_pMemoryCard->GetSaveGameOverheadSize(false);
        int prefsSize = m_pMemoryCard->GetSaveGameOverheadSize(false);
        int blocksNeeded;
        bool unformatted;
        int filesNeeded;
        bool encodingOk;
        bool sectorOk;
        bool cardPresent;
        int badFormat;
        bool openOk;
        bool canSave;
        bool ok = MemoryCardSlotOkForStartup(m_pMemoryCard, port, prefsSize, saveSize, &blocksNeeded,
                                             &unformatted, &filesNeeded, &encodingOk, &sectorOk, &cardPresent,
                                             &skip, &badFormat, &openOk, &canSave);
        if (skip) {
            break;
        }
        if (ok && cardPresent) {
            done = true;
            continue;
        }
        float y = top;
        done = false;
        CScreen::GetCurrent()->GetWidth();
        MohDVDErrorTask(0, 0);
        int mode = BOOT_NONE;
        if (!openOk) {
            DrawSlotLine(this, font, buffer, top, 0x31E, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x31F));
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x320, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x321));
            font.DrawTextCentered(y + 22.0f, Message(0x322));
        } else if (badFormat != 0) {
            if (badFormat == 3) {
                DrawSlotLine(this, font, buffer, top, 0x34D, letter);
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x34E));
                font.DrawTextCentered(y + 22.0f, Message(0x34F));
            } else {
                DrawSlotLine(this, font, buffer, top, 0x323, letter);
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x324));
                y += 44.0f;
                font.DrawTextCentered(y, Message(0x325));
                y += 22.0f;
                DrawSlotLine(this, font, buffer, y, 0x326, letter);
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x327));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x328));
                y += 22.0f;
                DrawSlotLine(this, font, buffer, y, 0x329, letter);
                y += 22.0f;
                y += 22.0f;
                sprintf(formatLine, "Formatting the Memory Card in Slot %c", letter);
                font.DrawTextCentered(y, formatLine);
                y += 22.0f;
                font.DrawTextCentered(y, "will erase all previously saved data.");
                mode = BOOT_FORMAT;
            }
        } else if (!sectorOk) {
            DrawSlotLine(this, font, buffer, top, 0x32C, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x32D));
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x32E));
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x32F, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x330));
            font.DrawTextCentered(y + 22.0f, Message(0x331));
        } else if (!encodingOk) {
            DrawSlotLine(this, font, buffer, top, 0x334, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x335));
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x336));
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x337));
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x338, letter);
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x339, letter);
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x33A, letter);
            y += 22.0f;
            y += 22.0f;
            sprintf(formatLine, "Formatting the Memory Card in Slot %c", letter);
            font.DrawTextCentered(y, formatLine);
            y += 22.0f;
            font.DrawTextCentered(y, "will erase all previously saved data.");
            mode = BOOT_FORMAT;
        } else if (unformatted) {
            DrawSlotLine(this, font, buffer, top, 0x317, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x318));
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x319));
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x31A, letter);
            y += 22.0f;
            font.DrawTextCentered(y, Message(0x31B));
            y += 22.0f;
            DrawSlotLine(this, font, buffer, y, 0x31C, letter);
            y += 22.0f;
            y += 22.0f;
            // The original passes the check result (0) as the letter.
            sprintf(formatLine, "Formatting the Memory Card in Slot %c", (s8)ok);
            font.DrawTextCentered(y, formatLine);
            y += 22.0f;
            font.DrawTextCentered(y, "will erase all previously saved data.");
            mode = BOOT_FORMAT;
        } else if (!cardPresent) {
            result = 0;
            break;
        } else {
            // Not enough room for the preferences and a save game.
            bool explain = false;
            if (filesNeeded > 0) {
                if (canSave) {
                    int line = (int)top;
                    font.DrawTextCentered((float)line, "Insufficient free files to save a new file on the");
                    line += lineStep;
                    sprintf(buffer, "Memory Card in Slot %c. ", letter);
                    font.DrawTextCentered((float)line, buffer);
                    line += lineStep;
                    font.DrawTextCentered((float)line, "This game requires at least 2 files and 4 blocks to");
                    line += lineStep;
                    font.DrawTextCentered((float)line, "save, 1 file and 2 blocks are needed for the Preference");
                    line += lineStep;
                    font.DrawTextCentered((float)line, "file and 1 file and 2 blocks for each Save Game file.");
                    line += lineStep;
                    font.DrawTextCentered((float)line, "Existing files may be loaded and overwritten.");
                    line += lineStep;
                    font.DrawTextCentered((float)line, "Press the B Button to manage the Memory Card");
                    line += lineStep;
                    font.DrawTextCentered((float)line, "or press the A Button to continue.");
                    line += lineStep;
                    y = (float)line;
                } else {
                    DrawSlotLine(this, font, buffer, top, 0x314, letter);
                    explain = true;
                }
            } else if (canSave) {
                int line = (int)top;
                font.DrawTextCentered((float)line, "Insufficient free blocks to save a new file on the");
                line += lineStep;
                sprintf(buffer, "Memory Card in Slot %c. ", letter);
                font.DrawTextCentered((float)line, buffer);
                line += lineStep;
                font.DrawTextCentered((float)line, "This game requires at least 2 files and 4 blocks to");
                line += lineStep;
                font.DrawTextCentered((float)line, "save, 1 file and 2 blocks are needed for the Preference");
                line += lineStep;
                font.DrawTextCentered((float)line, "file and 1 file and 2 blocks for each Save Game file.");
                line += lineStep;
                font.DrawTextCentered((float)line, "Existing files may be loaded and overwritten.");
                line += lineStep;
                font.DrawTextCentered((float)line, "Press the B Button to manage the Memory Card");
                line += lineStep;
                font.DrawTextCentered((float)line, "or press the A Button to continue.");
                line += lineStep;
                y = (float)line;
            } else {
                DrawSlotLine(this, font, buffer, top, 0x315, letter);
                explain = true;
            }
            if (explain) {
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x30A));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x30B));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x30C));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x30D));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x30E));
                y += 44.0f;
                font.DrawTextCentered(y, Message(0x310));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x311));
                y += 22.0f;
                font.DrawTextCentered(y, Message(0x312));
                font.DrawTextCentered(y + 22.0f, Message(0x313));
            }
            mode = BOOT_TO_SYSTEM_MENU;
        }

        WaitRenderList();
        WaitScreen();
        SendRenderList();
        g_inputMgr.Update(1.0f);
        if (g_inputMgr.GetValueAnyPlayer(0x80)) {
            done = true;
        }
        if (mode == BOOT_TO_SYSTEM_MENU) {
            if (g_inputMgr.GetValueAnyPlayer(0x7F)) {
                GXDrawDone();
                VISetBlack(1);
                VIFlush();
                VIWaitForRetrace();
                OSResetSystem(1, 1, 1);
            }
        } else if (mode == BOOT_FORMAT && g_inputMgr.GetValueAnyPlayer(0x7F)) {
            float formatTop = 50.0f * g_screen.m_heightScale;
            char slotLetter = port == 0 ? 'A' : 'B';
            char formatting[0x50];
            int frames = 0;
            do {
                frames++;
                sprintf(formatting, Message(0x34A), slotLetter);
                font.DrawTextCentered(formatTop, formatting);
                float line = formatTop + 22.0f;
                font.DrawTextCentered(line, Message(0x34B));
                font.DrawTextCentered(line + 22.0f, Message(0x34C));
                MohDVDErrorTask(0, 0);
                WaitRenderList();
                WaitScreen();
                SendRenderList();
                g_inputMgr.Update(1.0f);
            } while (frames != 60);
            bool formatted = UsePort(m_pMemoryCard, port) ? m_pMemoryCard->Format() : false;
            if (!formatted) {
                sprintf(buffer, Message(0x344), letter);
                float errorTop = 50.0f * g_screen.m_heightScale;
                char failed[0x50];
                do {
                    font.DrawTextCentered(errorTop, Message(0x343));
                    float line = errorTop + 22.0f;
                    sprintf(failed, Message(0x344), letter);
                    font.DrawTextCentered(line, failed);
                    line += 44.0f;
                    font.DrawTextCentered(line, Message(0x340));
                    font.DrawTextCentered(line + 22.0f, Message(0x341));
                    WaitRenderList();
                    WaitScreen();
                    SendRenderList();
                    g_inputMgr.Update(1.0f);
                } while (!g_inputMgr.GetValueAnyPlayer(0x80));
                done = true;
            }
        }
    } while (!done);
    WaitRenderList();
    WaitScreen();
    SendRenderList();
    TLT_CloseFile(fontData);
    return result;
}

// Keeps the shell's save data in the loader block across the level ELF.
void CShellMenu::SaveData() {
    m_crc = ShellDataCRC((const u8*)this, SHELL_CRC_SIZE);
    memcpy(LoadElf::Get(), this, SHELL_SAVE_SIZE);
    LoadElf::Get()->m_shellDataValid = 1;
    LoadElf::Get()->m_shellDataMagic = 0xDEADBEEF;
}

// Back from a level: takes the save data from the loader block (cleared if
// its CRC is bad).
void CShellMenu::LoadData() {
    LoadElf* loader = LoadElf::Get();
    bool valid = false;
    if (loader->m_shellDataValid && loader->m_shellDataMagic == 0xDEADBEEF) {
        valid = true;
    }
    field_0x18EC = valid;
    if (field_0x18EC) {
        memcpy(this, LoadElf::Get(), SHELL_SAVE_SIZE);
        if (!CheckDataCRC(NULL, 0, 0)) {
            memset(this, 0, SHELL_SAVE_SIZE);
        }
    }
}

// NULL data: the shell's own save data against its stored CRC.
int CShellMenu::CheckDataCRC(unsigned char* data, int size, unsigned long expected) const {
    if (data == NULL) {
        data = (unsigned char*)this;
        expected = m_crc;
        size = SHELL_CRC_SIZE;
    }
    return ShellDataCRC(data, size) == expected;
}

// Fades the shell music to `volume` (waiting for it); returns the old volume.
int CShellMenu::FadeTheVolume(int volume) {
    int old = 0;
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        old = pSoundStream->GetVolume();
        pSoundStream->FadeVolume(0x5DC, volume);
    }
    if (pSoundStream != NULL) {
        int current;
        do {
            UpdateREAL();
            CSoundSysLock lock;
            current = pSoundStream->GetVolume();
        } while (current != volume);
    }
    return old;
}

// Gallery movies: the credits play as a slide show; the movie files are
// named but never played (retail stub).
void CShellMenu::PlayGalleryMovie(int movie, int type, int credits, int extra) {
    char name[0x40];
    if (credits) {
        PlayCreditsSlideShow();
        return;
    }
    if (extra) {
        sprintf(name, "data\\Movies\\extras%d.mpc", extra);
        return;
    }
    switch (type) {
    case 0:
        sprintf(name, "data\\Movies\\history%d.mpc", movie);
        break;
    case 1:
        sprintf(name, "data\\Movies\\making%d.mpc", movie);
        break;
    }
}

void CShellMenu::PlayRewardMovie(int) {}

// Stages per mission (index 1..6).
static inline u8 LastStage(u32 mission) {
    u8 lastStage[7] = {0, 4, 3, 3, 3, 4, 2};
    return lastStage[mission];
}

bool CShellMenu::CheckIfThisIsTheLastLevelForTheMission() {
    return m_stage == LastStage(m_mission);
}

// After mission 5: the special medals for the medals won so far (all six
// mission medals: medals 8 and 9 and the 7th at level 2; four or more: the
// 7th at level 2; one or more: the 7th at level 1). True if one was new.
bool CShellMenu::CheckIfSpecialMedalsNeedToBeUnlocked() {
    if (!(field_0x218 & SHELL_FLAG_04)) {
        if (!(m_stage == LastStage(m_mission))) {
            return false;
        }
        if (m_mission != 5) {
            return false;
        }
        field_0x218 |= SHELL_FLAG_04;
    }
    int medals[9];
    GetMedalStatus(medals);
    int count = 0;
    for (int i = 0; i < 6; i++) {
        if (medals[i] == 1) {
            count++;
        }
    }
    bool unlocked = false;
    if (count >= 7 || count < 1) {
        return false;
    }
    if (count >= 4) {
        if (count == 6) {
            if (medals[7] != 1) {
                m_medals[1] |= 0x80;
                unlocked = true;
            }
        }
        if (count >= 5) {
            if (medals[8] != 1) {
                m_medals[1] |= 0x40;
                unlocked = true;
            }
        }
        if (medals[6] != 2) {
            m_medals[0] = (m_medals[0] & ~3) | 2;
            unlocked = true;
        }
    } else if (medals[6] != 1) {
        m_medals[0] = (m_medals[0] & ~3) | 1;
        unlocked = true;
    }
    return unlocked;
}

// All of the mission's levels at 3 stars: its medal and its gallery movies.
// True if the medal is new.
bool CShellMenu::CheckIfMovieAndMedalNeedsToBeUnlocked() {
    int stars[6 * 4];
    memset(stars, 0, sizeof(stars));
    GetAllLevelsStarStatus(stars);
    u8 stages = LastStage((u8)m_mission);
    int levelStars[4];
    levelStars[0] = stars[(m_mission - 1) * 4];
    levelStars[1] = stars[(m_mission - 1) * 4 + 1];
    levelStars[2] = stars[(m_mission - 1) * 4 + 2];
    levelStars[3] = stars[(m_mission - 1) * 4 + 3];
    u8 gold[4] = {0, 0, 0, 0};
    for (int i = 0; i < stages; i++) {
        if (levelStars[i] == 3) {
            gold[i] = 1;
        }
    }
    bool unlocked = false;
    switch (stages) {
    case 2:
        if (gold[0] && gold[1]) {
            unlocked = true;
        }
        break;
    case 3:
        if (gold[0] && gold[1] && gold[2]) {
            unlocked = true;
        }
        break;
    case 4:
        if (gold[0] && gold[1] && gold[2] && gold[3]) {
            unlocked = true;
        }
        break;
    default:
        unlocked = false;
        break;
    }
    int medals[9];
    GetMedalStatus(medals);
    if (unlocked) {
        if (medals[m_mission - 1] != 1) {
            switch (m_mission) {
            case 1:
                m_medals[0] |= 0x80;
                break;
            case 2:
                m_medals[0] |= 0x40;
                break;
            case 3:
                m_medals[0] |= 0x20;
                break;
            case 4:
                m_medals[0] |= 0x10;
                break;
            case 5:
                m_medals[0] |= 0x08;
                break;
            case 6:
                m_medals[0] |= 0x04;
                break;
            case 7:
                m_medals[0] = (m_medals[0] & ~3) | 1;
                break;
            case 8:
                m_medals[1] |= 0x80;
                break;
            case 9:
                m_medals[1] |= 0x40;
                break;
            case 10:
                m_medals[1] |= 0x20;
                break;
            }
        } else {
            unlocked = false;
        }
        m_moviesUnlocked[m_mission - 1][0] = 1;
        m_moviesUnlocked[m_mission - 1][1] = 1;
    }
    return unlocked;
}

// The level after the current one (mission 6 wraps to 1) into
// field_0xC/field_0xD.
static inline void NextLevel(CShellMenu* shell) {
    shell->field_0xC = shell->m_mission;
    shell->field_0xD = shell->m_stage;
    shell->field_0xD = shell->field_0xD + 1;
    switch (shell->field_0xC) {
    case 1:
        if (shell->field_0xD > 4) {
            shell->field_0xC++;
            shell->field_0xD = 1;
        }
        break;
    case 2:
    case 3:
    case 4:
        if (shell->field_0xD > 3) {
            shell->field_0xC++;
            shell->field_0xD = 1;
        }
        break;
    case 5:
        if (shell->field_0xD > 4) {
            shell->field_0xC++;
            shell->field_0xD = 1;
        }
        break;
    case 6:
        if (shell->field_0xD > 2) {
            shell->field_0xC = 1;
            shell->field_0xD = 1;
        }
        break;
    }
}

void CShellMenu::GetNextLevel() {
    NextLevel(this);
    SetUnlockedLevelStatus(&m_unlocked, field_0xD + field_0xC * 10, 1);
}

// The level-skip cheat: 3 stars on this level, then on to the next.
void CShellMenu::GetNextLevelCheat() {
    NextLevel(this);
    SetStarRatingStatus(&m_stars, m_stage + m_mission * 10, 3);
    CheckIfMovieAndMedalNeedsToBeUnlocked();
    CheckIfSpecialMedalsNeedToBeUnlocked();
    m_mission = field_0xC;
    m_stage = field_0xD;
    SetUnlockedLevelStatus(&m_unlocked, field_0xD + field_0xC * 10, 1);
}

void CShellMenu::CopyControllerButtons(char* buttons, int player) {
    for (int i = 0; i < 0xD; i++) {
        m_customButtons[player][i] = buttons[i];
    }
}

void CShellMenu::RetrieveShellControllerButtons(char* buttons, int player) {
    for (int i = 0; i < 0xD; i++) {
        buttons[i] = m_customButtons[player][i];
    }
}

const char* CShellMenu::Get_LanguageSuffix() {
    return m_languageSuffix;
}

int CShellMenu::Get_LanguageSubtitleIndex() {
    return m_languageSubtitleIndex;
}

// Fades the shell music to `volume` and waits for it.
static inline void FadeShellStream(int volume) {
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        pSoundStream->GetVolume();
        pSoundStream->FadeVolume(0x5DC, volume);
    }
    if (pSoundStream != NULL) {
        int current;
        do {
            UpdateREAL();
            CSoundSysLock lock;
            current = pSoundStream->GetVolume();
        } while (current != volume);
    }
}

// Silences and pauses the shell music; returns its volume.
static inline int StopShellStream() {
    int volume = 0;
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        volume = pSoundStream->GetVolume();
        pSoundStream->FadeVolume(0x5DC, 0);
    }
    if (pSoundStream != NULL) {
        int current;
        do {
            UpdateREAL();
            CSoundSysLock lock;
            current = pSoundStream->GetVolume();
        } while (current != 0);
    }
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        pSoundStream->Pause();
    }
    UpdateREAL();
    return volume;
}

// Restores the shell music after a slide show.
static inline void RestartShellStream(CShellMenu* shell, int volume) {
    SoundSetVolumes((float)((s8)shell->field_0x28 - 1) / 6.0f, (float)(shell->m_musicVolume - 1) / 6.0f);
    if (pSoundStream != NULL) {
        CSoundSysLock lock;
        pSoundStream->Unpause();
    }
    UpdateREAL();
    FadeShellStream(volume);
}

static inline void SetStreamVolume(CShellMenu* shell, CSoundStream* stream) {
    CSoundSysLock lock;
    stream->SetVolume((int)((127.0f * (float)(shell->m_musicVolume - 1)) / 6.0f));
}

// Slide fade: in over 75 frames, out over the last 55.
static inline u8 SlideAlpha(int frame, int duration, u8 alpha) {
    if (frame < 75) {
        return (frame << 7) / 75;
    }
    int left = duration - frame;
    if (left < 55) {
        return (left << 7) / 55;
    }
    return alpha;
}

static inline void DrawSlide(CSprite* slide, _spritepolyvert* verts) {
    slide->DrawPoly(verts, 4, CSprite::POLY_TRIANGLE_FAN);
    UpdateREAL();
    WaitRenderList();
    WaitScreen();
    SendRenderList();
    slide->DrawPoly(verts, 4, CSprite::POLY_TRIANGLE_FAN);
    WaitRenderList();
    WaitScreen();
    SendRenderList();
}

static const _spritepolyvert s_slideVerts[4] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 0.0f, 640.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {1.0f, 1.0f, 640.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    {0.0f, 1.0f, 0.0f, 448.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
};

enum {
    NUM_CREDIT_SLIDES = 20,
    MAX_BRIEFING_SLIDES = 11,
};

// The credits: 20 slides over looping music; A skips.
void CShellMenu::PlayCreditsSlideShow() {
    int volume = StopShellStream();
    int frame = 0;
    int slide = 0;
    u8 alpha = 0x80;
    CSoundStream* stream = new CSoundStream(1, 0x32);
    char musicName[0x50];
    sprintf(musicName, "data\\shell\\credits.asf");
    int durations[NUM_CREDIT_SLIDES];
    for (int i = 0; i < NUM_CREDIT_SLIDES; i++) {
        durations[i] = 250;
    }
    CSprite* slides[NUM_CREDIT_SLIDES];
    char name[0x80];
    for (int i = 0; i < NUM_CREDIT_SLIDES; i++) {
        sprintf(name, "credits%d.gsh", i + 1);
        slides[i] = new CSprite(name, (CSprite::ESpriteLoadType)4, (ERenderPriority)0x34);
    }
    g_soundStreamRequest = stream->QueueFile(musicName, 0x32, 0);
    SetStreamVolume(this, stream);
    while (true) {
        _spritepolyvert verts[4];
        memcpy(verts, s_slideVerts, sizeof(verts));
        alpha = SlideAlpha(frame, durations[slide], alpha);
        verts[0].color[3] = alpha;
        verts[1].color[3] = alpha;
        verts[2].color[3] = alpha;
        verts[3].color[3] = alpha;
        DrawSlide(slides[slide], verts);
        g_inputMgr.Update(1.0f);
        if (g_inputMgr.GetValueAnyPlayer(0x80)) {
            FadeShellStream(0);
            stream->PurgeQueue();
            delete stream;
            break;
        }
        SSndStreamStatus status;
        SSndStrmRequestStatus request;
        {
            CSoundSysLock lock;
            stream->GetStatus(status);
            stream->GetRequestStatus(g_soundStreamRequest, request);
        }
        if (request.m_state == 3) {
            // The music loops.
            stream->PurgeQueue();
            g_soundStreamRequest = stream->QueueFile(musicName, 0x32, 0);
            SetStreamVolume(this, stream);
        }
        frame++;
        if (frame % durations[slide] == 0) {
            slide++;
            frame = 0;
            if (slide >= NUM_CREDIT_SLIDES) {
                FadeShellStream(0);
                stream->PurgeQueue();
                delete stream;
                break;
            }
        }
    }
    for (int i = 0; i < NUM_CREDIT_SLIDES; i++) {
        if (slides[i] != NULL) {
            delete slides[i];
        }
    }
    RestartShellStream(this, volume);
}

// A mission briefing: its slides cycle until the narration ends; A skips.
void CShellMenu::PlayBriefingMovie(int mission) {
    static const int slideCounts[6] = {11, 9, 10, 8, 6, 11};
    static const int slideFrames[6][MAX_BRIEFING_SLIDES] = {
        {0xFA, 0x19A, 0x258, 0x208, 0x370, 0x334, 0x2D0, 0x17C, 0x1A4, 0x21C, 0x1388},
        {0x190, 0x258, 0x5DC, 0x348, 0x4EC, 0x35C, 0x154, 0x1E0, 0x1388, 1, 1},
        {0x168, 0x348, 0x370, 0x398, 0x12C, 0x320, 0x258, 0x384, 0x320, 0x1388, 1},
        {0x190, 0x1F4, 0x1F4, 0x12C, 0x258, 0x2BC, 0x258, 0x1388, 1, 1, 1},
        {0x140, 0x320, 0x2E4, 0x3AC, 0x280, 0x1388, 1, 1, 1, 1, 1},
        {0x352, 0x352, 0x352, 0x320, 0x320, 0x320, 0x320, 0x320, 0x1388, 1, 1},
    };
    int volume = StopShellStream();
    int frame = 0;
    u8 alpha = 0x80;
    CSoundStream* stream = new CSoundStream(1, 0x32);
    char musicName[0x80];
    sprintf(musicName, "data\\shell\\brief%d1.asf", mission);
    int slide = 0;
    int counts[6];
    int frames[6][MAX_BRIEFING_SLIDES];
    for (int i = 0; i < 6; i++) {
        counts[i] = slideCounts[i];
        for (int j = 0; j < MAX_BRIEFING_SLIDES; j++) {
            frames[i][j] = slideFrames[i][j] / 2;
        }
    }
    CSprite* slides[MAX_BRIEFING_SLIDES];
    char name[0x80];
    for (int i = 0; i < counts[mission - 1]; i++) {
        sprintf(name, "slide%d%d.gsh", mission, i + 1);
        slides[i] = new CSprite(name, (CSprite::ESpriteLoadType)4, (ERenderPriority)0x34);
    }
    g_soundStreamRequest = stream->QueueFile(musicName, 0x32, 0);
    SetStreamVolume(this, stream);
    int* durations = frames[mission - 1];
    while (true) {
        _spritepolyvert verts[4];
        memcpy(verts, s_slideVerts, sizeof(verts));
        alpha = SlideAlpha(frame, durations[slide], alpha);
        verts[0].color[3] = alpha;
        verts[1].color[3] = alpha;
        verts[2].color[3] = alpha;
        verts[3].color[3] = alpha;
        DrawSlide(slides[slide], verts);
        g_inputMgr.Update(1.0f);
        if (g_inputMgr.GetValueAnyPlayer(0x80)) {
            FadeShellStream(0);
            stream->PurgeQueue();
            delete stream;
            break;
        }
        SSndStreamStatus status;
        SSndStrmRequestStatus request;
        {
            CSoundSysLock lock;
            stream->GetStatus(status);
            stream->GetRequestStatus(g_soundStreamRequest, request);
        }
        if (request.m_state == 3) {
            stream->PurgeQueue();
            delete stream;
            break;
        }
        frame++;
        if (frame % durations[slide] == 0) {
            frame = 0;
            slide = (slide + 1) % counts[mission - 1];
        }
    }
    for (int i = 0; i < counts[mission - 1]; i++) {
        if (slides[i] != NULL) {
            delete slides[i];
        }
    }
    RestartShellStream(this, volume);
}

// Level `id` (mission * 10 + stage) unlocked or not.
void SetUnlockedLevelStatus(UnlockedLevelsInBits* bits, unsigned int id, unsigned int value) {
    u8* b = bits->m_bits;
    switch (id) {
    case 0xB:
        b[0] = (b[0] & ~0x80) | ((value << 7) & 0x80);
        break;
    case 0xC:
        b[0] = (b[0] & ~0x40) | ((value << 6) & 0x40);
        break;
    case 0xD:
        b[0] = (b[0] & ~0x20) | ((value << 5) & 0x20);
        break;
    case 0xE:
        b[0] = (b[0] & ~0x10) | ((value << 4) & 0x10);
        break;
    case 0x15:
        b[0] = (b[0] & ~0x08) | ((value << 3) & 0x08);
        break;
    case 0x16:
        b[0] = (b[0] & ~0x04) | ((value << 2) & 0x04);
        break;
    case 0x17:
        b[0] = (b[0] & ~0x02) | ((value << 1) & 0x02);
        break;
    case 0x18:
        b[0] &= ~0x01;
        break;
    case 0x1F:
        b[1] = (b[1] & ~0x80) | ((value << 7) & 0x80);
        break;
    case 0x20:
        b[1] = (b[1] & ~0x40) | ((value << 6) & 0x40);
        break;
    case 0x21:
        b[1] = (b[1] & ~0x20) | ((value << 5) & 0x20);
        break;
    case 0x22:
        b[1] &= ~0x10;
        break;
    case 0x29:
        b[1] = (b[1] & ~0x08) | ((value << 3) & 0x08);
        break;
    case 0x2A:
        b[1] = (b[1] & ~0x04) | ((value << 2) & 0x04);
        break;
    case 0x2B:
        b[1] = (b[1] & ~0x02) | ((value << 1) & 0x02);
        break;
    case 0x2C:
        b[1] &= ~0x01;
        break;
    case 0x33:
        b[2] = (b[2] & ~0x80) | ((value << 7) & 0x80);
        break;
    case 0x34:
        b[2] = (b[2] & ~0x40) | ((value << 6) & 0x40);
        break;
    case 0x35:
        b[2] = (b[2] & ~0x20) | ((value << 5) & 0x20);
        break;
    case 0x36:
        b[2] = (b[2] & ~0x10) | ((value << 4) & 0x10);
        break;
    case 0x3D:
        b[2] = (b[2] & ~0x08) | ((value << 3) & 0x08);
        break;
    case 0x3E:
        b[2] = (b[2] & ~0x04) | ((value << 2) & 0x04);
        break;
    case 0x3F:
        b[2] &= ~0x02;
        break;
    case 0x40:
        b[2] &= ~0x01;
        break;
    default:
        DebugMsg("Level ID: %d\n", id);
        break;
    }
}

// Unlocks a level directly (password).
static inline void UnlockMission(CShellMenu* shell, int mission) {
    shell->m_mission = mission;
    shell->m_stage = 1;
    shell->field_0xC = mission;
    shell->field_0xD = 1;
    SetUnlockedLevelStatus(&shell->m_unlocked, shell->m_stage + shell->m_mission * 10, 1);
}

// Sets a secret's 2-bit found state to 1.
static inline void FindSecret(u8& byte, int shift) {
    byte = (byte & ~(3 << shift)) | (1 << shift);
}

// Passwords, matched by their CRC-32.
bool CShellMenu::CheckIfValidPassword(char* password) {
    bool valid = false;
    u32 crc = ShellDataCRC((const u8*)password, strlen(password));
    if (crc == 0x2F29CCB7) {
        ActivateCheatNamed_All();
        return true;
    }
    if (crc == 0x0412FE17) {
        valid = true;
        FindSecret(m_secrets[0], 2);
    }
    if (crc == 0x72C433DF) {
        valid = true;
        FindSecret(m_secrets[1], 2);
    }
    if (crc == 0x3D8E0F1E) {
        valid = true;
        FindSecret(m_secrets[1], 0);
    }
    if (crc == 0x8A7E1481) {
        valid = true;
        FindSecret(m_secrets[0], 6);
    }
    if (crc == 0x5C3F5629) {
        valid = true;
        FindSecret(m_secrets[0], 4);
    }
    if (crc == 0x832B676B) {
        valid = true;
        FindSecret(m_secrets[0], 0);
    }
    if (crc == 0x06A5A920) {
        valid = true;
        FindSecret(m_secrets[1], 4);
    }
    if (crc == 0x7E6A8CD5) {
        valid = true;
        FindSecret(m_secrets[1], 6);
    }
    if (crc == 0x3255C06F) {
        valid = true;
        FindSecret(m_secrets[2], 4);
    }
    if (crc == 0x5A36312F) {
        UnlockMission(this, 1);
        return true;
    }
    if (crc == 0x1A481BB2) {
        UnlockMission(this, 2);
        return true;
    }
    if (crc == 0xCD04237D) {
        UnlockMission(this, 3);
        return true;
    }
    if (crc == 0xA4A39546) {
        UnlockMission(this, 4);
        return true;
    }
    if (crc == 0x0A77FA00) {
        UnlockMission(this, 5);
        return true;
    }
    if (crc == 0xFA45471F) {
        GetNextLevelCheat();
        return true;
    }
    return valid;
}

// Everything: all levels with 3 stars, medals, secrets, movies and extras.
void CShellMenu::ActivateCheatNamed_All() {
    for (int mission = 0; mission < 6; mission++) {
        for (int stage = 0; stage < 4; stage++) {
            u32 id = stage + (mission + 1) * 10 + 1;
            SetStarRatingStatus(&m_stars, id, 3);
            SetUnlockedLevelStatus(&m_unlocked, id, 1);
        }
    }
    m_medals[0] |= 0x80 | 0x40 | 0x20 | 0x10 | 0x08 | 0x04;
    m_medals[0] = (m_medals[0] & ~3) | 1;
    m_medals[1] |= 0x80 | 0x40 | 0x20;
    for (int i = 0; i < 10; i++) {
        FindSecret(m_secrets[i / 4], 6 - (i % 4) * 2);
    }
    for (int i = 0; i < 6; i++) {
        m_moviesUnlocked[i][0] = 1;
        m_moviesUnlocked[i][1] = 1;
    }
    for (int i = 0; i < 5; i++) {
        m_extrasUnlocked[i] = 1;
    }
    // Levels that don't exist.
    m_stars.m_bits[1] &= ~3;
    m_stars.m_bits[2] &= ~3;
    m_stars.m_bits[3] &= ~3;
    m_stars.m_bits[5] &= ~0xC;
    m_stars.m_bits[5] &= ~3;
    m_unlocked.m_bits[0] &= ~1;
    m_unlocked.m_bits[1] &= ~0x10;
    m_unlocked.m_bits[1] &= ~1;
    m_unlocked.m_bits[2] &= ~2;
    m_unlocked.m_bits[2] &= ~1;
    field_0x19A5 = 1;
}

int CShellMenu::PassLevelsCompletedToIStudio(int* out) {
    int levels[6 * 4];
    GetLevelUnlockedStatus(levels);
    for (int i = 0; i < 6 * 4; i++) {
        if (levels[i] > 0) {
            levels[i] = 1;
        }
    }
    memcpy(out, levels, sizeof(levels));
    return 6 * 4;
}

int CShellMenu::PassLevelsRatingsToIStudio(int* out) {
    int stars[6 * 4];
    GetAllLevelsStarStatus(stars);
    memcpy(out, stars, sizeof(stars));
    return 6 * 4;
}

int CShellMenu::PassMedalStatusToIStudio(int* out) {
    int medals[9];
    GetMedalStatus(medals);
    memcpy(out, medals, sizeof(medals));
    return 9;
}

// Pairs per secret: found, then its state - 1.
int CShellMenu::PassSecretsToIStudio(int* out) {
    int secrets[10];
    int pairs[10][2];
    GetSecretsStatus(secrets);
    for (int i = 0; i < 10; i++) {
        if (secrets[i] > 0) {
            pairs[i][0] = 1;
            pairs[i][1] = secrets[i] - 1;
        } else {
            pairs[i][0] = 0;
            pairs[i][1] = 0;
        }
    }
    memcpy(out, pairs, sizeof(pairs));
    return 10;
}

void CShellMenu::SetupAudioSettings(int* settings) {
    field_0x28 = settings[0];
    m_musicVolume = settings[1];
    m_soundMode = settings[2];
    ApplyAudio(this);
}

void CShellMenu::SetSoundFXValue(int volume) {
    field_0x28 = volume;
}

void CShellMenu::SetMusicValue(int volume) {
    m_musicVolume = volume;
}

void CShellMenu::ResetAudio() {
    ApplyAudio(this);
}

int CShellMenu::PassAudioSettingToIStudio(int* out) {
    int settings[3];
    settings[0] = (s8)field_0x28;
    settings[1] = m_musicVolume;
    settings[2] = m_soundMode;
    memcpy(out, settings, sizeof(settings));
    return 3;
}

// Frees the journal pictures of the level last shown (library 11).
void CShellMenu::NullOutJournalSpritesFromLibrary() {
    if (field_0x19AA == 6) {
        return;
    }
    int level = field_0x19AB + (field_0x19AA - 1) * 4 - 1;
    s8 first = 0;
    for (int i = 0; i < level; i++) {
        first += ((s8*)m_journalPictures)[i];
    }
    int count = ((s8*)m_journalPictures)[level];
    for (int i = 0; i < count; i++) {
        CSprite*& sprite = LibraryInfo[11].images[first + i];
        if (sprite != NULL) {
            delete sprite;
            LibraryInfo[11].images[first + i] = NULL;
        }
    }
    field_0x19AA = -1;
    field_0x19AB = -1;
}

void CShellMenu::SetupControllerOptionsSettings(int* settings) {
    m_invertLook = settings[0];
    m_crosshair = settings[1];
    m_rumble = settings[2];
    m_cookGrenades = settings[3];
    field_0x218 |= SHELL_NEWGAME;
}

int CShellMenu::PassControllerOptionsSettingToIStudio(int* out) {
    int settings[4];
    settings[0] = (s8)m_invertLook;
    settings[1] = (s8)m_crosshair;
    settings[2] = (s8)m_rumble;
    settings[3] = (s8)m_cookGrenades;
    memcpy(out, settings, sizeof(settings));
    return 4;
}

void CShellMenu::InitControllerFromOptions() {
    g_inputMgr.field_0x244A = (s8)m_invertLook == 1;
    g_inputMgr.m_feedbackEnabled = (s8)m_rumble == 1;
}

int CShellMenu::GetCrosshairStatus() {
    return (u8)m_crosshair;
}

void CShellMenu::ChangeNewGameSignStatus() {
    if (!field_0x254) {
        field_0x254 = 1;
    }
}

void CShellMenu::CompletedAnotherObjective() {
    m_objectivesDone++;
}

void CShellMenu::PlayShellSound(int sound) {
    if (m_shellOn) {
        int event[2] = {0xB, 0};
        event[1] = sound;
        AEMS_SendEvent(event);
    }
}

int CShellMenu::GetGalleryMoviesUnlocked(int* movies, int* extras) {
    int unlocked[12];
    for (int i = 0; i < 12; i++) {
        unlocked[i] = ((u8*)m_moviesUnlocked)[i];
    }
    memcpy(movies, unlocked, sizeof(unlocked));
    int extra[5];
    for (int i = 0; i < 5; i++) {
        extra[i] = m_extrasUnlocked[i];
    }
    memcpy(extras, extra, sizeof(extra));
    return 12;
}

// Secrets found twice (state 2) turn their cheats on.
void CShellMenu::SetGlobalCheatStructure() {
    int secrets[10];
    GetSecretsStatus(secrets);
    g_cheats.m_invincible = 0;
    g_cheats.field_0x1 = 0;
    for (u32 i = 1; i <= 10; i++) {
        u8 on = secrets[i - 1] == 2;
        switch (i) {
        case 1:
            g_cheats.field_0xB = on;
            break;
        case 2:
            g_cheats.field_0xC = on;
            break;
        case 3:
            g_cheats.field_0x8 = on;
            break;
        case 4:
            g_cheats.field_0xE = on;
            break;
        case 5:
            g_cheats.field_0xF = on;
            break;
        case 6:
            g_cheats.field_0xD = on;
            break;
        case 7:
            g_cheats.field_0xA = on;
            break;
        case 8:
            g_cheats.field_0x9 = on;
            break;
        case 9:
            g_cheats.field_0x3 = on;
            g_cheats.field_0x4 = 0.0f;
            break;
        case 10:
            g_cheats.field_0x2 = on;
            break;
        }
    }
}

int CShellMenu::GetSoundModeValue() {
    switch (m_soundMode) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 0;
    default:
        return 0;
    }
}

void CShellMenu::SetupPlayersAdvancedMultiPlayerSettings(int player, int* settings) {
    CShellPlayerConfig& config = m_players[player];
    config.m_invertLook = settings[0];
    config.m_crosshair = settings[1];
    config.m_cookGrenades = settings[2];
    config.m_rumble = settings[3];
    config.m_radar = settings[4];
    field_0x143C[player] = settings[5];
}

int CShellMenu::PassPlayersAdvancedMultiPlayerSettingToIStudio(int player, int* out) {
    CShellPlayerConfig& config = m_players[player];
    int settings[6];
    settings[0] = config.m_invertLook;
    settings[1] = config.m_crosshair;
    settings[2] = config.m_cookGrenades;
    settings[3] = config.m_rumble;
    settings[4] = config.m_radar;
    settings[5] = field_0x143C[player];
    memcpy(out, settings, sizeof(settings));
    return 6;
}

void CShellMenu::SetupPlayersMultiPlayerName(int player, char* name) {
    name[10] = 0;
    strcpy(m_players[player].m_name, name);
}

void CShellMenu::PassPlayersMultiPlayerName(int player, char* out) {
    strcpy(out, m_players[player].m_name);
}

void CShellMenu::SetupPlayersModelAndTeamColorSettingForMultiplayer(int player, int* settings) {
    m_players[player].m_character = settings[0];
    m_players[player].m_team = settings[1];
}

int CShellMenu::PassPlayersModelAndTeamColorSettingForMultiplayer(int player, int* out) {
    int settings[2];
    settings[0] = m_players[player].m_character;
    settings[1] = m_players[player].m_team;
    memcpy(out, settings, sizeof(settings));
    return 2;
}

void CShellMenu::SetupPlayersMultiPlayerControllerValue(int player, int config) {
    m_players[player].m_controllerConfig = config;
}

int CShellMenu::PassPlayersMultiPlayerControllerValue(int player) {
    return m_players[player].m_controllerConfig;
}

void CShellMenu::SetupMultiPlayerLevelSettings(int* settings) {
    m_mpWeaponSet = settings[0];
    m_mpStage = settings[1];
    m_mpTimeLimit = settings[2];
    field_0x1428 = settings[3];
    field_0x1434 = settings[4];
    m_mpMusicTrack = settings[5];
    m_mpTimeLimitOn = m_mpTimeLimit > 0 ? 1 : 0;
    field_0x1430 = field_0x1428 > 0 ? 1 : 0;
}

void CShellMenu::PassMultiPlayerLevelSettingsToIStudio(int* out) {
    int settings[6];
    settings[0] = m_mpWeaponSet;
    settings[1] = m_mpStage;
    settings[2] = m_mpTimeLimit;
    settings[3] = field_0x1428;
    settings[4] = field_0x1434;
    settings[5] = m_mpMusicTrack;
    memcpy(out, settings, sizeof(settings));
}

// Each player whose controller is ready and window is active plays on its
// own port.
void CShellMenu::SetActivePlayers(int* active) {
    for (int i = 0; i < 4; i++) {
        if (g_inputMgr.CDeviceManager::IsReady(i) && active[i] != 0) {
            m_mpControllerPort[i] = i;
        } else {
            m_mpControllerPort[i] = -1;
        }
    }
}

static inline int CountActiveWindows(CShellMenu* shell) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (shell->m_activeWindows[i] != 0) {
            count++;
        }
    }
    return count;
}

void CShellMenu::Save_ActiveWindowsFromIStudio(int* windows) {
    for (int i = 0; i < 4; i++) {
        m_activeWindows[i] = windows[i];
    }
    m_mpNumPlayers = CountActiveWindows(this);
}

void CShellMenu::Pass_ActiveWindowsToIStudio(int* out) {
    int windows[4];
    memset(windows, 0, sizeof(windows));
    for (int i = 0; i < 4; i++) {
        if (m_activeWindows[i] > 0) {
            windows[i] = m_activeWindows[i];
        }
    }
    memcpy(out, windows, sizeof(windows));
}

int CShellMenu::Get_NumPlayers() {
    int count = CountActiveWindows(this);
    m_mpNumPlayers = count;
    return count;
}

// Kills, team kills, score, team score, deaths and accuracy (percent).
void CShellMenu::PassMP_StatsToIStudio(int player, int* out) {
    MPPlayerStats& stats = m_mpStats[player];
    float accuracy = 0.0f;
    int values[6];
    values[0] = stats.m_kills;
    values[1] = stats.m_teamKills;
    values[2] = stats.m_score;
    values[3] = stats.m_goalScore;
    values[4] = stats.m_deaths;
    if (stats.m_shotsFired > 0) {
        float hits = 0.0f;
        for (int i = 0; i < NUM_SHOT_LOCATIONS; i++) {
            hits += (float)stats.m_shotLocations[i];
        }
        accuracy = hits / (float)stats.m_shotsFired;
        if (accuracy > 1.0f) {
            accuracy = 1.0f;
        }
    }
    values[5] = (int)(100.0f * accuracy);
    memcpy(out, values, sizeof(values));
}

// The player's favourite weapon, abbreviated to fit.
void CShellMenu::PassMP_PrefWeaponNameToIStudio(int player, char* out) {
    MPPlayerStats& stats = m_mpStats[player];
    int best = 0;
    int weapon = -1;
    for (int i = 0; i < NUM_STAT_WEAPONS; i++) {
        if (stats.m_weaponKills[i] > best) {
            best = stats.m_weaponKills[i];
            weapon = i;
        }
    }
    if (weapon < 0) {
        strcpy(out, "None");
        return;
    }
    strcpy(out, Message(weapon + MSG_BEST_WEAPON));
    if (strstr(out, "Walther") != NULL) {
        strcpy(out, "P38");
    } else if (strstr(out, "Stiel Gran") != NULL) {
        strcpy(out, "S. Granate");
    } else if (strstr(out, "Silenced") != NULL) {
        strcpy(out, "S. Pistol");
    } else if (strstr(out, "Panzerschr") != NULL) {
        strcpy(out, "Panzer");
    } else if (strstr(out, "Gewehr") != NULL) {
        strcpy(out, "Gewehr 43");
    } else if (strstr(out, "Springfiel") != NULL) {
        strcpy(out, "Spring.");
    }
}

// The player killed most; returns its index (-1 none).
int CShellMenu::PassMP_PrefTargetToIStudio(int player, char* out) {
    MPPlayerStats& stats = m_mpStats[player];
    int best = 0;
    int target = -1;
    for (int i = 0; i < 4; i++) {
        if (stats.m_killedPlayer[i] > best) {
            best = stats.m_killedPlayer[i];
            target = i;
        }
    }
    if (target >= 0) {
        strcpy(out, m_players[target].m_name);
    } else {
        strcpy(out, "None");
    }
    return target;
}

// Where the player hits most.
void CShellMenu::PassMP_PrefHitLocationToIStudio(int player, char* out) {
    MPPlayerStats& stats = m_mpStats[player];
    int best = 0;
    u32 location = (u32)-1;
    for (int i = 0; i < NUM_SHOT_LOCATIONS; i++) {
        if (stats.m_shotLocations[i] > best) {
            best = stats.m_shotLocations[i];
            location = i;
        }
    }
    switch (location) {
    case 0:
        strcpy(out, "Head");
        break;
    case 1:
    case 2:
        strcpy(out, "Arm");
        break;
    case 3:
        strcpy(out, "Torso");
        break;
    case 4:
    case 5:
        strcpy(out, "Leg");
        break;
    case 6:
        strcpy(out, "Groin");
        break;
    default:
        strcpy(out, "None");
        break;
    }
}

void CShellMenu::SetupAllPlayersButtonsForMultiplayerGame() {
    for (int i = 0; i < 4; i++) {
        if (m_activeWindows[i] != 0) {
            g_ControllerConfig.m_config = m_players[i].m_controllerConfig;
            g_ControllerConfig.m_player = i;
            g_ControllerConfig.SetupButtonsForMultiplayerGame();
        }
    }
}

u8 CShellMenu::GetSKUMode() {
    return 1;
}

// The match statistics are kept compact (by player); the results screens
// want them by window.
void CShellMenu::MoveStatsIntoProperAreaForIstudio() {
    int count = CountActiveWindows(this);
    m_mpNumPlayers = count;
    if (count < 2 || count > 3) {
        return;
    }
    MPPlayerStats byWindow[4];
    memset(byWindow, 0, sizeof(byWindow));
    int window = 0;
    MPPlayerStats* source = m_mpStats;
    for (int copied = 0; copied < count; window++) {
        if (m_activeWindows[window] != 0) {
            memcpy(&byWindow[window], source, sizeof(MPPlayerStats));
            source++;
            copied++;
        }
    }
    memcpy(m_mpStats, byWindow, sizeof(m_mpStats));
}
