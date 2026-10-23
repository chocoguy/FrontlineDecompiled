// CShellMenu: the front-end shell (iSiren menus) and the game progress it
// keeps: current mission/stage, unlocked levels, stars, medals, secrets,
// options, level and career statistics, the multiplayer setup and match
// statistics, and the memory card save data. Hand-maintained.
#ifndef _GAME_SHELLMENU_H_
#define _GAME_SHELLMENU_H_

#include "types.h"
#include "game_types.h"
#include "game/IStudio.h"
#include "game/pumhandler.h"

class CFont;
class CMemoryCard;
class CPlayerObject;
class CShellMenu;
class CSoundStream;
class CSprite;

// Stars per level, 2 bits each (MSB first), 4 levels per mission byte.
struct StarLevelsInBits {
    /* 0x0 */ u8 m_bits[8];
};

// Unlocked levels, 1 bit each (MSB first).
struct UnlockedLevelsInBits {
    /* 0x0 */ u8 m_bits[4];
};

enum {
    NUM_SHOT_LOCATIONS = 7,
    NUM_STAT_WEAPONS = 45,
    STAT_MAX = 9999999,  // counters stop here
};

// A level's (or the career's) statistics. size 0xF4
struct SLevelStats {
    /* 0x00 */ u32 m_shotsFired;
    /* 0x04 */ u32 m_hits;
    /* 0x08 */ u32 m_hitsTaken;
    /* 0x0C */ u32 m_unk0xC;
    /* 0x10 */ u32 m_enemiesKilled;
    /* 0x14 */ u32 m_shotLocations[NUM_SHOT_LOCATIONS];  // by EShotLocations
    /* 0x30 */ u32 m_time;  // frames
    /* 0x34 */ u32 m_minutes;
    /* 0x38 */ f32 m_accuracy;  // percent
    /* 0x3C */ u32 m_weaponKills[NUM_STAT_WEAPONS];  // by weapon type
    /* 0xF0 */ u8 m_bestWeapon;
    /* 0xF1 */ u8 m_evaluation;  // evaluation message index
    /* 0xF2 */ u8 m_rating;  // level: star rating (1..3)
    /* 0xF3 */ u8 unk_0xF3[0x1];
};

// What the player carried out of a level (the next stage starts with it).
// size 0xBC
struct SFinishedLevel {
    /* 0x00 */ f32 m_health;
    /* 0x04 */ u32 m_ammo[NUM_STAT_WEAPONS];  // by weapon type
    /* 0xB8 */ u8 m_finished;
    /* 0xB9 */ u8 unk_0xB9[0x3];
};

// Per-player multiplayer settings (MPPlayerData's layout). size 0x1C
struct CShellPlayerConfig {
    /* 0x00 */ s32 m_team;
    /* 0x04 */ s32 m_character;  // multiplayer character model
    /* 0x08 */ char m_name[0xB];
    /* 0x13 */ u8 m_invertLook;
    /* 0x14 */ u8 m_crosshair;  // crosshair on
    /* 0x15 */ u8 m_cookGrenades;
    /* 0x16 */ u8 m_rumble;
    /* 0x17 */ u8 m_radar;  // radar on
    /* 0x18 */ u8 m_controllerConfig;  // controller configuration
    /* 0x19 */ u8 unk_0x19[0x3];
};

// One multiplayer player's match statistics (size 0x100).
struct MPPlayerStats {
    /* 0x00 */ s32 m_shotsFired;
    /* 0x04 */ s32 m_shotLocations[NUM_SHOT_LOCATIONS];
    /* 0x20 */ s32 m_weaponKills[NUM_STAT_WEAPONS];
    /* 0xD4 */ s32 m_killedPlayer[4];  // by the victim's controller port
    /* 0xE4 */ s32 m_kills;
    /* 0xE8 */ s32 m_teamKills;
    /* 0xEC */ s32 m_score;  // kills - team kills
    /* 0xF0 */ s32 m_goalScore;  // the team's score, shown against the score limit
    /* 0xF4 */ s32 m_deaths;
    /* 0xF8 */ s32 m_lastHitBy;  // player index, -1 none
    /* 0xFC */ u8 m_star;  // drawn with a star (leading)
    /* 0xFD */ u8 unk_0xFD[0x3];
};

