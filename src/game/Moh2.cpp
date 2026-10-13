// main(): core start-up, the shell (front end) loop, then the level loop
// (GameLoop once per frame, restarting the level after a death) and the exit
// back to the stub loader. Also the disc-error / reset task that runs every
// frame, the game's core global objects, and the out-of-line definitions of
// the ISceneNode / IMovingSceneNode defaults.
#include "game/Moh2.h"
#include "game/AIFilter.h"
#include "game/CFadeIn.h"
#include "game/CFadeOut.h"
#include "game/CFog.h"
#include "game/Pause.h"
#include "game/Screen.h"
#include "game/ShellMenu.h"
#include "game/UserInterface.h"
#include "game/anim_obj.h"
#include "game/animintf.h"
#include "game/camera.h"
#include "game/color.h"
#include "game/compartment.h"
#include "game/debug_prof.h"
#include "game/decal.h"
#include "game/destructor_queue.h"
#include "game/dmesh.h"
#include "game/dmrender.h"
#include "game/draw_context.h"
#include "game/fader.h"
#include "game/font.h"
#include "game/gameconfig.h"
#include "game/input.h"
#include "game/isexportdefs.h"
#include "game/light.h"
#include "game/loadelf.h"
#include "game/mainloop.h"
#include "game/MathFun.h"
#include "game/matstack.h"
#include "game/matrix.h"
#include "game/motionblur.h"
#include "game/particlesystem.h"
#include "game/plane.h"
#include "game/player.h"
#include "game/player_weapon_object.h"
#include "game/projectilebullet.h"
#include "game/publisher.h"
#include "game/pumhandler.h"
#include "game/real_bridge.h"
#include "game/renderlist.h"
#include "game/scene.h"
#include "game/skybox.h"
#include "game/sound.h"
#include "game/sprite.h"
#include "game/staticmesh.h"
#include "game/gcSystem.h"
#include "game/system.h"
#include "game/toilet.h"
#include "game/trig.h"
#include "game/vector.h"
#include "game/viewport.h"
#include "ea/sys/ipad.h"
#include "ea/sys/systask.h"
#include "ea/sys/timer.h"
#include "movie/rcmp_main.h"
#include <dolphin/dvd.h>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#include <dolphin/pad.h>
#include <dolphin/vi.h>
#include "stdio.h"

u8 InternalFont[0x62B0] __attribute__((aligned(32))) = {
#include "game/Moh2_InternalFont.inc"
};

CScene g_scene;
CCamera g_camera;
CFog g_fog(CColor(0x80, 0x80, 0x80, 0x80), 80.0f, 100.0f);
CViewport g_viewport;
CViewport g_mpViewports[6] = {
    CViewport(0.0f, 0.0f, 1.0f, 0.5f, 0),
    CViewport(0.0f, 0.5f, 1.0f, 1.0f, 1),
    CViewport(0.0f, 0.0f, 0.5f, 0.5f, 0),
    CViewport(0.5f, 0.0f, 1.0f, 0.5f, 1),
    CViewport(0.0f, 0.5f, 0.5f, 1.0f, 2),
    CViewport(0.5f, 0.5f, 1.0f, 1.0f, 3),
};
CRenderList theRenderList(0x3000, 2);
Pause g_Pause;
CShellMenu g_Shell;
CFadeIn g_FadeIn(CColor(0, 0, 0, 0x80), CColor(0, 0, 0, 0), 30.0f);
GameConfig g_config;
CCheats g_cheats;
CScreenFieldMode g_screen((EScreenWidth)640, (EFrameFormat)2, (EDepthFormat)0);
static CFadeOut multiPlayerEndGameFadeOut(CColor(0, 0, 0, 0), CColor(0, 0, 0, 0x80), 180.0f);

float g_DefaultMotionBlur = 0.2f;
float g_SlowMOHMotionBlur = 0.7f;
static float g_frameDel = 1.0f;  // frame time in 60Hz frames, from CScreen::Wait
static bool g_bBeginMPEndGameFadeOut = true;
static float g_MPShowScoreTimer = 360.0f;  // frames the final scores show before the fade-out
u8* pInternalFont = InternalFont;

u32 g_uiGameState;
static int g_frameNum;
static int g_flipWait;
static int g_listDraw;
static int g_listWait;
static int g_mainTime;
int g_updateTime;
static int g_drawTime;
static int g_prevMainTime;
static int g_prevListDraw;
static int g_prevUpdateTime;
static int g_prevDrawTime;

CFont* errorFont;
int FontLoaded;
int resethappend;
int DisableReset;
int DisableResetEntirley;
int QueueReset;

static void InitCORE(int);
static void RestartGame();
static void DrawStatusScreen(CSprite*, CSprite*, float);
static void LoadTheGame();
static void GameLoop();
static void EndErrorText();

