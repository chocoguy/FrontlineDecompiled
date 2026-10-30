// The in-level game loop: loading a level (Gameloop_Init), restarting it,
// per-frame camera/script update and shutdown, plus the level settings
// (fog, clear color, default lights) from the level contents.
#include "game/mainloop.h"
#ifdef FRONTLINE_NATIVE
#include "native/ai.h"
#include "native/corpses.h"
#include "native/hooks.h"
#endif
#include "game/AIFilter.h"
#include "game/ConfigGC.h"
#include "game/Moh2.h"
#include "game/Pause.h"
#include "game/Screen.h"
#include "game/ShellMenu.h"
#include "game/Tank_object.h"
#include "game/UserInterface.h"
#include "game/animdb.h"
#include "game/animemot.h"
#include "game/anim.h"
#include "game/bsbifunc.h"
#include "game/bsmessage.h"
#include "game/bsutil.h"
#include "game/camera.h"
#include "game/color.h"
#include "game/compartment.h"
#include "game/debug_prof.h"
#include "game/destructor_queue.h"
#include "game/input.h"
#include "game/isexportdefs.h"
#include "game/light.h"
#include "game/particlesystem.h"
#include "game/player.h"
#include "game/propdat.h"
#include "game/publisher.h"
#include "game/pumhandler.h"
#include "game/real_bridge.h"
#include "game/scene.h"
#include "game/skybox.h"
#include "game/sound.h"
#include "game/soundschedule.h"
#include "game/staticobjfactory.h"
#include "game/toilet.h"
#include "game/vector.h"
#include "dolphin/os.h"
#include "math.h"
#include "stdio.h"

#define DEG_TO_RAD 0.017453292f

CWorldObject::SAcquireSet g_idSet = {{0, 1, 2, 3}};

CAnimObjectFactory g_AnimObjFactory;
CPlayerObject* g_pPlayers[4];
CBulletFactory g_BulletFactory;
CWeaponFactory g_WeaponFactory;
CBulletDecalManager g_BulletDecalManager;
char g_soundStreamFilename[0x100];
int g_soundStreamRequest = -1;

static void* g_pLevelEMT;
CWorldObject* g_pWorld;
CBulletFactory* g_pBulletFactory;
CWeaponFactory* g_pWeaponFactory;
int g_mpTimeLimit;
int g_bMPEndLevel;

// Level setting fields (TLT resource 0xD).
enum {
    LS_NEAR = 15,
    LS_FAR = 16,
    LS_FOG_COLOR = 18,
    LS_FOG_START = 21,
    LS_FOG_END = 22,
    LS_CLEAR_COLOR = 23,
    LS_AMBIENT_COLOR = 29,
    LS_AMBIENT_INTENSITY = 32,
    LS_LIGHT1 = 33,  // dir[3], color[3], intensity per directional light
    LS_LIGHT2 = 40,
    LS_LIGHT3 = 47,
    LS_SHELL_FLAG = 75,
};

static inline float SettingF(LevelFileContentsStruct_* settings, int field) {
    return *(float*)&settings->fields[field].value;
}

static inline int SettingI(LevelFileContentsStruct_* settings, int field) {
    return settings->fields[field].value;
}

static inline void SetColor(CColor& color, LevelFileContentsStruct_* settings, int field) {
    color.r = SettingI(settings, field);
    color.g = SettingI(settings, field + 1);
    color.b = SettingI(settings, field + 2);
}

// A directional default light: normalized direction, color, intensity.
static inline void SetDirectionalLight(int index, CVector3& dir, CColor& color,
                                       LevelFileContentsStruct_* settings, int field) {
    SetColor(color, settings, field + 3);
    float x = SettingF(settings, field);
    float y = SettingF(settings, field + 1);
    float z = SettingF(settings, field + 2);
    dir.x = x;
    dir.z = z;
    dir.y = y;
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float len = sqrtf(zz + (xx + yy));
    if (len != 0.0f) {
        float inv = 1.0f / len;
        dir.x = dir.x * inv;
        dir.y = dir.y * inv;
        dir.z = dir.z * inv;
    }
    CLight::SetDefaultLight((CLight::EDefaultLights)index, dir, color,
                            SettingF(settings, field + 6));
}