// The saved part of CShellMenu (memory card and the loader block).
#define SHELL_SAVE_SIZE 0x18E0
#define SHELL_CRC_SIZE 0x18DC

// The memory card preferences file. size 0xC
struct SShellPrefs {
    /* 0x0 */ u8 m_saveSlot;  // the save game to load at startup
    /* 0x1 */ char m_signature[9];  // "Frontline"
    /* 0xA */ u8 unk_0xA[0x2];
};

// field_0x218 flags
#define SHELL_SUBTITLES 0x80  // subtitles option (gates optional subtitles)
#define SHELL_CONTROLS_SET 0x40  // controller config chosen
#define SHELL_DIFFICULTY_SET 0x20  // difficulty chosen
#define SHELL_NEWGAME 0x10
#define SHELL_FLAG_08 0x08
#define SHELL_FLAG_04 0x04

// size 0x19DC
class CShellMenu {
public:
    /* 0x0 */ u8 m_controllerSetting;
    /* 0x1 */ u8 m_difficulty;
    /* 0x2 */ u8 field_0x2;
    /* 0x3 */ u8 field_0x3;
    /* 0x4 */ u32 m_mission;
    /* 0x8 */ u32 m_stage;
    /* 0xC */ u8 field_0xC;  // furthest mission
    /* 0xD */ u8 field_0xD;  // furthest stage
    /* 0xE */ s8 m_objectivesDone;  // this level
    /* 0xF */ u8 field_0xF;  // back from a finished level
    /* 0x10 */ u8 field_0x10;  // enemies in the level (kill percentage)
    /* 0x11 */ u8 unk_0x11[0x3];
    /* 0x14 */ u8 m_secrets[4];  // 2 bits per secret
    /* 0x18 */ u8 m_medals[4];  // 1 bit per medal
    /* 0x1C */ StarLevelsInBits m_stars;
    /* 0x24 */ UnlockedLevelsInBits m_unlocked;
    // Audio options (1..7; sound mode).
    /* 0x28 */ u8 field_0x28;  // sound effects volume
    /* 0x29 */ s8 m_musicVolume;
    /* 0x2A */ s8 m_soundMode;
    /* 0x2B */ u8 unk_0x2B[0x1];
    /* 0x2C */ SLevelStats m_careerStats;
    /* 0x120 */ SLevelStats m_levelStats;
    // Options.
    /* 0x214 */ u8 m_invertLook;
    /* 0x215 */ u8 m_crosshair;  // easy difficulty turns it on
    /* 0x216 */ u8 m_rumble;
    /* 0x217 */ u8 m_cookGrenades;  // single player option
    /* 0x218 */ u8 field_0x218;  // SHELL_* flags
    /* 0x219 */ u8 unk_0x219[0x3];
    /* 0x21C */ u32 field_0x21C;  // language
    /* 0x220 */ char m_customButtons[4][0xD];
    /* 0x254 */ u8 field_0x254;
    /* 0x255 */ u8 unk_0x255[0x3];
    /* 0x258 */ SFinishedLevel m_finishedLevels[6][4];  // [mission - 1][stage - 1]
    /* 0x13F8 */ u32 field_0x13F8;  // career shots when the evaluation was last picked
    /* 0x13FC */ u8 m_moviesUnlocked[6][2];  // gallery movies by mission
    /* 0x1408 */ u8 m_extrasUnlocked[0x5];
    /* 0x140D */ u8 unk_0x140D[0x3];
    // Multiplayer game setup (0x1410..)
    /* 0x1410 */ s32 m_mpEnabled;
    /* 0x1414 */ s32 m_mpNumPlayers;
    /* 0x1418 */ s32 m_mpWeaponSet;
    /* 0x141C */ s32 m_mpMission;
    /* 0x1420 */ s32 m_mpStage;
    /* 0x1424 */ s32 m_mpTimeLimit;  // minutes
    /* 0x1428 */ s32 field_0x1428;  // score limit
    /* 0x142C */ s32 m_mpTimeLimitOn;
    /* 0x1430 */ s32 field_0x1430;  // score limit on
    /* 0x1434 */ s32 field_0x1434;  // friendly fire
    /* 0x1438 */ s32 m_mpMusicTrack;  // 0: random (1..8)
    /* 0x143C */ s32 field_0x143C[4];  // starting health by controller port
    /* 0x144C */ CShellPlayerConfig m_players[4];
    /* 0x14BC */ s32 m_mpControllerPort[4];  // -1 = slot unused
    /* 0x14CC */ MPPlayerStats m_mpStats[4];
    /* 0x18CC */ s32 m_activeWindows[4];  // multiplayer split-screen windows in use
    /* 0x18DC */ u32 m_crc;  // CRC-32 of 0x0..0x18DC (the save data ends here)
    /* 0x18E0 */ u8 unk_0x18E0[0x4];
    /* 0x18E4 */ const char* m_languageSuffix;
    /* 0x18E8 */ s32 m_languageSubtitleIndex;
    /* 0x18EC */ u8 field_0x18EC;  // back from a level (not the first boot)
    /* 0x18ED */ u8 unk_0x18ED[0x3];
    /* 0x18F0 */ IStudio m_studio;
    /* 0x1908 */ u8 m_shellOn;
    /* 0x1909 */ u8 unk_0x1909[0x3];
    /* 0x190C */ CMemoryCard* m_pMemoryCard;
    /* 0x1910 */ u8 m_saveSlot;  // the save game last loaded or saved
    /* 0x1911 */ u8 unk_0x1911[0x3];
    // Statistics laid out for the menu (GetLevelStats/GetCumulativeStats).
    /* 0x1914 */ u32 m_statsOut[0x15];
    /* 0x1968 */ u8 unk_0x1968[0x2C];
    /* 0x1994 */ f32 m_moviePlayTime;  // game time the attract movie starts
    /* 0x1998 */ u8 field_0x1998;
    /* 0x1999 */ u8 field_0x1999;
    /* 0x199A */ u8 field_0x199A;
    /* 0x199B */ u8 field_0x199B;
    /* 0x199C */ u8 unk_0x199C[0x6];
    /* 0x19A2 */ s8 field_0x19A2;
    /* 0x19A3 */ s8 field_0x19A3;
    /* 0x19A4 */ u8 field_0x19A4;
    /* 0x19A5 */ u8 field_0x19A5;
    /* 0x19A6 */ u8 field_0x19A6;  // prompts on
    /* 0x19A7 */ u8 field_0x19A7;  // a controller is missing (set each update)
    /* 0x19A8 */ u8 field_0x19A8;
    /* 0x19A9 */ u8 field_0x19A9;
    /* 0x19AA */ s8 field_0x19AA;  // journal mission whose pictures are loaded (-1 none)
    /* 0x19AB */ s8 field_0x19AB;  // journal stage
    /* 0x19AC */ s8 m_journalPictures[6][4];  // journal pictures per level
    /* 0x19C4 */ CUserMessageTable m_messages;
    /* 0x19D4 */ CSprite* m_pUnpluggedSprite;
    /* 0x19D8 */ CFont* m_pFont;

