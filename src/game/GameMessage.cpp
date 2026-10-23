#include "game/GameMessage.h"

#include <string.h>

#include "game/ConfigGC.h"
#include "game/Moh2.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "game/input.h"
#include "game/player.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/toilet.h"
#include "game/uistypes.h"

// One mission secret as the shell script passes it in.
struct GMSecret {
    int found;
    int count;
};

static inline u32 GMParam(UISParam_t* params, int i) { return params[i].value; }

static inline char* GMText(UISParam_t* params, int i) {
    return (ADDR_TO_PTR(UISString_t, params[i].value))->text;
}

// Handles a game message posted by the iSiren shell / pause screen scripts.
// `params` are the inputs; the answer (if any) goes to result[0].
void GameMessage(unsigned long message, unsigned short, unsigned short, unsigned long numParams,
                 UISParam_t* params, UISParam_t* result) {
    switch (message) {
    case 1: {
        DebugMsg("PAUSE_QUIT_GAME\n");
        g_Pause.field_0x95 = 1;
        int sound[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, 0x7FFF};
        AEMS_SendEvent(sound);
        g_uiGameState |= 1;
        g_uiGameState |= 8;
        result[0].value = 0;
        break;
    }
    case 2: {
        DebugMsg("PAUSE_CONTINUE_GAME\n");
        int sound[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, 0x7FFF};
        AEMS_SendEvent(sound);
        g_Pause.UnpauseGame();
        result[0].value = 0;
        break;
    }
    case 0:
        DebugMsg("GM_KILL_SHELL\n");
        if (g_Shell.bIsShellOn()) {
            g_Shell.TurnShellOff();
        }
        break;
    case 0x82:
        DebugMsg("GM_LOAD_MISSION\n");
        g_Shell.ChangeNewGameSignStatus();
        g_Shell.m_mpEnabled = 0;
        if (GMParam(params, 0) == 0) {
            g_Shell.Set_currentMission(1);
            g_Shell.Set_currentStage(1);
        } else {
            g_Shell.Set_currentMission(GMParam(params, 0));
            g_Shell.Set_currentStage(GMParam(params, 1));
        }
        if (!(g_Shell.field_0x218 & SHELL_DIFFICULTY_SET)) {
            g_Shell.field_0x218 |= SHELL_DIFFICULTY_SET;
        }
        if (!(g_Shell.field_0x218 & SHELL_CONTROLS_SET)) {
            g_Shell.field_0x218 |= SHELL_CONTROLS_SET;
        }
        if (!(g_Shell.field_0x218 & SHELL_NEWGAME)) {
            g_Shell.field_0x218 |= SHELL_NEWGAME;
        }
        g_FinalSinglePlayerController = GMParam(params, 2) - 1;
        result[0].value = 0;
        break;
    case 0x32:
        result[0].value = g_Shell.Get_currentControllerSetting();
        break;
    case 0x28: {
        u32 setting = GMParam(params, 0);
        g_ControllerConfig.m_config = setting;
        if (setting < 6) {
            g_ControllerConfig.SetUpDefaultConfig(setting);
            g_Shell.Set_currentControllerSetting(setting);
        }
        if (!(g_Shell.field_0x218 & SHELL_CONTROLS_SET) && g_Shell.field_0x19A8 != 0) {
            g_Shell.field_0x218 |= SHELL_CONTROLS_SET;
        }
        break;
    }
    case 0x1F4: {
        int event[2] = {0xB, 0};
        AEMS_SendEvent(event);
        break;
    }
    case 0x1FE: {
        int event[2] = {0xB, 1};
        AEMS_SendEvent(event);
        break;
    }
    case 0x208: {
        int event[2] = {0xB, 2};
        AEMS_SendEvent(event);
        break;
    }
    case 3:
        DebugMsg("DDAYINTRO_DONE\n");
        g_Pause.SetState((PauseState)6);
        result[0].value = 0;
        break;
    case 4:
        g_Pause.SetState((PauseState)4);
        DebugMsg("DDAYINTRO_START_ENGINE\n");
        result[0].value = 0;
        break;
    case 0x29A:
        DebugMsg("DDAYINTRO_FADE_OUT\n");
        g_Pause.SetState((PauseState)5);
        result[0].value = 0;
        break;
    case 0xD2:
        result[0].value = g_Shell.MemoryCardQueryPort(GMParam(params, 0) - 1);
        break;
    case 0xDC:
        result[0].value = g_Shell.MemoryCardIsFormated(GMParam(params, 0) - 1);
        DebugMsg("Memory Card slot %d Formatted Query: %d\n", GMParam(params, 0) - 1, result[0].value);
        break;
    case 0xE6: {
        int volume = g_Shell.FadeTheVolume(0);
        DebugMsg("Memory Card Format\n");
        result[0].value = g_Shell.MemoryCardFormat(GMParam(params, 0) - 1);
        g_Shell.FadeTheVolume(volume);
        break;
    }
    case 0xF0: {
        int port = 0;
        if (numParams == 1) {
            port = GMParam(params, 0) - 1;
        }
        DebugMsg("Memory Card Save Game Size\n");
        if (g_Shell.MemoryCardArePrefsPresent(port)) {
            result[0].value = g_Shell.MemoryCardSaveGameSize();
        } else {
            int prefsSize = g_Shell.MemoryCardSavePrefsSize();
            result[0].value = g_Shell.MemoryCardSaveGameSize() + prefsSize;
        }
        break;
    }
    case 0xFA:
        DebugMsg("Memory Card Free Space\n");
        result[0].value = g_Shell.MemoryCardFreeSpace(GMParam(params, 0) - 1);
        break;
    case 0x104:
        DebugMsg("Memory Card Rewind Save List\n");
        g_Shell.MemoryCardRewindSaveGameList(GMParam(params, 0) - 1);
        break;
    case 0x10E: {
        unsigned int info;
        DebugMsg("Memory Card Get Next Save Game %d %d\n", GMParam(params, 0), GMParam(params, 1));
        result[0].value = g_Shell.MemoryCardGetNextSaveGame(GMParam(params, 0) - 1,
                                                            ADDR_TO_PTR(unsigned int, GMParam(params, 1)), &info,
                                                            GMText(params, 2));
        (ADDR_TO_PTR(UISString_t, GMParam(params, 2)))->maxLength = strlen(GMText(params, 2));
        if (result[0].value == 1) {
            DebugMsg("Memory Card Get Next Save Game returning %s\n", GMText(params, 2));
        } else {
            DebugMsg("Memory Card Get Next Save Game returning no more games\n");
        }
        break;
    }
    case 0x118:
        DebugMsg("Memory Card Save Game %d %d %s\n", GMParam(params, 0), GMParam(params, 1),
                 GMText(params, 2));
        DisableReset = 1;
        result[0].value =
            g_Shell.MemoryCardSaveGame(GMParam(params, 0) - 1, GMParam(params, 1), GMText(params, 2));
        DisableReset = 0;
        break;
    case 0x11D:
        DebugMsg("Memory Card Load Game %d, %d\n", GMParam(params, 0) - 1, GMParam(params, 1));
        result[0].value = g_Shell.MemoryCardLoadGame(GMParam(params, 0) - 1, GMParam(params, 1));
        break;
    case 0x96:
        DebugMsg("Getting cumulative player stats for the level\n");
        if (g_Shell.field_0x19A9 == 0) {
            memcpy(ADDR_TO_PTR(void, GMParam(params, 0)), g_Shell.GetCumulativeStats(), 0x40);
            result[0].value = 0x10;
        }
        break;
    case 0xA0:
        DebugMsg("Getting per level player stats\n");
        if (g_Shell.field_0x19A9 == 0) {
            memcpy(ADDR_TO_PTR(void, GMParam(params, 0)), g_Shell.GetLevelStats(), 0x54);
        }
        result[0].value = 0x15;
        break;
    case 0x309:
        result[0].value = 1;
        break;
    case 0x190:
        g_Shell.field_0x199A = 1;
        g_Shell.field_0x199B = 0;
        g_Shell.SetMoviePlaytime();
        g_Shell.field_0x19A8 = 0;
        DebugMsg("ENTERING MAIN SCREEN: Resetting Movie Timer\n");
        break;
    case 0x19A:
        g_Shell.field_0x199A = 1;
        g_Shell.SetMoviePlaytime();
        DebugMsg("Resetting Movie Timer\n");
        break;
    case 0x1A4:
        g_Shell.field_0x199A = 0;
        break;
    case 0xAA:
        result[0].value = g_Shell.PassMedalStatusToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0xA5:
        DebugMsg("Getting Evaluation messages\n");
        strcpy(GMText(params, 0), g_Shell.GetWeaponMessage());
        strcpy(GMText(params, 1), g_Shell.GetEvalMessage());
        break;
    case 0xBE:
        result[0].value = g_Shell.PassLevelsCompletedToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0xC3:
        result[0].value = g_Shell.PassLevelsRatingsToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x15E:
        result[0].value = 1;
        break;
    case 0x140:
        g_Shell.PlayBriefingMovie(GMParam(params, 0));
        break;
    case 0x16D:
        result[0].value = g_Shell.Get_currentMission();
        break;
    case 0x168:
        result[0].value = 1;
        break;
    case 0x163:
        g_Shell.PlayRewardMovie(GMParam(params, 0));
        break;
    case 0x1E:
        result[0].value = g_Shell.PassSecretsToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x14: {
        GMSecret secrets[10];
        memcpy(secrets, ADDR_TO_PTR(void, GMParam(params, 0)), sizeof(secrets));
        for (int i = 0; i < 10; i++) {
            if (secrets[i].found == 0) {
                g_Shell.SetIndividualSecretsStatus(i + 1, 0);
            } else if (secrets[i].count > 0) {
                g_Shell.SetIndividualSecretsStatus(i + 1, 2);
            } else {
                g_Shell.SetIndividualSecretsStatus(i + 1, 1);
            }
        }
        break;
    }
    case 0x12C:
        result[0].value =
            g_Shell.GetGalleryMoviesUnlocked(ADDR_TO_PTR(int, GMParam(params, 0)), ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x122:
        g_Shell.PlayGalleryMovie(GMParam(params, 0), GMParam(params, 1), GMParam(params, 2),
                                 GMParam(params, 3));
        break;
    case 0x23:
        g_ControllerConfig.m_config = GMParam(params, 0);
        g_ControllerConfig.OnEnter();
        g_ControllerConfig.SetAppliedState(false);
        g_ControllerConfig.m_active = true;
        break;
    case 0x2D:
        if (g_ControllerConfig.GetAppliedState()) {
            result[0].value = 1;
            g_ControllerConfig.m_active = false;
            g_Shell.field_0x218 |= SHELL_CONTROLS_SET;
        } else {
            result[0].value = 0;
        }
        break;
    case 0x136: {
        // Retry the level last played.
        int mission = g_Shell.field_0xC;
        unsigned int stage = g_Shell.field_0xD;
        g_Shell.Set_currentMission(g_Shell.field_0xC);
        g_Shell.Set_currentStage(stage);
        memcpy(ADDR_TO_PTR(void, GMParam(params, 0)), &mission, 4);
        memcpy(ADDR_TO_PTR(void, GMParam(params, 1)), &stage, 4);
        result[0].value = 1;
        break;
    }
    case 0x1AE:
        result[0].value = g_Shell.field_0x254;
        break;
    case 0x154:
        g_Shell.ResetShell();
        result[0].value = 1;
        break;
    case 0x64:
        result[0].value = g_Shell.PassAudioSettingToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x5A:
        g_Shell.SetupAudioSettings(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x78:
        result[0].value = g_Shell.Get_currentDifficulty();
        break;
    case 0x6E:
        g_Shell.Set_currentDifficulty(GMParam(params, 0));
        if (!(g_Shell.field_0x218 & SHELL_DIFFICULTY_SET) && g_Shell.field_0x19A8 != 0) {
            g_Shell.field_0x218 |= SHELL_DIFFICULTY_SET;
        }
        break;
    case 0xA:
        result[0].value = g_Shell.CheckIfValidPassword(GMText(params, 0));
        break;
    case 0x9B:
        DebugMsg("Getting total player stat in the suitcase\n");
        strcpy(GMText(params, 0), g_Shell.GetWeaponMessageForTotalStats());
        strcpy(GMText(params, 1), g_Shell.GetEvalMessageForTotalStats());
        break;
    case 0x14A: {
        DebugMsg("Getting level to retry again\n");
        int mission = g_Shell.Get_currentMission();
        int stage = g_Shell.Get_currentStage();
        memcpy(ADDR_TO_PTR(void, GMParam(params, 0)), &mission, 4);
        memcpy(ADDR_TO_PTR(void, GMParam(params, 1)), &stage, 4);
        result[0].value = g_Shell.field_0xF != 0;
        break;
    }
    case 0x330:
        DebugMsg("Look at me, I'm being activated in the beginning of the Mission Log!\n");
        if (g_Shell.field_0x19A4 != 0) {
            DebugMsg("Look at me, I'm unloading the previous Mission Log!\n");
            g_Shell.NullOutJournalSpritesFromLibrary();
            TLT_UnLoadShellBriefingLogBigFile();
            g_Shell.field_0x19A4 = 0;
        }
        break;
    case 0x331: {
        u32 mission = GMParam(params, 0);
        u32 level = GMParam(params, 1);
        if (mission != 6) {
            g_Shell.field_0x19AA = mission;
            g_Shell.field_0x19AB = level;
            DebugMsg("Look at me, I'm trying to load a  Mission Log!\n");
            TLT_LoadShellBriefingLogBigFile(mission, level);
            g_Shell.field_0x19A4 = 1;
        }
        break;
    }
    case 5:
        g_Pause.field_0x94 = 1;
        break;
    case 6:
        g_Pause.field_0x94 = 0;
        break;
    case 0x50:
        result[0].value = g_Shell.PassControllerOptionsSettingToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x46:
        g_Shell.SetupControllerOptionsSettings(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x18A:
        result[0].value = g_inputMgr.CDeviceManager::IsReady(GMParam(params, 0) - 1);
        break;
    case 7:
        if (g_Shell.bIsShellOn()) {
            result[0].value = 1;
        } else {
            result[0].value = 1;
        }
        break;
    case 0x3C:
        result[0].value = (g_Shell.field_0x218 >> 7) & 1;
        break;
    case 0x41:
        g_Shell.field_0x218 =
            (g_Shell.field_0x218 & ~SHELL_SUBTITLES) | (GMParam(params, 0) != 0 ? SHELL_SUBTITLES : 0);
        break;
    case 0x222:
        g_Shell.SetSoundFXValue(GMParam(params, 0));
        g_Shell.ResetAudio();
        break;
    case 0x223:
        g_Shell.SetMusicValue(GMParam(params, 0));
        g_Shell.ResetAudio();
        break;
    case 0x1B3:
        result[0].value = (g_Shell.field_0x218 >> 5) & 1;
        break;
    case 0x1B5:
        result[0].value = (g_Shell.field_0x218 >> 6) & 1;
        break;
    case 0x212: {
        u32 id = GMParam(params, 0);
        if (g_Shell.bIsShellOn()) {
            g_Shell.PlayShellSound(id);
        } else if (g_Pause.bIsGamePaused()) {
            int sound[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, 0x7FFF};
            sound[2] = id;
            AEMS_SendEvent(sound);
        }
        break;
    }
    case 0x3CB:
        g_Shell.field_0x19A8 = 1;
        break;
    case 0x21C:
        switch (g_Shell.field_0x21C) {
        case 1:
            result[0].value = 1;
            break;
        case 2:
            result[0].value = 4;
            break;
        case 3:
            result[0].value = 5;
            break;
        case 4:
            result[0].value = 2;
            break;
        case 5:
            result[0].value = 3;
            break;
        default:
            result[0].value = 1;
            break;
        }
        break;
    case 0x25D:
        result[0].value = g_Shell.GetSKUMode();
        break;
    case 0xFC:
        result[0].value = g_Shell.MemoryCardGetMaxBlocks(GMParam(params, 0) - 1);
        break;
    case 0x119:
        if (g_Shell.MemoryCardDeleteSaveGame(GMParam(params, 0) - 1, GMParam(params, 1))) {
            result[0].value = 1;
        } else {
            result[0].value = 2;
        }
        break;
    case 0x174:
        result[0].value = g_Shell.MemoryCardIs8KSectorSize(GMParam(params, 0) - 1);
        break;
    case 0x176:
        result[0].value = g_Shell.MemoryCardIsEncodingCorrect(GMParam(params, 0) - 1);
        break;
    case 0x178:
        result[0].value = g_Shell.MemoryCardIsAnyObjectInSlot(GMParam(params, 0) - 1);
        break;
    case 0x17A: {
        int port = 0;
        if (numParams == 1) {
            port = GMParam(params, 0) - 1;
        }
        if (g_Shell.MemoryCardArePrefsPresent(port)) {
            result[0].value = 1;
        } else {
            result[0].value = 2;
        }
        break;
    }
    case 0x17C:
        result[0].value = g_Shell.MemoryCardNumFilesFree(GMParam(params, 0) - 1);
        break;
    case 0x25B: {
        UISString_t* name = ADDR_TO_PTR(UISString_t, GMParam(params, 0));
        g_Shell.MemoryCardGetCorruptFileName(name->text, GMParam(params, 1) - 1);
        name->maxLength = strlen(name->text);
        break;
    }
    case 0x16F:
        g_Shell.SetupPlayersAdvancedMultiPlayerSettings(GMParam(params, 0) - 1, ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x170:
        g_Shell.PassPlayersAdvancedMultiPlayerSettingToIStudio(GMParam(params, 0) - 1,
                                                               ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x17E:
        g_Shell.PassPlayersMultiPlayerName(GMParam(params, 0) - 1, GMText(params, 1));
        break;
    case 0x180:
        g_Shell.SetupPlayersMultiPlayerName(GMParam(params, 0) - 1, GMText(params, 1));
        break;
    case 0x182:
        g_Shell.PassPlayersModelAndTeamColorSettingForMultiplayer(GMParam(params, 0) - 1,
                                                                  ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x184:
        g_Shell.SetupPlayersModelAndTeamColorSettingForMultiplayer(GMParam(params, 0) - 1,
                                                                   ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x186:
        result[0].value = g_Shell.PassPlayersMultiPlayerControllerValue(GMParam(params, 0) - 1);
        break;
    case 0x188: {
        u32 player = GMParam(params, 0);
        u32 setting = GMParam(params, 1);
        g_Shell.SetupPlayersMultiPlayerControllerValue(player - 1, setting);
        g_ControllerConfig.m_config = setting;
        g_ControllerConfig.m_player = player - 1;
        g_ControllerConfig.SetupButtonsForMultiplayerGame();
        break;
    }
    case 0x18C:
        g_Shell.PassMultiPlayerLevelSettingsToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x18E:
        g_Shell.SetupMultiPlayerLevelSettings(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x1A6:
        g_Shell.m_mpMission = GMParam(params, 0);
        g_Shell.m_mpStage = GMParam(params, 1);
        g_Shell.m_mpEnabled = 1;
        break;
    case 0x1A2:
        g_Shell.Pass_ActiveWindowsToIStudio(ADDR_TO_PTR(int, GMParam(params, 0)));
        break;
    case 0x1A0: {
        int* active = ADDR_TO_PTR(int, GMParam(params, 0));
        g_Shell.SetActivePlayers(active);
        g_Shell.Save_ActiveWindowsFromIStudio(active);
        break;
    }
    case 0x192:
        g_Shell.PassMP_StatsToIStudio(GMParam(params, 0) - 1, ADDR_TO_PTR(int, GMParam(params, 1)));
        break;
    case 0x194:
        g_Shell.PassMP_PrefWeaponNameToIStudio(GMParam(params, 0) - 1, GMText(params, 1));
        break;
    case 0x196:
        g_Shell.PassMP_PrefTargetToIStudio(GMParam(params, 0) - 1, GMText(params, 1));
        result[0].value = 1;
        break;
    case 0x198:
        g_Shell.PassMP_PrefHitLocationToIStudio(GMParam(params, 0) - 1, GMText(params, 1));
        break;
    case 0x19E:
        DebugMsg("GM_MULTI_RESTART_GAME \n");
        g_Shell.ResetMPPlayersStats();
        g_Shell.m_mpEnabled = 1;
        g_Shell.Get_NumPlayers();
        if (g_Shell.bIsShellOn()) {
            g_Shell.TurnShellOff();
        }
        break;
    case 0x7C:
        if (g_Shell.m_mpEnabled != 0) {
            result[0].value = g_Shell.Get_NumPlayers();
        } else {
            result[0].value = 1;
        }
        break;
    case 0x4D2: {
        // Shell slot of the player that opened the pause screen.
        CPlayerObject* player = NULL;
        if (g_PauseScreenLockedController >= 0 && g_PauseScreenLockedController <= 4) {
            for (int i = 0; i < g_NumPlayers; i++) {
                player = g_scene.GetPlayer(i);
                if (player != NULL && g_PauseScreenLockedController == (int)player->m_controllerPort) {
                    break;
                }
            }
            result[0].value = player->field_0x768;
        } else {
            result[0].value = g_scene.GetPlayer(0)->field_0x768;
        }
        break;
    }
    case 0x1F5:
        g_Shell.field_0x199B = 1;
        break;
    case 0x1F7:
        g_Shell.field_0x199B = 0;
        break;
    default:
        DebugMsg("UNKNOWN GAME MESSAGE\n");
        result[0].value = 0;
        break;
    }
}