static CMatrix IdentityMatrix() {
    CMatrix m;
    m.Ident();
    return m;
}

// The frame time, slowed tenfold by the slow-motion cheat.
static inline float GetFrameDelta() {
    float dt = g_frameDel;
    if (g_cheats.field_0x4 > 0.0f) {
        dt *= 0.1f;
    }
    return dt;
}

static void InitCORE(int arg) {
    SysInitParms_t parms = {1, 0, 1, 0, 0};
    SysInit(&parms);
    SysLoadAllModules(false, arg);
    CScreen::Init();
    InitREAL(arg);
    MathLLAngleInit();
    MathFunSRandom(OSGetTick());
    CMatrixStack::InitClass();
    CDestructorQueue::Init(100);
    CPublisher::Init(100);
    g_inputMgr.Init();
    g_inputMgr.SetKeymap(0, g_config_keymap);
    SoundInit((ESoundMode)g_Shell.GetSoundModeValue());
    g_screen.SetCurrent();
    g_screen.SetClearColor(CColor(0, 0, 0, 0x80));
    AnimSystem_Init();
    DMRenderInit();
    CCompartment::InitClass(&theRenderList, &g_screen);
    CStaticMesh::InitClass(&theRenderList, &g_screen);
    CSprite::InitClass(&theRenderList, &g_screen);
    CAnimObject::InitClass(&theRenderList, &g_screen);
    CDMesh::InitClass(&theRenderList);
    CPlayerObject::InitClass(&g_screen, &g_camera);
    CSkyBox::InitClass(&theRenderList, &g_screen);
    CProjectileBullet::InitClass(&theRenderList, &g_screen);
    CPlayerWeaponObject::InitClass();
    CParticleSystem::InitClass(&theRenderList, &g_screen, &g_scene);
    CLight::InitClass(&g_scene);
    CFont::InitClass(&theRenderList, &g_screen);
    CBulletDecalManager::InitClass(&theRenderList, &g_screen);
    CMotionBlur::Init(&theRenderList);
    CFader::Init(&theRenderList);
    DebugStats_Init(&g_screen, &theRenderList);
}

// Draws the render list and waits for the GPU to finish it.
void WaitRenderList() {
    PROFILE_BEGIN(PROFBIN_LIST_DRAW);
    g_prevListDraw = g_listDraw;
    g_listDraw = 0;
    g_listDraw -= OSGetTick();
    theRenderList.Draw();
    g_listDraw += OSGetTick();
    PROFILE_END(PROFBIN_LIST_DRAW);

    PROFILE_BEGIN(PROFBIN_GPU_WAIT);
    PROFILE_BEGIN(PROFBIN_LIST_WAIT);
    g_listWait = 0;
    g_listWait -= OSGetTick();
    theRenderList.Wait();
    g_listWait += OSGetTick();
    PROFILE_END(PROFBIN_GPU_WAIT);
    PROFILE_END(PROFBIN_LIST_WAIT);
}

// Flips the screen and waits for the retrace; the frames waited become the
// next frame's time step.
void WaitScreen() {
    PROFILE_BEGIN(PROFBIN_GPU_WAIT);
    PROFILE_BEGIN(PROFBIN_FLIP_WAIT);
    g_flipWait = 0;
    g_flipWait -= OSGetTick();
    g_screen.Flip();
    g_frameDel = g_screen.Wait();
    g_flipWait += OSGetTick();
    PROFILE_END(PROFBIN_GPU_WAIT);
    PROFILE_END(PROFBIN_FLIP_WAIT);
}

// Starts building the next frame's render list.
void SendRenderList() {
    PROFILE_BEGIN(PROFBIN_LIST_KICK);
    theRenderList.Kick();
    PROFILE_END(PROFBIN_LIST_KICK);
}

// Leaves the game by booting the stub loader, telling it why and where.
static void RestartGame() {
    WaitRenderList();
    WaitScreen();
    GXDrawDone();
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();

    const char* stub = "Moh2StubRelGC.elf";
    LoadElf* elf = LoadElf::Get();
    bool levelDone = g_Shell.field_0xF != 0;
    u8 pauseFlag = g_Pause.field_0x95;
    bool lastLevel = g_Shell.CheckIfThisIsTheLastLevelForTheMission();
    int reason;
    if (levelDone) {
        if (lastLevel) {
            reason = 4;
        } else {
            reason = 2;
        }
    } else if (pauseFlag) {
        reason = 3;
    } else {
        reason = 1;
    }
    elf->m_reason = reason;
    int stage = g_Shell.Get_currentStage();
    elf->m_mission = g_Shell.Get_currentMission();
    elf->m_stage = stage;
    elf->RunModule(stub);
}