static inline void GetMissionAndStage(int& mission, int& stage) {
    if (g_bInMultiplayerMode) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
    } else {
        mission = g_Shell.Get_currentMission();
        stage = g_Shell.Get_currentStage();
    }
}

void LoadLevelSettings() {
    LevelFileContentsStruct_* settings =
        TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    float nearPlane = SettingF(settings, LS_NEAR);
    float farPlane = SettingF(settings, LS_FAR);
#ifdef FRONTLINE_NATIVE
    farPlane *= NativeDrawDistance();  // Rewritten (SetFog moves the fog with it)
#endif

    int mission;
    int stage;
    GetMissionAndStage(mission, stage);
    mission--;
    stage--;
    int index = stage + mission * 4;
    char* landmark;
    if (index + 0x1C4 < umTable.m_count) {
        landmark = umTable.m_entries[index + 0x1C4].m_string;
    } else {
        landmark = NULL;
    }
    char* time;
    if (index + 0x1DC < umTable.m_count) {
        time = umTable.m_entries[index + 0x1DC].m_string;
    } else {
        time = NULL;
    }
    SetLandmarkStampString(landmark);
    SetTimeStampString(time);

    g_Shell.field_0x10 = SettingI(settings, LS_SHELL_FLAG);
    for (int i = 0; i < g_NumPlayers; i++) {
        CCamera& camera = g_pPlayers[i]->m_camera;
        camera.m_near = nearPlane;
        camera.m_projectionValid = false;
        camera.m_frustumValid = false;
        camera.m_far = farPlane;
        camera.m_projectionValid = false;
        camera.m_frustumValid = false;
    }

    CColor color(0, 0, 0, 0x80);
    SetColor(color, settings, LS_FOG_COLOR);
    SetFog(color, SettingF(settings, LS_FOG_START), SettingF(settings, LS_FOG_END));
    SetColor(color, settings, LS_CLEAR_COLOR);
    g_screen.SetClearColor(color);

    // ambient (the original passes its direction uninitialized)
    CVector3 dir;
    SetColor(color, settings, LS_AMBIENT_COLOR);
    CLight::SetDefaultLight((CLight::EDefaultLights)0, dir, color,
                            SettingF(settings, LS_AMBIENT_INTENSITY));
    SetDirectionalLight(1, dir, color, settings, LS_LIGHT1);
    SetDirectionalLight(2, dir, color, settings, LS_LIGHT2);
    SetDirectionalLight(3, dir, color, settings, LS_LIGHT3);
}

static inline CPlayerObject* NewPlayer() {
    CPlayerObject* player = new CPlayerObject;
    return player;
}

// Loads and sets up the level, reporting progress to `progress`.
void Gameloop_Init(GameloopProgressFn progress, CSprite*, CSprite* sprite) {
    int mission;
    int stage;
    GetMissionAndStage(mission, stage);
    g_bInMultiplayerMode = g_Shell.m_mpEnabled != 0;
    if (g_bInMultiplayerMode) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
        g_Shell.m_mpMission = mission;
        g_Shell.m_mpStage = stage;
        g_NumPlayers = g_Shell.m_mpNumPlayers;
        g_levelWeaponSetID = g_Shell.m_mpWeaponSet;
    } else {
        g_NumPlayers = 1;
    }

    SoundClose();
    SoundInit((ESoundMode)g_Shell.GetSoundModeValue());
    progress(NULL, sprite, 0.1f);
    progress(NULL, sprite, 0.1f);
    g_Shell.SetGlobalCheatStructure();
#ifdef FRONTLINE_NATIVE
    NativeAILevelStart(g_Shell.Get_currentDifficulty(), g_bInMultiplayerMode);
    NativeCorpsesLevelStart(g_bInMultiplayerMode);