    void MoveStatsIntoProperAreaForIstudio();  // 0x800DE334
    u8 GetSKUMode();  // 0x800DE420
    void SetupAllPlayersButtonsForMultiplayerGame();  // 0x800DE428
    void PassMP_PrefHitLocationToIStudio(int, char*);  // 0x800DE4AC
    int PassMP_PrefTargetToIStudio(int, char*);  // 0x800DE5FC
    void PassMP_PrefWeaponNameToIStudio(int, char*);  // 0x800DE6B0
    void PassMP_StatsToIStudio(int, int*);  // 0x800DE904
    int Get_NumPlayers();  // 0x800DEA7C
    void Pass_ActiveWindowsToIStudio(int*);  // 0x800DEACC
    void Save_ActiveWindowsFromIStudio(int*);  // 0x800DEB60
    void SetActivePlayers(int*);  // 0x800DEBCC
    void PassMultiPlayerLevelSettingsToIStudio(int*);  // 0x800DEC48
    void SetupMultiPlayerLevelSettings(int*);  // 0x800DECA8
    int PassPlayersMultiPlayerControllerValue(int);  // 0x800DED1C
    void SetupPlayersMultiPlayerControllerValue(int, int);  // 0x800DED2C
    int PassPlayersModelAndTeamColorSettingForMultiplayer(int, int*);  // 0x800DED3C
    void SetupPlayersModelAndTeamColorSettingForMultiplayer(int, int*);  // 0x800DED88
    void PassPlayersMultiPlayerName(int, char*);  // 0x800DEDA4
    void SetupPlayersMultiPlayerName(int, char*);  // 0x800DEDD8
    int PassPlayersAdvancedMultiPlayerSettingToIStudio(int, int*);  // 0x800DEE14
    void SetupPlayersAdvancedMultiPlayerSettings(int, int*);  // 0x800DEE84
    int GetSoundModeValue();  // 0x800DEEC8
    void SetGlobalCheatStructure();  // 0x800DEF28
    int GetGalleryMoviesUnlocked(int* movies, int* extras);  // 0x800DF0EC
    void PlayShellSound(int);  // 0x800DF1CC
    void CompletedAnotherObjective();  // 0x800DF210
    void ChangeNewGameSignStatus();  // 0x800DF220
    int GetCrosshairStatus();  // 0x800DF238
    void InitControllerFromOptions();  // 0x800DF240
    int PassControllerOptionsSettingToIStudio(int*);  // 0x800DF2B4
    void SetupControllerOptionsSettings(int*);  // 0x800DF364
    void NullOutJournalSpritesFromLibrary();  // 0x800DF3C8
    int PassAudioSettingToIStudio(int*);  // 0x800DF530
    void ResetAudio();  // 0x800DF5C0
    void SetMusicValue(int);  // 0x800DF814
    void SetSoundFXValue(int);  // 0x800DF824
    void SetupAudioSettings(int*);  // 0x800DF834
    int PassSecretsToIStudio(int*);  // 0x800DFA18
    int PassMedalStatusToIStudio(int*);  // 0x800DFB3C
    int PassLevelsRatingsToIStudio(int*);  // 0x800DFB80
    int PassLevelsCompletedToIStudio(int*);  // 0x800DFBC4
    void ActivateCheatNamed_All();  // 0x800DFCA8
    bool CheckIfValidPassword(char*);  // 0x800DFFD8
    void PlayBriefingMovie(int);  // 0x800E0770
    void PlayCreditsSlideShow();  // 0x800E0E7C
    int Get_LanguageSubtitleIndex();  // 0x800E14F8
    const char* Get_LanguageSuffix();  // 0x800E1500
    void RetrieveShellControllerButtons(char*, int);  // 0x800E1508
    void CopyControllerButtons(char*, int);  // 0x800E15A4
    void GetNextLevelCheat();  // 0x800E1640
    void GetNextLevel();  // 0x800E17CC
    bool CheckIfMovieAndMedalNeedsToBeUnlocked();  // 0x800E1914
    bool CheckIfSpecialMedalsNeedToBeUnlocked();  // 0x800E1C1C
    bool CheckIfThisIsTheLastLevelForTheMission();  // 0x800E1E58
    void PlayRewardMovie(int);  // 0x800E1EAC
    void PlayGalleryMovie(int movie, int type, int credits, int extra);  // 0x800E1EB0
    int FadeTheVolume(int);  // 0x800E1F48
    int CheckDataCRC(unsigned char*, int, unsigned long) const;  // 0x800E2000
    void LoadData();  // 0x800E2338
    void SaveData();  // 0x800E23E8
    bool MemoryCardBootUpPortCheck(int);  // 0x800E270C
    void MemoryCardBootUpCheck();  // 0x800E3AAC
    void CorruptFileDialog(char*, char, char*, int, bool, int, const char*, const char*);  // 0x800E3D1C
    int MemoryCardFreeSpace(int);  // 0x800E421C
    int MemoryCardSavePrefsSize();  // 0x800E4298
    int MemoryCardSaveGameSize();  // 0x800E42C0
    bool MemoryCardAutoLoad(int);  // 0x800E42E8
    int MemoryCardLoadGame(int, unsigned int);  // 0x800E4738
    void MemoryCardGetCorruptFileName(char*, int);  // 0x800E4BB8
    int MemoryCardNumFilesFree(int);  // 0x800E4BE8
    bool MemoryCardIsAnyObjectInSlot(int);  // 0x800E4C0C
    int MemoryCardGetMaxBlocks(int);  // 0x800E4C30
    bool MemoryCardIsEncodingCorrect(int);  // 0x800E4C54
    bool MemoryCardIs8KSectorSize(int);  // 0x800E4C78
    int MemoryCardSaveGame(int, int, char*);  // 0x800E4C9C
    bool MemoryCardDeleteSaveGame(int, unsigned int);  // 0x800E5268
    int MemoryCardGetNextSaveGame(int, unsigned int*, unsigned int*, char*);  // 0x800E52F4
    void MemoryCardRewindSaveGameList(int);  // 0x800E5380
    bool MemoryCardArePrefsPresent(int);  // 0x800E53F4
    bool MemoryCardFormat(int);  // 0x800E5470
    int MemoryCardIsFormated(int);  // 0x800E54EC
    bool MemoryCardQueryPort(int);  // 0x800E5568
    const char* GetWeaponMessageForTotalStats();  // 0x800E558C
    const char* GetEvalMessageForTotalStats();  // 0x800E56D4
    const char* GetWeaponMessage();  // 0x800E57A0
    const char* GetEvalMessage();  // 0x800E58E8
    u32* GetCumulativeStats();  // 0x800E5A14
    u32* GetLevelStats();  // 0x800E5DC4
    void CalculateLevelStats();  // 0x800E6374
    void UpdateLevelTimer(CPlayerObject*, unsigned int);  // 0x800E6574
    void ResetLevelTimer();  // 0x800E6590
    bool ShouldIUsePlayersFinishedLevelData(int, int);  // 0x800E659C
    u32 GetPlayersFinishedLevelAmmo(int, int, EWeaponTypes);  // 0x800E65D4
    float GetPlayersFinishedLevelHealth(int, int);  // 0x800E65FC
    void SetPlayersFinishedLevelAmmo(int, int, EWeaponTypes, unsigned int);  // 0x800E661C
    void SetPlayersFinishedLevelHealth(int, int, float);  // 0x800E6644
    void SetPlayersFinishedLevelSuccessfully(int, int);  // 0x800E6664
    MPPlayerStats* GetMPPlayerStats(int);  // 0x800E6688
    int Get_MPPlayerScore(int);  // 0x800E669C
    // `killer` (player index) killed `victim`.
    void Add_MPPlayerKilled(int killer, int victim);  // 0x800E66AC
    void Add_MPShotLocation(int shooter, int victim, int count, EShotLocations);  // 0x800E6950
    void Add_MPShotFired(int, int);  // 0x800E69A0
    void ResetMPPlayersStats();  // 0x800E69DC
    void Set_shotLocation(unsigned int, EShotLocations, bool);  // 0x800E6A28
    void Set_enemiesKilled(unsigned int, bool);  // 0x800E6A50
    void Set_hitsTaken(unsigned int, bool);  // 0x800E6AF8
    void Set_hits(unsigned int, bool);  // 0x800E6B18
    void Set_shotsFired(unsigned int, bool);  // 0x800E6B38
    void ResetCurrentLevelStats();  // 0x800E6B58
    void GetSecretsStatus(int*);  // 0x800E6C84
    void SetIndividualSecretsStatus(int, int);  // 0x800E6D3C
    void GetMedalStatus(int*);  // 0x800E6DFC
    int Get_currentStage();  // 0x800E6EB4
    int Get_currentMission();  // 0x800E6EBC
    u8 Get_currentDifficulty();  // 0x800E6EC4
    u8 Get_currentControllerSetting();  // 0x800E6ECC
    void Set_currentStage(unsigned int);  // 0x800E6ED4
    void Set_currentMission(unsigned int);  // 0x800E6EDC
    void Set_currentDifficulty(unsigned char);  // 0x800E6EE4
    void Set_currentControllerSetting(unsigned char);  // 0x800E6F44
    void ShutdownShell();  // 0x800E6F4C
    void TurnShellOn();  // 0x800E6FE0
    void TurnShellOff();  // 0x800E7160
    bool bIsShellOn();  // 0x800E723C
    void ShellDraw(unsigned long);  // 0x800E7244
    void ShellUpdate(unsigned long);  // 0x800E7274
    void SetMoviePlaytime();  // 0x800E74F4
    void ShellInit(unsigned long);  // 0x800E752C
    ~CShellMenu();  // 0x800E79FC
    void ResetShell();  // 0x800E7A7C
    CShellMenu();  // 0x800E7DD8
    void GetLevelUnlockedStatus(int*);  // 0x800E82A8
    void GetAllLevelsStarStatus(int*);  // 0x800E8424