// The loading screen: the mission picture full screen and the progress bar
// (`progress` 0..1) drawn as one frame.
static void DrawStatusScreen(CSprite* background, CSprite* bar, float progress) {
    float width = g_screen.GetWidth();
    float height = g_screen.GetHeight();
    if (background != NULL) {
        _spritepolyvert quad[4] = {
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {1.0f, 0.0f, width, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {1.0f, 1.0f, width, height, 0.0f, {0x80, 0x80, 0x80, 0x80}},
            {0.0f, 1.0f, 0.0f, height, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        };
        background->DrawPoly(quad, 4, CSprite::POLY_TRIANGLE_FAN);
    }

    float left = 163.0f * g_screen.m_widthScale;
    float top = 396.0f * g_screen.m_heightScale;
    float bottom = 16.0f * g_screen.m_heightScale + top;
    float right = progress * (312.0f * g_screen.m_widthScale) + left;
    _spritepolyvert barQuad[4] = {
        {0.0f, 0.0f, left, top, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {progress, 0.0f, right, top, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {progress, 1.0f, right, bottom, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {0.0f, 1.0f, left, bottom, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    };
    bar->DrawPoly(barQuad, 4, CSprite::POLY_TRIANGLE_FAN);

    WaitRenderList();
    WaitScreen();
    SendRenderList();
    g_inputMgr.Update(1.0f);
    MohDVDErrorTask(0, 0);
}

// Leaves the shell and loads the selected level behind the loading screen.
static void LoadTheGame() {
    char path[64];
    int mission;
    int stage;
    if (g_Shell.m_mpEnabled) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
    } else {
        mission = g_Shell.Get_currentMission();
        stage = g_Shell.Get_currentStage();
    }
    sprintf(path, "data/loading/load%d_%d.gsh", mission, stage);
    CSprite background(path, (CSprite::ESpriteLoadType)1, (ERenderPriority)0xA);
    CSprite loadBar("data/loading/loadbar.gsh", (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6F);

    g_screen.SetClear(false);
    DrawStatusScreen(&background, &loadBar, 0.0f);
    DrawStatusScreen(&background, &loadBar, 0.0f);
    MohDVDErrorTask(0, 0);
    background.Unload();

    g_Shell.ShutdownShell();
    if (g_Shell.field_0x19A4) {
        TLT_UnLoadShellBriefingLogBigFile();
    }
    g_Shell.ResetCurrentLevelStats();
    g_Shell.ResetLevelTimer();
    g_Shell.field_0xF = 0;
    g_Shell.InitControllerFromOptions();
    Gameloop_Init(DrawStatusScreen, NULL, &loadBar);

    WaitRenderList();
    SendRenderList();
}

// One frame of a level: input, the multiplayer clock, game update, HUD,
// pause menu and drawing (one viewport per player in multiplayer).
static void GameLoop() {
    float dt = GetFrameDelta();
    g_frameNum++;
    g_prevMainTime = g_mainTime;
    g_mainTime = 0;
    g_mainTime -= OSGetTick();
    PROFILE_BEGIN(PROFBIN_FRAME);
    PROFILE_BEGIN(PROFBIN_MAIN);
    DebugProfTimerStart(0);
    DebugProfTimerStart(1);
    DebugProfTimerStart(2);
    DebugProfTimerStart(3);
    DebugProfTimerStart(4);
    DebugProfTimerPause(1);
    DebugProfTimerPause(2);
    DebugProfTimerPause(3);
    DebugProfTimerPause(4);
    DebugProfTimerResume(1);

    g_inputMgr.Update(dt);
    if (g_bInMultiplayerMode) {
        CMotionBlur::SetAmount(0.0f);
    } else {
        CMotionBlur::SetAmount(g_DefaultMotionBlur);
        if (g_cheats.field_0x4 > 0.0f) {
            CMotionBlur::SetAmount(g_SlowMOHMotionBlur);
        }
    }

    int vsyncs = g_screen.GetVSyncs();
    static int lastVSync = vsyncs;
    if (!g_Pause.bIsGamePaused()) {
        int pauseState = g_Pause.field_0x88;
        if (pauseState >= 5 || pauseState == 0) {
            if (g_bInMultiplayerMode && g_Shell.m_mpTimeLimitOn) {
                float left = g_mpTimeLimit - g_screen.GetVSyncs();
                if (left <= 0.0f) {
                    g_bMPEndLevel = 1;
                } else {
                    float minutes = left / 59.94f / 60.0f;
                    static float lastTimeRemaining = minutes;
                    float warning = 0.0f;
                    if ((lastTimeRemaining > 5.0f && minutes <= 5.0f) ||
                        (lastTimeRemaining > 1.0f && minutes <= 1.0f)) {
                        warning = 5.0f;
                    }
                    for (int i = 0; i < g_NumPlayers; i++) {
                        CPlayerObject* player = g_scene.GetPlayer(i);
                        player->m_ui.m_mpTimeRemaining = minutes;
                        if (player->m_ui.m_mpTimeWarning <= 0.0f) {
                            player->m_ui.m_mpTimeWarning = warning;
                        }
                    }
                    lastTimeRemaining = minutes;
                }
            }
            CDestructorQueue::Execute();
            Gameloop_Loop(dt);
            if (g_inputMgr.GetValue(0, 0x88)) {
                CScreen::Capture();
            }
            g_Shell.UpdateLevelTimer(g_scene.GetPlayer(0), dt);
            g_aigAIFilterGlobalObject.UpdateFromGlobals(dt);
            g_prevUpdateTime = g_updateTime;
            g_updateTime = 0;
            g_scene.Update(dt);
            g_aigAIFilterGlobalObject.UpdateToGlobals();
            DebugProfTimerResume(2);
            if (!g_unplugged && !g_bInMultiplayerMode) {
                g_scene.GetPlayer(0)->m_ui.Draw(dt);
                UpdateXFilesText(dt);
            }
            DebugProfTimerPause(2);
        }
    } else {
        g_prevUpdateTime = g_updateTime;
        g_updateTime = 0;
        if (g_scene.GetPlayer(0)->m_ui.m_letterbox && !g_unplugged) {
            g_scene.GetPlayer(0)->m_ui.Draw(dt);
        }
        // The multiplayer clock stops while paused.
        if (g_bInMultiplayerMode && g_Shell.m_mpTimeLimitOn) {
            g_mpTimeLimit += vsyncs - lastVSync;
        }
    }
    if (g_bInMultiplayerMode) {
        lastVSync = vsyncs;
    }
    DebugProfTimerPause(1);

    g_pPopUpMessageHandler[0]->m_dt = dt;
    g_pPopUpMessageHandler[1]->m_dt = dt;
    g_pPopUpMessageHandler[2]->m_dt = dt;
    g_pPopUpMessageHandler[3]->m_dt = dt;
    UpdateSubtitles(dt);
    unsigned long frames = dt;
    g_Pause.PauseUpdate(frames);
    if (!g_bInMultiplayerMode) {
        g_Pause.PauseDraw(frames);
    }

    PROFILE_BEGIN(PROFBIN_DRAW);
    DebugProfTimerResume(2);
    g_prevDrawTime = g_drawTime;
    g_drawTime = 0;
    g_drawTime -= OSGetTick();
    if (g_bInMultiplayerMode) {
        // Two players split the screen in halves, three or four in quarters
        // (indexed by controller port).
        int base = g_NumPlayers > 2 ? 2 : 0;
        int slot = 0;
        for (int i = 0; i < g_NumPlayers; i++) {
            while (g_Shell.m_mpControllerPort[slot++] == -1) {
            }
            if (g_NumPlayers < 3) {
                g_viewport = g_mpViewports[base + i];
            } else {
                g_viewport = g_mpViewports[base + g_Shell.m_mpControllerPort[slot - 1]];
            }
            g_viewport.SetViewport(g_screen);
            g_scene.SetCamera(&g_scene.GetPlayer(i)->m_camera);
            g_scene.Draw(g_viewport, g_screen, theRenderList, g_fog);
            g_scene.GetPlayer(i)->m_ui.Draw(dt);
            theRenderList.Restart();
        }
        // With three players the unused quarter shows the scores.
        if (g_NumPlayers == 3 && !g_bMPEndLevel) {
            int n = 0;
            while (g_Shell.m_mpControllerPort[n++] != -1) {
            }
            g_viewport = g_mpViewports[base + n - 1];
            g_viewport.SetViewport(g_screen);
            g_scene.GetPlayer(2)->m_ui.DrawFragCount(true);
            theRenderList.Restart();
        }
        g_viewport.SetRect(0.0f, 0.0f, 1.0f, 1.0f);
        g_viewport.SetViewport(g_screen);
        g_Pause.PauseDraw(frames);
    } else {
        g_scene.Draw(g_viewport, g_screen, theRenderList, g_fog);
    }
    g_drawTime += OSGetTick();
    DebugProfTimerPause(2);
    PROFILE_END(PROFBIN_DRAW);

    DebugProfTimerResume(4);
    DebugStats_Draw(g_prevListDraw, g_prevMainTime - g_prevUpdateTime - g_prevDrawTime, g_prevUpdateTime,
                    g_prevDrawTime, g_listWait > 0 ? g_listWait : 0);
    DebugProfTimerPause(4);

    // Debug drawing (compiled out) used this context; constructing it still
    // loads the camera's projection and view into GX.
    DebugProfTimerResume(2);
    CDrawContext ctx(g_camera.m_worldToCamera, g_camera.m_projection, g_viewport.GetTMClipToView(),
                     g_screen.GetTMViewToScreen(), 0);
    static CMatrix ident = IdentityMatrix();
    ctx.m_pWorld = &ident;
    DebugProfTimerPause(2);
    DebugProfTimerResume(4);
    DebugProfTimerPause(4);
    DebugProfTimerResume(4);
    DebugProfTimerPause(4);

    DebugProfTimerResume(1);
    UpdateREAL();
    DebugProfTimerPause(1);
    DebugProfTimerResume(3);
    SoundSync();
    DebugProfTimerPause(3);

    // Multiplayer game over: show the scores, then fade out (with the sound)
    // and leave the level.
    if (g_bInMultiplayerMode && g_bMPEndLevel) {
        g_Pause.SetState((PauseState)7);
        g_scene.GetPlayer(0)->m_ui.DrawFragCount(false);
        if (g_MPShowScoreTimer > 0.0f) {
            CColor color(0, 0, 0, 0x10);
            g_MPShowScoreTimer -= dt;
            CFader::SetColor(color);
            multiPlayerEndGameFadeOut.m_from = color;
        } else if (g_bBeginMPEndGameFadeOut) {
            CFader::SetControl(&multiPlayerEndGameFadeOut);
            g_bBeginMPEndGameFadeOut = false;
        }
        if (multiPlayerEndGameFadeOut.IsFading()) {
            SoundSetMasterVolume(
                127.0f * (1.0f - multiPlayerEndGameFadeOut.m_time / multiPlayerEndGameFadeOut.m_duration));
        } else {
            g_Shell.field_0xF = 0;
            g_uiGameState |= GAMESTATE_LEVEL_DONE;
        }
    }

    g_mainTime += OSGetTick();
    PROFILE_END(PROFBIN_MAIN);
    DebugProfTimerResume(1);
    DebugProfTimerResume(2);
    DebugProfTimerResume(3);
    DebugProfTimerResume(4);
    DebugProfTimerStop(1);
    DebugProfTimerStop(2);
    DebugProfTimerStop(3);
    DebugProfTimerStop(4);
    DebugProfTimerStop(0);

    static u32 uiLoopCount = 0;
    if (!(++uiLoopCount & 0x3FFF)) {
        DebugProfTimerDumpAll();
    }
}

// Debug output; compiled out of the retail game.
void DebugMsg(const char*, ...) {}

int main() {
    char path[64];

    InitCORE(0);
    SYNCTASK_add(MohDVDErrorTask, 0, 0);
    NullifyScreenAndLibrary();
    ShellIsScreen();
    ShellIsLibrary();
    float dt = GetFrameDelta();
    g_Shell.LoadData();
    g_Shell.ShellInit(dt);
    g_inputMgr.SetKeymap(0, g_config_keymap);
    if (!g_Shell.field_0x18EC) {
        RCMP_Initialize();
        sprintf(path, "data\\Movies\\ealogo%s.mpc", g_Shell.Get_LanguageSuffix());
        RCMP_PlayMovie(path, false);
        sprintf(path, "data\\Movies\\title%s.mpc", g_Shell.Get_LanguageSuffix());
        RCMP_PlayMovie(path, false);
        sprintf(path, "data\\Movies\\intro%s.mpc", g_Shell.Get_LanguageSuffix());
        RCMP_PlayMovie(path, true);
        RCMP_Shutdown();
    }
    g_Shell.TurnShellOn();
    g_inputMgr.SetKeymap(0, g_config_keymap);

    // The front end, until a level is chosen.
    g_uiGameState |= GAMESTATE_IN_SHELL;
    for (;;) {
        dt = GetFrameDelta();
        g_frameNum++;
        g_prevMainTime = g_mainTime;
        g_mainTime = 0;
        g_mainTime -= OSGetTick();
        g_inputMgr.Update(dt);
        unsigned long frames = dt;
        g_Shell.ShellUpdate(frames);
        g_Shell.ShellDraw(frames);
        UpdateREAL();
        SoundSync();
        g_mainTime += OSGetTick();
        if (!g_Shell.bIsShellOn()) {
            break;
        }
        WaitRenderList();
        WaitScreen();
        SendRenderList();
    }
    g_uiGameState &= ~GAMESTATE_IN_SHELL;

    LoadTheGame();
    MUSIC_Start(0, 500, 0);
    AmbientTrack_Start();
    if (g_Shell.Get_currentMission() == 6 && g_Shell.Get_currentStage() == 1 && !g_bInMultiplayerMode) {
        g_Pause.SetState((PauseState)3);
    }

    // The level, restarted after each death until it is finished or quit.
    for (;;) {
        g_scene.GetPlayer(0)->m_ui.m_letterbox = 0;
        g_Shell.ResetCurrentLevelStats();
        CFader::SetControl(&g_FadeIn);
        if (!g_inputMgr.VerifyAnalog()) {
            g_inputMgr.Reset();
            for (int i = 0; i < 100000; i++) {
                g_inputMgr.Update(0.0f);
                if (g_inputMgr.IsReady()) {
                    break;
                }
            }
        }

        g_uiGameState |= GAMESTATE_IN_LEVEL;
        while (!(g_uiGameState & GAMESTATE_LEVEL_DONE)) {
            GameLoop();
            WaitRenderList();
            WaitScreen();
            SendRenderList();
            PROFILE_END(PROFBIN_FRAME);
            PROFILE_UPDATE();
        }
        g_uiGameState &= ~GAMESTATE_IN_LEVEL;
        g_inputMgr.ResetActuatorState(0);
        g_inputMgr.Update(1.0f);

        bool levelDone = g_Shell.field_0xF != 0;
        g_Shell.CalculateLevelStats();
        if (levelDone || (g_uiGameState & GAMESTATE_NO_RETRY) || g_bInMultiplayerMode) {
            break;
        }

        // Retry: black screen with a message for two frames, then restart.
        CFader::SetColor(CColor(0, 0, 0, 0));
        const char* fontFile = (const char*)TLT_LoadFileFromLevelBigFile("objFont.gfn", NULL);
        CFont* font = new CFont(fontFile, (ERenderPriority)0x6F);
        font->SetColor(CColor(0x5F, 0x5F, 0x5F, 0x80));
        font->SetScale(1.0f, 1.0f);
        font->DrawText(20.0f, 408.0f * g_screen.m_heightScale, GetUMString(0x2F4));
        WaitRenderList();
        WaitScreen();
        SendRenderList();
        font->DrawText(20.0f, 408.0f * g_screen.m_heightScale, GetUMString(0x2F4));
        WaitRenderList();
        WaitScreen();
        SendRenderList();
        delete font;
        g_Pause.Reset();
        Gameloop_Restart();
        g_uiGameState &= ~GAMESTATE_LEVEL_DONE;
    }

    if (g_scene.GetPlayer(0) != NULL && !g_bInMultiplayerMode) {
        g_scene.GetPlayer(0)->SetFinishedLevelData();
    }
    g_Pause.PauseDraw(GetFrameDelta());
    WaitRenderList();
    SendRenderList();

    g_Shell.SaveData();
    Gameloop_Shutdown();
    CPublisher::Shutdown();
    CDestructorQueue::Shutdown();
    g_inputMgr.Shutdown();
    SoundClose();
    AnimSystem_Shutdown();
    DMRenderShutdown();
    CMatrixStack::EndClass();
    ShutdownREAL();
    SysShutdown();
    DebugMsg("\nShutDown() Finished!!!!!\n\n");
    RestartGame();
    return 0;
}

int GetFrameCount() {
    return g_frameNum;
}

float GetGameTime() {
    return 0.016683351f * g_frameNum;
}

void SetFog(CColor& color, float start, float range) {
    g_fog.m_color = color;
    g_fog.m_start = start;
    g_fog.m_range = range;
}

static void EndErrorText() {
    WaitRenderList();
    WaitScreen();
    SendRenderList();
}

// One centred line of the disc-error screen (drawn twice, as the original does).
static inline void DrawErrorLine(float y, const char* text) {
    g_screen.SetClearColor(CColor(0, 0, 0, 0x80));
    g_screen.SetClear(true);
    errorFont->SetColor(CColor(0x50, 0x12, 0x12, 0x80));
    errorFont->DrawTextCentered(y * g_screen.m_heightScale, text);
    errorFont->DrawTextCentered(y * g_screen.m_heightScale, text);
}

// Runs every frame (a sync task) and from the loading screen: handles the
// reset button and the B+X+Start soft reset, and while the drive reports an
// error (cover open, wrong disc, ...) shows the error screen until it clears.
int MohDVDErrorTask(int, int) {
    int error = 0;
    static int softwareresetcheckstarted = 0;
    static int softwareresetstarttime = 0;
    if (!FontLoaded) {
        errorFont = new CFont((const char*)pInternalFont, (ERenderPriority)0x6F);
        FontLoaded = 1;
    }

    int status;
    do {
        if (OSGetResetButtonState() && resethappend < 2 && !DisableResetEntirley) {
            resethappend = 1;
        } else if (resethappend == 1) {
            resethappend = 2;
        } else if ((resethappend > 1 && resethappend < 60) || QueueReset) {
            if (DisableReset) {
                QueueReset = 1;
            } else {
                QueueReset = 0;
                resethappend++;
            }
        } else if (resethappend >= 60) {
            PADRecalibrate(0xF0000000);
            GXDrawDone();
            VISetBlack(TRUE);
            VIFlush();
            VIWaitForRetrace();
            OSResetSystem(1, 1, FALSE);
            resethappend = 0;
        } else {
            resethappend = 0;
        }

        // B+X+Start held on any pad for half a second also resets.
        if ((gPadStatus[0].button == 0x1600 || gPadStatus[1].button == 0x1600 ||
             gPadStatus[2].button == 0x1600 || gPadStatus[3].button == 0x1600) &&
            !DisableResetEntirley) {
            if (!softwareresetcheckstarted) {
                softwareresetstarttime = TIMER_gettick();
                softwareresetcheckstarted = 1;
            } else {
                float halfSecond = 0.5f * TIMER_getfrequency();
                if ((float)(TIMER_gettick() - softwareresetstarttime) > halfSecond && resethappend == 0) {
                    resethappend = 1;
                }
            }
        } else {
            softwareresetcheckstarted = 0;
            softwareresetstarttime = 0;
        }

        status = DVDGetDriveStatus();
        if (resethappend < 2) {
            switch (status) {
            case 5:  // cover open
                g_screen.SetClear(true);
                DrawErrorLine(195.0f, "The Disc Cover is open.");
                DrawErrorLine(225.0f, "If you want to continue the game, please close the Disc Cover.");
                EndErrorText();
                error = 1;
                break;
            case 6:  // wrong disc
                DrawErrorLine(180.0f, "This is not the Medal of Honor Frontline Game Disc.");
                DrawErrorLine(210.0f, "Please insert the Medal of Honor Frontline");
                DrawErrorLine(240.0f, "Game Disc.");
                EndErrorText();
                error = 1;
                break;
            case 4:  // no disc
                g_screen.SetClear(true);
                DrawErrorLine(180.0f, "Please insert the");
                DrawErrorLine(210.0f, "Medal of Honor Frontline");
                DrawErrorLine(240.0f, "Game Disc.");
                EndErrorText();
                error = 1;
                break;
            case 11:  // retry
                g_screen.SetClear(true);
                DrawErrorLine(170.0f, "The Game Disc could not be read.");
                DrawErrorLine(200.0f, "Please read the");
                DrawErrorLine(230.0f, "Nintendo GameCube\x99 Instruction Booklet");
                DrawErrorLine(260.0f, "for more information.");
                EndErrorText();
                error = 1;
                break;
            case -1:  // fatal
                error = 1;
                g_screen.SetClear(true);
                DrawErrorLine(170.0f, "An error has occurred.");
                DrawErrorLine(200.0f, "Turn off the power and refer to the");
                DrawErrorLine(230.0f, "Nintendo GameCube\x99 Instruction Booklet");
                DrawErrorLine(260.0f, "for further instructions.");
                EndErrorText();
                break;
            case 0:  // ready
                error = 0;
                DisableResetEntirley = 0;
                break;
            }
        }
        if (error || resethappend > 0) {
            g_inputMgr.ResetActuatorState(0);
            g_inputMgr.Update(1.0f);
        }
    } while (resethappend > 2 || error || status == -1);
    return 0;
}

// Out-of-line defaults of the scene-node interfaces (inline in the
// original's headers, so emitted weak here).

CFog::~CFog() {}

CPlane::~CPlane() {}

CPlane::CPlane() {}

IMovingSceneNode::~IMovingSceneNode() {}

ISceneNode::~ISceneNode() {}

IDestructible::~IDestructible() {}

void IMovingSceneNode::Reset() {}

void IMovingSceneNode::Halt() {}

void IMovingSceneNode::ApplyForceTo(CVector3, float) {}

void IMovingSceneNode::SetTMLocalToWorld(const CMatrix&) {}

void IMovingSceneNode::PreTransform(const CMatrix&) {}

void IMovingSceneNode::Transform(const CMatrix&) {}

void IMovingSceneNode::Move(CVector3) {}

void IMovingSceneNode::Rotate(CVector3, float) {}

void IMovingSceneNode::Pitch(float) {}

void IMovingSceneNode::Roll(float) {}

// Like the original, returns nothing (r3 is left holding `this`).
int IMovingSceneNode::Yaw(float) {}

void IMovingSceneNode::SetPosition(CVector3) {}

void IMovingSceneNode::SetBasis(CVector3, CVector3, CVector3) {}

void IMovingSceneNode::Orthonormalize() {}

CVector3 IMovingSceneNode::GetWorldLinearVelocity() const {
    return CVector3(0.0f, 0.0f, 0.0f);
}

void IMovingSceneNode::EnterLightVolume(BPDLightVolume*) {}

void IMovingSceneNode::ExitLightVolume(BPDLightVolume*) {}

BPDLightVolume* IMovingSceneNode::GetLightVolume() {
    return NULL;
}

const IMovingSceneNode* IMovingSceneNode::AsMovingNode() const {
    return this;
}

IMovingSceneNode* IMovingSceneNode::AsMovingNode() {
    return this;
}

// The three update steps return nothing, like the original (r3 is left
// holding `this`).
int ISceneNode::BeginUpdate(float) {}

void ISceneNode::UpdateAI(float) {}

void ISceneNode::CommitAI() {}

void ISceneNode::ConstrainVelocity() {}

int ISceneNode::AttemptUpdate(float) {}

void ISceneNode::OnCollision(const CCollision&) {}

int ISceneNode::CommitUpdate() {}

void ISceneNode::Draw(CDrawContext&) {}

const IVolume* ISceneNode::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return NULL;
}

const IVolume* ISceneNode::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return NULL;
}

void ISceneNode::GetTMLocalToWorld(CMatrix& out) const {
    out.Ident();
}

void ISceneNode::GetPosition(CVector3& out) const {
    out.z = 0.0f;
    out.y = 0.0f;
    out.x = 0.0f;
}

void ISceneNode::GetRightward(CVector3& out) const {
    out.x = 1.0f;
    out.y = 0.0f;
    out.z = 0.0f;
}

void ISceneNode::GetForward(CVector3& out) const {
    out.x = 0.0f;
    out.y = 1.0f;
    out.z = 0.0f;
}

void ISceneNode::GetUpward(CVector3& out) const {
    out.x = 0.0f;
    out.y = 0.0f;
    out.z = 1.0f;
}

int ISceneNode::IsVisible(CDrawContext&) const {
    return 0;
}

int ISceneNode::IsDrawEnabled() const {
    return 0;
}

int ISceneNode::GetCollisionId() const {
    return -1;
}

void ISceneNode::SetCollisionId(EClsnId) {}

BSObject* ISceneNode::GetScriptObject() const {
    return 0;
}

void ISceneNode::TriggerScriptEvent(int, void*, bool) {}

void ISceneNode::HandleBulletCollision(CBullet*, const CCollision&) {}

const CStaticObject* ISceneNode::AsStaticObject() const {
    return NULL;
}

CStaticObject* ISceneNode::AsStaticObject() {
    return NULL;
}

const CHierObject* ISceneNode::AsHierObject() const {
    return NULL;
}

CHierObject* ISceneNode::AsHierObject() {
    return NULL;
}

const CWorldObject* ISceneNode::AsWorldObject() const {
    return NULL;
}

CWorldObject* ISceneNode::AsWorldObject() {
    return NULL;
}

const CAnimObject* ISceneNode::AsAnimObject() const {
    return NULL;
}

CAnimObject* ISceneNode::AsAnimObject() {
    return NULL;
}

const CSoldierObject* ISceneNode::AsSoldierObject() const {
    return NULL;
}

CSoldierObject* ISceneNode::AsSoldierObject() {
    return NULL;
}

const CPlayerObject* ISceneNode::AsPlayerObject() const {
    return NULL;
}

CPlayerObject* ISceneNode::AsPlayerObject() {
    return NULL;
}

const CPlayerWeaponObject* ISceneNode::AsPlayerWeaponObject() const {
    return NULL;
}

CPlayerWeaponObject* ISceneNode::AsPlayerWeaponObject() {
    return NULL;
}

const CAnimatedPlayer* ISceneNode::AsAnimatedPlayerObject() const {
    return NULL;
}

CAnimatedPlayer* ISceneNode::AsAnimatedPlayerObject() {
    return NULL;
}

const CLight* ISceneNode::AsLight() const {
    return NULL;
}

CLight* ISceneNode::AsLight() {
    return NULL;
}

const CBullet* ISceneNode::AsBullet() const {
    return NULL;
}

CBullet* ISceneNode::AsBullet() {
    return NULL;
}

const CCollisionVolume* ISceneNode::AsCollisionVolume() const {
    return NULL;
}

CCollisionVolume* ISceneNode::AsCollisionVolume() {
    return NULL;
}

const CAIDoodad* ISceneNode::GetAIDoodad() const {
    return 0;
}

CAIDoodad* ISceneNode::GetAIDoodad() {
    return 0;
}

void ISceneNode::SetAttachedLight(CLight*, CVector3) {}

int ISceneNode::GetAttachedLight() const {
    return 0;
}

void IDestructible::Destroy() {}

const IMovingSceneNode* ISceneNode::AsMovingNode() const {
    return NULL;
}

IMovingSceneNode* ISceneNode::AsMovingNode() {
    return NULL;
}