#endif
    if (g_bInMultiplayerMode) {
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
    }

    char path[0xB0];
    sprintf(path, "data/emt_%d_%d.emt", mission, stage);
    g_pLevelEMT = TLT_LoadFileNormal(path, NULL, 0x100);
    AnimFileRelocate(g_pLevelEMT, 0);
    g_AnimDB.AddFile((AnimFileFormat_t*)g_pLevelEMT, NULL);
    TLT_CloseFile(g_pLevelEMT);
    progress(NULL, sprite, 0.2f);
    progress(NULL, sprite, 0.2f);
    TLT_Init(mission, stage);
    progress(NULL, sprite, 0.3f);
    progress(NULL, sprite, 0.3f);
    LoadLevelSoundModules(mission, stage);
    progress(NULL, sprite, 0.35f);
    progress(NULL, sprite, 0.35f);
    g_pWorld = new CWorldObject;
    progress(NULL, sprite, 0.4f);
    progress(NULL, sprite, 0.4f);

    g_inputMgr.SetKeymap(0, g_config_keymap);
    if (g_Shell.m_mpEnabled == 0) {
        g_ControllerConfig.m_player = 0;
        g_ControllerConfig.SetupButtonsForGame();
    } else {
        g_Shell.SetupAllPlayersButtonsForMultiplayerGame();
    }
    NullifyScreenAndLibrary();
    PauseIsScreen();
    PauseIsLibrary();
    g_Pause.PauseInit();

    g_AnimObjFactory.SetLargestObjectSize(g_aigAIFilterGlobalObject.m_largestAIObjectSize);
    if (g_bInMultiplayerMode) {
        g_AnimObjFactory.Init(0x34, NULL);
    } else {
#ifdef FRONTLINE_NATIVE
        // With room for the bodies Rewritten keeps (native/corpses.h).
        g_AnimObjFactory.Init(0x1A + NativeCorpseSlots(), NULL);
#else
        g_AnimObjFactory.Init(0x1A, NULL);
#endif
    }
    CStaticObjectFactory::Init();
    g_BulletFactory.Init();
    g_WeaponFactory.Init();
    g_pWeaponFactory = &g_WeaponFactory;
    g_pBulletFactory = &g_BulletFactory;
    g_BulletDecalManager.Init__NotTheVirtualOne();

    if (!g_bInMultiplayerMode) {
        g_pPlayers[0] = NewPlayer();
        g_pPlayers[0]->Init();
        g_scene.AddPlayer(g_pPlayers[0]);
        g_pPlayers[0]->field_0x768 = 0;
        g_pPlayers[0]->m_camera.field_0x294 = 0;
        g_pPlayers[0]->m_ui.m_playerIndex = 0;
        g_pPlayers[0]->m_controllerPort = g_FinalSinglePlayerController;
        BSUtilInitScriptingSystem(false);
        FindPlayerStartTriggerAndCreatePlayer(false, 0);
    } else {
        BSUtilInitScriptingSystem(false);
        CPlayerObject** players = g_pPlayers;
        int numPlayers = 0;
        for (int slot = 0; slot < 4; slot++) {
            if (g_Shell.m_mpControllerPort[slot] == -1) {
                continue;
            }
            *players = NewPlayer();
            (*players)->Init();
            (*players)->m_playerIndex = numPlayers;
            if (g_NumPlayers < 3) {
                (*players)->field_0x768 = numPlayers;
                (*players)->m_camera.field_0x294 = numPlayers;
                (*players)->m_ui.m_playerIndex = numPlayers;
            } else {
                (*players)->field_0x768 = slot;
                (*players)->m_camera.field_0x294 = slot;
                (*players)->m_ui.m_playerIndex = numPlayers;
            }
            (*players)->m_controllerPort = g_Shell.m_mpControllerPort[slot];
            int port = g_Shell.m_mpControllerPort[slot];
            g_inputMgr.SetKeymap(port, (KEYMAP*)((u8*)g_config_keymap + port * 0x104));
            FindPlayerStartTriggerAndCreatePlayer(false, numPlayers);
            (*players)->InitMultiplayer((MPPlayerData*)&g_Shell.m_players[slot], false);
            g_scene.AddPlayer(*players);
            players++;
            numPlayers++;
        }
        BSSendMessage(0x2A6, NULL, NULL, NULL, NULL);
    }
    progress(NULL, sprite, 0.5f);
    progress(NULL, sprite, 0.5f);