    // User message `index`, or NULL past the table.
    const char* Message(int index) {
        return index < m_messages.m_count ? m_messages.m_entries[index].m_string : NULL;
    }
};
// The saved part (SHELL_SAVE_SIZE bytes: the memory card and the loader block)
// is plain data with the same layout on both builds.
FILE_OFFSET_ASSERT(CShellMenu, m_careerStats, 0x2C);
FILE_OFFSET_ASSERT(CShellMenu, m_levelStats, 0x120);
FILE_OFFSET_ASSERT(CShellMenu, m_customButtons, 0x220);
FILE_OFFSET_ASSERT(CShellMenu, m_finishedLevels, 0x258);
FILE_OFFSET_ASSERT(CShellMenu, field_0x13F8, 0x13F8);
FILE_OFFSET_ASSERT(CShellMenu, m_mpEnabled, 0x1410);
FILE_OFFSET_ASSERT(CShellMenu, m_players, 0x144C);
FILE_OFFSET_ASSERT(CShellMenu, m_mpStats, 0x14CC);
FILE_OFFSET_ASSERT(CShellMenu, m_crc, 0x18DC);
OFFSET_ASSERT(CShellMenu, m_studio, 0x18F0);
OFFSET_ASSERT(CShellMenu, m_statsOut, 0x1914);
OFFSET_ASSERT(CShellMenu, m_moviePlayTime, 0x1994);
OFFSET_ASSERT(CShellMenu, m_messages, 0x19C4);
SIZE_ASSERT(CShellMenu, 0x19DC);

// Free functions
void SetUnlockedLevelStatus(UnlockedLevelsInBits*, unsigned int, unsigned int);  // 0x800E058C
bool MemoryCardSlotOkForStartup(CMemoryCard*, int, int, int, int*, bool*, int*, bool*, bool*, bool*, bool*, int*, bool*, bool*);  // 0x800E3FF0
void iDebugMsg(const char*, ...);  // 0x800E6524
// Raises the stars of level `id` (mission * 10 + stage) to `stars`.
void SetStarRatingStatus(StarLevelsInBits*, unsigned int id, unsigned int stars);  // 0x800E85A0

extern int g_NumPlayers;  // 0x8034E2D0
extern const char* emptyString;  // 0x8034E2D8
extern CSoundStream* pSoundStream;  // 0x8034ECA0
extern bool g_bIsRubberGrenadeCheatEnabled;  // 0x8034ECA4
extern bool g_bInMultiplayerMode;  // 0x8034ECA5
extern CShellMenu g_Shell;  // 0x80195090 (Moh2.cpp)

#endif