#ifdef FRONTLINE_NATIVE
    float fov = DEG_TO_RAD * NORMAL_FOV;
#else
    float fov = DEG_TO_RAD * DEFAULT_FOV;
#endif
    float aspect = CCamera::ASPECT_4x3;
    for (int i = 0; i < g_NumPlayers; i++) {
        CCamera* camera = &g_pPlayers[i]->m_camera;
        camera->m_near = 0.4f;
        camera->m_projectionValid = false;
        camera->m_frustumValid = false;
        camera->m_far = 200.0f;
        camera->m_projectionValid = false;
        camera->m_frustumValid = false;
        camera->SetPerspective(fov, fov * aspect);
        g_scene.SetCamera(camera);
    }

    CSkyBox* skybox = CSkyBox::CreateInstance();
    g_scene.Add(*skybox);
    skybox->SetTextures();
    InitTimeStampStuff();
    SetXFilesTextOnscreenTime(15.0f);
    LoadLevelSettings();
    progress(NULL, sprite, 0.6f);
    progress(NULL, sprite, 0.6f);
    g_pWorld->Acquire(g_idSet, true);
    progress(NULL, sprite, 0.8f);
    progress(NULL, sprite, 0.8f);
    g_scene.SetWorld(g_pWorld);

    // stream in the compartments
    while (g_pWorld->m_requests.m_size > 0) {
        UpdateREAL();
        g_pWorld->BeginUpdate(1.0f);
        float done;
        if (g_pWorld->m_loadTotal != 0) {
            done = g_pWorld->m_loadProgress / (float)(g_pWorld->m_loadTotal * 2);
        } else {
            done = 0.0f;
        }
        progress(NULL, sprite, 0.2f * done + 0.8f);
    }
    progress(NULL, sprite, 1.0f);
    progress(NULL, sprite, 1.0f);

    if (g_bInMultiplayerMode) {
        if (g_Shell.m_mpTimeLimitOn != 0) {
            int vsyncs = g_screen.GetVSyncs();
            int seconds = (int)(60.0f * (float)g_Shell.m_mpTimeLimit);
            g_mpTimeLimit = (int)(59.94f * (float)seconds + (float)vsyncs);
        } else {
            g_mpTimeLimit = -1;
        }
        g_bMPEndLevel = 0;
    }
    CTankObject::VerifyAIDataSize();
    DebugProfTimerInitAll();
    DebugProfTimerRegister(0, "Total", -1);
    DebugProfTimerRegister(1, "Update", 0);
    DebugProfTimerRegister(2, "Draw", 0);
    DebugProfTimerRegister(3, "Sound", 0);
    DebugProfTimerRegister(4, "Debug", 0);
}

void Gameloop_Shutdown() {
    DebugProfTimerDumpAll();
    ResetTimeStampStuff();
    g_scene.Shutdown();
    CSkyBox::DestroyInstance();
    for (int i = 0; i < g_NumPlayers; i++) {
        if (g_pPlayers[i] != NULL) {
            g_pPlayers[i]->m_bsgo.Destroy();
            delete g_pPlayers[i];
            g_pPlayers[i] = NULL;
        }
    }
    BSUtilShutdownScriptingSystem();
#ifdef FRONTLINE_NATIVE
    NativeCorpsesReset();
#endif
    g_AnimObjFactory.Shutdown();
    g_BulletFactory.Shutdown();
    g_WeaponFactory.Shutdown();
    g_pWeaponFactory = NULL;
    g_pBulletFactory = NULL;
    g_SoundSchedule.Shutdown();
    UnloadLevelSoundModules();
    g_Pause.PauseShutdown();
    TLT_Shutdown();
}

// Per frame: scripts, then each player's camera follows the player's eye.
void Gameloop_Loop(float dt) {
    DebugProfTimerResume(4);
    DebugProfTimerPause(4);
    PROFILE_BEGIN(PROFBIN_SCRIPT);
    BSUtilUpdate(dt);
    PROFILE_END(PROFBIN_SCRIPT);

    for (int i = 0; i < g_NumPlayers; i++) {
        CPlayerObject* player = g_scene.GetPlayer(i);
        CCamera* camera = &player->m_camera;
        CMatrix eye;
        CMatrix tm;
        player->GetTMLocalToWorld(tm);
        eye.Multiply(player->m_eyeTransform, tm);
        camera->SetTMLocalToWorld(eye);
        camera->Orthonormalize();
        float fov = DEG_TO_RAD * player->m_fov;
        if (g_NumPlayers == 2) {
            camera->SetPerspective(fov, 0.5f * fov * CCamera::ASPECT_4x3);
        } else {
            camera->SetPerspective(fov, fov * CCamera::ASPECT_4x3);
        }
        g_scene.SetCamera(camera);
        camera->AttemptUpdate(dt);
        g_scene.SetCamera(camera);
#ifdef FRONTLINE_NATIVE
        if (g_NumPlayers == 1) {
            // The pitch is the camera's: it was set from the player as the
            // last update left it.
            int flags = (player->m_attached ? NATIVE_VIEW_ATTACHED : 0) | (player->m_onMG ? NATIVE_VIEW_ON_GUN : 0);
            if (!player->m_onMG && (!player->m_attached || player->m_attachedKeepWeapon)) {
                flags |= NATIVE_VIEW_LOOK;
            }
            float lookScale = tanf(0.5f * DEG_TO_RAD * player->m_fov) / tanf(0.5f * DEG_TO_RAD * NORMAL_FOV);
            NativePlayerView(flags, lookScale, player->m_pitch);
        }
#endif
    }
}

// Restarts the current level (single player) without reloading it.
void Gameloop_Restart() {
    SoundSetMasterVolume(0);
    g_SoundSchedule.Shutdown();
    MUSIC_Stop();
    AmbientTrack_Select(0);
    ResetOneShotBank();
    CPublisher::Reset();
    CDestructorQueue::Reset();
    CLight::ResetClass();
    g_scene.Reset();

    g_camera.field_0x8 = 0;
    g_camera.field_0x4 = 0;
    g_pPlayers[0]->m_camera.field_0x8 = 0;
    g_pPlayers[0]->m_camera.field_0x4 = 0;
    g_scene.SetCamera(&g_camera);
    CSkyBox* skybox = CSkyBox::GetInstance();
    skybox->m_sceneLinks[1] = 0;
    skybox->m_sceneLinks[0] = 0;
    g_scene.Add(*skybox);

    CCompartment::GlobalReset();
    for (int i = 0; i < 4; i++) {
        CCompartment* compartment = g_pWorld->GetCompartment(i);
        if (compartment != NULL) {
            compartment->Reset();
        }
    }
    CWorldObject::SAcquireSet ids = {{0, 1, 2, 3}};
    g_pWorld->Acquire(ids, true);

#ifdef FRONTLINE_NATIVE
    NativeCorpsesReset();
#endif
    g_AnimObjFactory.Reset();
    CStaticObjectFactory::Reset();
    CParticleSystem::Reset();
    g_BulletFactory.Reset();
    g_WeaponFactory.Reset();
    g_BulletDecalManager.Reset();
    g_pPlayers[0]->ReInit();
    g_pPlayers[0]->field_0x4 = 0;
    g_pPlayers[0]->field_0x8 = 0;
    g_scene.AddPlayer(g_pPlayers[0]);
    g_pPlayers[0]->m_bsgo.m_player = NULL;
    BSUtilInitScriptingSystem(true);
    FindPlayerStartTriggerAndCreatePlayer(true, 0);
    LoadLevelSettings();
    while (g_pWorld->m_requests.m_size > 0) {
        UpdateREAL();
        SoundSync();
        g_pWorld->BeginUpdate(1.0f);
    }
    g_pWorld->m_sceneLinks[1] = 0;
    g_pWorld->m_sceneLinks[0] = 0;
    g_scene.SetWorld(g_pWorld);
    MUSIC_Start(0, 500, 0);
    g_SoundSchedule.Init();
    SoundSetMasterVolume(0x7F);
    g_Pause.SetState((PauseState)0);
}
